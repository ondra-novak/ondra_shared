#ifndef __ONDRA_SHARED_STRINGVIEW_H_pjqwhqoishxbvewuq_
#define __ONDRA_SHARED_STRINGVIEW_H_pjqwhqoishxbvewuq_

#include <string>
#include <algorithm>
#include <vector>
#include <iostream>
#if __cplusplus >= 201703L
#include <string_view>
#endif

namespace ondra_shared {

	template<typename T> struct RemoveConst {typedef T Result;};
	template<typename T> struct RemoveConst<const T> {typedef T Result;};
	template<typename T> class StringView;
	template<typename T> struct StringLess {
	public:
		bool operator()(const T &a, const T &b) const {return a < b;}
	};
	template<> struct StringLess<char> {
	public:
		bool operator()(char a, char b) const {return static_cast<unsigned char>(a) < static_cast<unsigned char>(b);}
	};
	template<> struct StringLess<const char> {
	public:
		bool operator()(char a, char b) const {return static_cast<unsigned char>(a) < static_cast<unsigned char>(b);}
	};


	///stores a refernece to string and size
	/** Because std::string is very slow and heavy */
	template<typename T>
	class StringViewBase {
	public:
		typedef T Type;
		typedef typename RemoveConst<T>::Result MutableType;

		constexpr StringViewBase() :data(0), length(0) {}
		constexpr StringViewBase(T *str) : data(str), length(calcLength(str)) {}
		constexpr StringViewBase(T *str, std::size_t length): data(str),length(length) {}

		StringViewBase &operator=(const StringViewBase &other) {
			if (&other != this) {
				this->~StringViewBase();
				new(this) StringViewBase(other);
			}
			return *this;
		}

		operator std::basic_string<MutableType>() const { return std::basic_string<MutableType>(data, length); }
#if __cplusplus >= 201703L
		operator std::basic_string_view<MutableType>() const { return std::basic_string_view<MutableType>(data, length); }
#endif
		StringViewBase substr(std::size_t index) const {
			std::size_t indexadj = std::min(index, length);
			return StringViewBase(data + indexadj, length - indexadj);
		}
		StringViewBase substr(std::size_t index, std::size_t len) const {
			std::size_t indexadj = std::min(index, length);
			return StringViewBase(data + indexadj, std::min(length-indexadj, len));
		}

		int compare(const StringViewBase &other) const {
			StringLess<T> less;
			//equal strings by pointer and length
			if (other.data == data && other.length == length) return 0;
			//compare char by char
			std::size_t cnt = std::min(length, other.length);
			for (std::size_t i = 0; i < cnt; ++i) {
				if (less(data[i],other.data[i])) return -1;
				if (less(other.data[i],data[i])) return 1;
			}
			if (length < other.length) return -1;
			if (length > other.length) return 1;
			return 0;
		}

		bool operator==(const StringViewBase &other) const { return compare(other) == 0; }
		bool operator!=(const StringViewBase &other) const { return compare(other) != 0; }
		bool operator>=(const StringViewBase &other) const { return compare(other) >= 0; }
		bool operator<=(const StringViewBase &other) const { return compare(other) <= 0; }
		bool operator>(const StringViewBase &other) const { return compare(other) > 0; }
		bool operator<(const StringViewBase &other) const { return compare(other) < 0; }

		T * const data;
		const std::size_t length;

		T &operator[](std::size_t pos) const { return data[pos]; }
		
		T *begin() const { return data; }
		T *end() const { return data + length; }

		constexpr static std::size_t calcLength(T *str) {
			return (str && *str)?(calcLength(str+1)+1):(0);
		}
		bool empty() const {return length == 0;}

		static const std::size_t npos = -1;

		std::size_t indexOf(const StringView<MutableType> sub, std::size_t pos = 0) const {
			if (sub.length > length) return npos;
			std::size_t eflen = length - sub.length + 1;
			while (pos < eflen) {
				if (substr(pos,sub.length) == sub) return pos;
				pos++;
			}
			return npos;
		}


