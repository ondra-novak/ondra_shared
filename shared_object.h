/*
 * shared.h
 *
 *  Created on: 17. 4. 2020
 *      Author: ondra
 */

#ifndef SRC_ONDRA_SHARED_SHARED_H_pokqwpo9djqwpoqp
#define SRC_ONDRA_SHARED_SHARED_H_pokqwpo9djqwpoqp

#include <utility>
#include <shared_mutex>
#include "refcnt.h"

namespace ondra_shared {

///SharedObject encapsulates an object (instance of a class) to allow to share object between the threads with proper synchronization
/**
 * The feature is similar to shared_ptr, but the object is protected by the locks. So you can share the instance by copying it
 * between variable, but to access the object, you need to lock it first.
 *
 * There are two types of locking. Standard lock() allows exclusive access to the object. Shared lock allows to acces for read only
 * operations (returns const reference). Both locked types are carried as smart pointers which unlocks them automatically as they
 * are destroyed. The object itself is release once the last reference is destroyed. There is also function to manual release
 *
 * @tparam K type/class to share
 * @tparam Traits allows you to modify behavour of the class depend on various traits of the type. There is two predefined traits.
 *    SharedObjectDefaultTraits and SharedObjectVirtualTraits. Default traits are default. Virtual traits are used along with
 *    function cast() or dynamicCast() which allows to change type of the object as standart static_cast and dynamic_cast while
 *    it still allows to lock the object shared or exclusive. Virtual traits uses virtual binding to access original object. Virtual binding
 *    is slightly slower and nested casts can slow access even more.
 */
template<typename K, typename Traits> class SharedObject;

template<typename K>
class SharedObjectDefaultTraits {
public:
	class Storage: public RefCntObj {
	public:
		template<typename ... Args >
		Storage(Args && ... args):subj(std::forward<Args>(args)...) {}
		K &get() {return subj;}
		const K &get() const {return subj;}
		void lock() const {lk.lock();}
		void unlock() const {lk.unlock();}
		void lock_shared() const {lk.lock_shared();}
		void unlock_shared() const {lk.unlock_shared();}

	protected:
		K subj;
		mutable std::shared_timed_mutex lk;
	};

	using PStorage = RefCntPtr<Storage>;
};

template<typename K>
class SharedObjectVirtualTraits {
public:
	class Storage {
	public:
		Storage (K *ptr):ptr(ptr) {}
		K &get() {return *ptr;}
		const K &get() const  {return *ptr;}
		virtual void lock() const = 0;
		virtual void unlock() const =0;
		virtual void lock_shared() const  =0;
		virtual void unlock_shared() const =0;
		virtual void addRef() const noexcept  = 0;
		virtual bool release() const noexcept = 0;
		virtual bool isShared() const = 0;
		virtual long use_count() const noexcept = 0;
		virtual Storage *clone(K *ptr) const = 0;
		virtual ~Storage() {}
	protected:
		K *ptr;
	};
	using PStorage = RefCntPtr<Storage>;

	static PStorage create(const PStorage &source, K *ptr) {
		if (&source->get() == ptr ) return source;
		else return source->clone(ptr);
	}

	template<typename Z>
	static PStorage create(RefCntPtr<Z> source, K *obj) {

		class S: public Storage {
		public:
			virtual void lock() const override {return src->lock();}
			virtual void unlock() const override {return src->unlock();}
			virtual void lock_shared() const override {return src->lock_shared();}
			virtual void unlock_shared() const override {return src->unlock_shared();}
			virtual void addRef() const noexcept  override {return src->addRef();}
			virtual bool release() const noexcept override {return src->release();}
			virtual bool isShared() const override {return src->isShared();}
			virtual long use_count() const noexcept override {return src->use_count();}
			virtual S *clone(K *ptr) const {return new S(src, ptr);}
			S(const RefCntPtr<Z> &src, K *obj):Storage(obj),src(src) {}

		protected:
			RefCntPtr<Z> src;
		};

		return PStorage(new S(source,obj));
	}
};


template<typename K, typename Traits = SharedObjectDefaultTraits<K> >
class SharedObject {

	using Subj = typename Traits::Storage;
	using PSubj = typename Traits::PStorage;

	struct Shared {
		static void lock(const PSubj &x) {x->lock_shared();}
		static void unlock(const PSubj &x) {x->unlock_shared();}
		using Ret = const K;
	};
	struct Exclusive {
		static void lock(const PSubj &x) {x->lock();}
		static void unlock(const PSubj &x) {x->unlock();}
		using Ret = K;
	};

	///Tracking lock when the object is referenced
	/**
	 * @tparam Bahev Class describes behaviour of the lock
	 */
	template<typename Behav>
	class Lock {
	public:
		using Ret = typename Behav::Ret;
		///Construct the access and claim the lock
		Lock(PSubj subj):subj(subj) {
			if (this->subj) Behav::lock(this->subj);
		}
		///Destroy the access object and release lock
		~Lock() {
			if (this->subj) Behav::unlock(this->subj);
		}
		///Move the instance to different variable
		Lock(Lock &&src):subj(std::move(src)) {}
		///Move the instance to different variable
		Lock &operator=(Lock &&src) {
			if (this->subj) Behav::unlock(this->subj);
			subj = (std::move(src.subj));
			return *this;
		}