		std::size_t lastIndexOf(const StringView<MutableType> sub, std::size_t pos = 0) const {
			if (sub.length > length) return -1;
			std::size_t eflen = length - sub.length + 1;
			while (pos < eflen) {
				eflen--;
				if (substr(eflen,sub.length) == sub) return eflen;
			}
			return npos;
		}


		///Helper class to provide operation split
		/** split() function */
		class SplitFn {
		public:
			SplitFn(const StringViewBase &source, const StringViewBase &separator, unsigned int limit)
				:source(source),separator(separator),startPos(0),limit(limit) {}

			///returns next element
			StringViewBase operator()() {
				std::size_t fnd = limit?source.indexOf(separator, startPos):npos;
				std::size_t strbeg = startPos, strlen;
				if (fnd == (std::size_t)-1) {
					strlen = source.length - strbeg;
					startPos = source.length;
				} else {
					strlen = fnd - startPos;
					startPos = fnd + separator.length;
					limit = limit -1;
				}
				return source.substr(strbeg, strlen);
			}
			///Returns rest of the string
			operator StringViewBase() const {
				return source.substr(startPos);
			}

			operator bool() const {
				return startPos < source.length;
			}
			bool operator !() const {
				return startPos >= source.length;
			}
		protected:
			StringViewBase source;
			StringViewBase separator;
			std::size_t startPos;
			unsigned int limit;
		};

		///Function splits string into parts separated by the separator
		/** Result of this function is another function, which
		 * returns part everytime it is called.
		 *
		 * You can use following code to process all parts
		 * @code
		 * auto splitFn = str.split(",");
		 * for (StrViewA s = splitFn(); !str.isEndSplit(s); s = splitFn()) {
		 * 		//work with "s"
		 * }
		 * @endcode
		 * The code above receives the split function into the splitFn
		 * variable. Next, code processes all parts while each part
		 * is available in the variable "s"
		 *
		 * Function starts returning the empty string once it extracts the
		 * last part. However not every empty result means end of the
		 * processing. You should pass the returned value to the
		 * function isSplitEnd() which can determine, whether the
		 * returned value is end of split.
		 * @param separator
		 * @param limit allows to limit count of substrings. Default is unlimited
		 * @return function which provides split operation.
		 */
		SplitFn split(const StringViewBase &separator, unsigned int limit = (unsigned int)-1) const {
			return SplitFn(*this,separator, limit);
		}

		///Determines, whether argument has been returned as end of split cycle
		/** @retval true yes
		 *  @retval false no, this string is not marked as split'send
		 * @param result
		 * @return
		 */
		bool isSplitEnd(const StringViewBase &result) const {
			return result.length == 0 && result.data == data + length;
		}

		template<typename Fn>
		StringViewBase trim(Fn &&fn) {
			StringViewBase src(*this);
			while (!src.empty() && fn(src[0])) src = src.substr(1);
			while (!src.empty() && fn(src[src.length-1])) src = src.substr(0,src.length-1);
			return src;
		}

	};

	template<typename T>
	std::ostream &operator<<(std::ostream &out, const StringViewBase<T> &ref) {
		out.write(ref.data, ref.length);
		return out;
	}


	template<typename T>
	class MutableStringView: public StringViewBase<T> {
	public:
		typedef StringViewBase< T> Base;
		using StringViewBase< T>::StringViewBase;
		constexpr MutableStringView() {}
		constexpr MutableStringView(const StringViewBase<T> &other):Base(other.data, other.length) {}

	};

	template<typename T>
	class StringView: public StringViewBase<const T> {
	public:
		typedef StringViewBase<const T> Base;
		using StringViewBase<const T>::StringViewBase;
		constexpr StringView() {}
		constexpr StringView(const StringViewBase<T> &other):Base(other.data, other.length) {}
		constexpr StringView(const StringViewBase<const T> &other):Base(other.data, other.length) {}
		constexpr StringView(const std::basic_string<T> &string) : Base(string.data(),string.length()) {}
		constexpr StringView(const std::vector<T> &string) : Base(string.data(),string.size()) {}
		constexpr StringView(const std::initializer_list<T> &list) :Base(list.begin(), list.size()) {}
		constexpr StringView(const MutableStringView<T> &src): Base(src.data, src.length) {}
		constexpr StringView(const StringView &other):Base(other) {}
#if __cplusplus >= 201703L
		constexpr StringView(const std::basic_string_view<T> &str):Base(str.data(), str.length()) {}
#endif
		StringView substr(std::size_t index) const {
			return StringView(Base::substr(index));
		}
		StringView substr(std::size_t index, std::size_t len) const {
			return StringView(Base::substr(index,len));
		}
		bool begins(const StringView &prefix) const {
			return substr(0,prefix.length) == prefix;
		}
		bool ends(const StringView &suffix) const {
			return suffix.length<this->length && substr(this->length-suffix.length) == suffix;
		}


		class SplitFn: public Base::SplitFn {
		public:
			typedef typename Base::SplitFn SplitBase;
			SplitFn(const SplitBase &other):SplitBase(other) {}


			///returns next element
			StringView operator()() {return StringView(static_cast<SplitBase &>(*this)());}
			operator StringView () {return StringView(Base(static_cast<SplitBase &>(*this)));}
		};

		SplitFn split(const StringView &separator, unsigned int limit = (unsigned int)-1) const {
			return SplitFn(Base::split(separator, limit));
		}
		template<typename Fn>
		StringView trim(const Fn &fn) {
			return StringView(Base::trim(fn));
		}

	};


	typedef StringView<char> StrViewA;
	typedef StringView<wchar_t> StrViewW;


	class BinaryView: public StringView<unsigned char>{
	public:
		using StringView<unsigned char>::StringView;

		BinaryView() {}

		BinaryView(const StringViewBase<unsigned char> &from)
			:StringView<unsigned char>(from)
		{}

		///Explicit conversion from any view to binary view
		/** it might be useful for simple serialization, however, T should be POD or flat object */
		template<typename T>
		explicit BinaryView(const StringViewBase<T> &from)
			:StringView<unsigned char>(reinterpret_cast<const unsigned char *>(from.data), from.length*sizeof(T))
		{}


		///Explicit conversion binary view to any view
		/** it might be useful for simple deserialization, however, T should be POD or flat object */
		template<typename T>
		explicit operator StringView<T>() const {
			return StringView<T>(reinterpret_cast<const T *>(data), length/sizeof(T));
		}
	};


	class MutableBinaryView: public MutableStringView<unsigned char> {
	public:
		using MutableStringView<unsigned char>::MutableStringView;

		MutableBinaryView() {}

		MutableBinaryView(const StringViewBase<unsigned char> &from)
			:MutableStringView<unsigned char>(from)
		{}

		///Explicit conversion from any view to binary view
		/** it might be useful for simple serialization, however, T should be POD or flat object */
		template<typename T>
		explicit MutableBinaryView(const StringViewBase<T> &from)
			:MutableStringView<unsigned char>(reinterpret_cast<unsigned char *>(from.data), from.length*sizeof(T))
		{}


		///Explicit conversion binary view to any view
		/** it might be useful for simple deserialization, however, T should be POD or flat object */
		template<typename T>
		explicit operator MutableStringView<T>() const {
			return MutableStringView<T>(reinterpret_cast<T *>(data), length/sizeof(T));
		}
		///Explicit conversion binary view to any view
		/** it might be useful for simple deserialization, however, T should be POD or flat object */
		template<typename T>
		explicit operator StringView<T>() const {
			return StringView<T>(reinterpret_cast<const T *>(data), length/sizeof(T));
		}
	};




}


#endif