		const Ret &operator*() const {return subj->get();}
		explicit operator const Ret *() const {return &subj->get();}
		const Ret *operator->() const {return &subj->get();}
		bool operator==(nullptr_t) const {return subj == nullptr;}
		bool operator!=(nullptr_t) const {return subj != nullptr;}
		template<typename B>
		bool operator==(const Lock<B> &oth) const {return subj == oth.subj;}
		template<typename B>
		bool operator!=(const Lock<B> &oth) const {return subj != oth.subj;}
		Ret &operator*() {return subj->get();}
		operator bool() {return subj != nullptr;}
		operator bool() const {return subj != nullptr;}
		operator Ret *() {return &subj->get();}
		Ret *operator->() {return &subj->get();}
		bool operator! () const {return subj == nullptr;}
		void release() {
			if (subj != nullptr) {
				Behav::unlock(this->subj);
				this->subj = nullptr;
			}
		}

	protected:
		PSubj subj;
	};

public:

	using LockShared = const Lock<Shared>;
	using LockExcl = Lock<Exclusive>;

	///Create empty instance
	/**If you need to construct instance, use make() */
	SharedObject() {}
	///Create empty unassigned instance
	/**If you need to construct instance, use make() */
	SharedObject(nullptr_t) {}

	SharedObject(const PSubj &sub):subj(sub) {}

	///test for null
	/**
	 * @retval true is null
	 * @retval false not null
	 */
	bool operator==(nullptr_t) const {return subj == nullptr;}
	///test for null
	/**
	 * @retval false is null
	 * @retval true not null
	 */
	bool operator!=(nullptr_t) const {return subj != nullptr;}
	///Compare two shared objects
	/**
	 * @param other object
	 * @retval true same object
	 * @retval false different objects
	 */
	bool operator==(SharedObject &oth) const {return subj == oth.subj;}
	///Compare two shared objects
	/**
	 * @param other object
	 * @retval false same object
	 * @retval true different objects
	 */
	bool operator!=(SharedObject &oth) const {return subj != oth.subj;}
	///Returns true if instance is not null
	operator bool() const {return subj != nullptr;}
	///Returns true if instance is null
	bool operator! () const {return subj == nullptr;}

	///Constructs an object
	/**
	 * @param args arguments for constructor
	 * @return new instance of SharedObject
	 */
	template<typename ... Args >
	static SharedObject make(Args && ... args) {
		return SharedObject(new Subj(std::forward<Args>(args)...));
	}

	///Lock the object exclusively
	/**
	 * Locks the object and returns a smart pointer which allows to access the object. During the pointer's live the object is locked
	 * for exclusive access
	 * @return smart pointer to acess the object
	 */
	LockExcl lock() {
		return LockExcl(subj);
	}
	///Lock the object in shared mode - because the instance is marked as const, it cannot be locked exclusive
	/**
	 * Locks the object and returns a smart pointer which allows to access the object. During the pointer's live the object is locked
	 * for shared access
	 * @return smart pointer to acess the object
	 */
	LockShared lock() const {
		return LockShared(subj);
	}
	///Lock the object in shared mode
	/**
	 * Locks the object and returns a smart pointer which allows to access the object. During the pointer's live the object is locked
	 * for shared access
	 * @return smart pointer to acess the object
	 */
	LockShared lock_shared() const {
		return LockShared(subj);
	}

	///Cast object to differen type
	/** The new type must be compatible with original type
	 *
	 * @return A kind of SharedObject which allows to access original object through different type. Note that type uses different traits. It
	 * is still counted as reference
	 */
	template<typename Z>
	SharedObject<Z,SharedObjectVirtualTraits<Z> > cast() const {
		PSubj s(subj);
		K &k = s->get();
		Z *obj = &k;
		return SharedObject<Z,SharedObjectVirtualTraits<Z> >(
				SharedObjectVirtualTraits<Z>::create(subj, obj));
	}

	///Cast object to differen type using dynamic cast
	/** The new type must be compatible with original type
	 *
	 * @return A kind of SharedObject which allows to access original object through different type. Note that type uses different traits. It
	 * is still counted as reference. If the object cannot be casted using dynamic_cast, result is nullptr
	 */
	template<typename Z>
	SharedObject<Z,SharedObjectVirtualTraits<Z> > dynamicCast() const {
		PSubj s(subj);
		K &k = s->get();
		Z *obj = dynamic_cast<Z *>(&k);
		if (obj == nullptr) {
			return SharedObject<Z,SharedObjectVirtualTraits<Z> >(nullptr);
		}
		else {
			return SharedObject<Z,SharedObjectVirtualTraits<Z> >(
					SharedObjectVirtualTraits<Z>::create(subj, obj));
		}
	}

	///release reference setting the current variable to nullptr
	void release() {
		subj = nullptr;
	}

protected:
	PSubj subj;

};



}



#endif /* SRC_ONDRA_SHARED_SHARED_H_pokqwpo9djqwpoqp */
