#ifndef __ONDRA_SHARED_STRINGPOOL_86451564598984
#define __ONDRA_SHARED_STRINGPOOL_86451564598984


#include <memory>
#include <vector>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include "stringview.h"

namespace ondra_shared {



template<typename T>
class StringPoolTraits {
public:
     static inline void putEnd(std::vector<T> &) {}
     static inline const T *empty(std::vector<T> &) {return nullptr;}
};

template<>
class StringPoolTraits<char> {
public:
     static inline void putEnd(std::vector<char> &buffer) {buffer.push_back((char)0);}
     static inline const char *empty() {return "";}
};

template<>
class StringPoolTraits<wchar_t> {
public:
     static inline void putEnd(std::vector<wchar_t> &buffer) {buffer.push_back((wchar_t)0);}
     static inline const wchar_t *empty() {return L"";}
};



///String pool - creates and manages strings in a pool
/**
 * The pool is defined as large array of characters located at one place. Adding strings
 * to the pool is faster then creating new standalone string, because most o the memory space
 * is already allocated. It is also more efficient to put multiple string together because
 * there is minimal wasted memory space between strings.
 *
 * @note to avoid reference invalid memory, any instance of string from the pool locks
 * whole pool preventing it to dealloc even if you need only small portion of the pool
 *
 *
 */
template<typename T, typename View = StringView<T> >
class StringPool {
public:

     using PoolData = std::vector<T>;
     using PPool = std::shared_ptr<PoolData>;

     class String {
     public:
          String(const PPool &buffer, std::size_t offset, std::size_t length)
               :buffer(buffer),offset(offset),length(length) {}
          String(const View &str):buffer(nullptr),ptr(stGetData(str)),length(stGetLength(str)) {}
          String():buffer(nullptr),ptr(StringPoolTraits<T>::empty()),length(0) {}

          operator View() const {
               return getView();
          }

          View getView() const {
               if (buffer) return View(buffer->data(), buffer->size()).substr(offset,length);
               else return View(ptr, length);
          }


          std::size_t getLength() const {
               return length;
          }

          const T *getData() const {
               if (buffer) return buffer->data()+offset;
               else return ptr;
          }

          bool empty() const {return length == 0;}

          bool operator==(const String &other) const {return getView() == other.getView();}
          bool operator!=(const String &other) const {return getView() != other.getView();}
          bool operator>(const String &other) const {return getView() > other.getView();}
          bool operator<(const String &other) const {return getView() < other.getView();}
          bool operator>=(const String &other) const {return getView()>= other.getView();}
          bool operator<=(const String &other) const {return getView()<= other.getView();}

          ///depricated call
          void relocate(StringPool & ) const {}

     protected:
          PPool buffer;
          union {
               std::size_t offset;
               const T *ptr;
          };
          std::size_t length;
     };


     ///Adds string to the pool
     /**
      * @param str string to add to pool
      * @return returns object which refers to the string in the pool
      */
     String add(const View &str) {
          if (str.empty()) return String();
          std::size_t pos = data->size();
          data->insert(data->end(), str.begin(), str.end());
          StringPoolTraits<T>::putEnd(*data);
          return String(data, pos, stGetLength(str));
     }

     ///Starts adding charactes and parts of strings to the pool
     /**
      * @return return mark, which refers to the begin of the string
      *
      * Because the object is not MT safe, this doesn't involves any locking mechanism.
      * Calles just need to avoid to call the function add() between begin_add() and end_add()
      */
     std::size_t begin_add() {
          return data->size();
     }

     ///Pushes character to the pool
     /**
      * @param t charater to push
      *
      * Function can be called only after begin_add(), otherwise, the character becomes
      * inaccessible
      */
     void push_back(const T &t) {
          data->push_back(t);
     }
     ///Appends string
     /**
      * @param str string to append
      *
      * Function can be called only after the function begin_add(), otherwise the string
      * becomes inaccesible
      */
     void append(const View &str) {
          data->insert(data->end(), str.begin(), str.end());
     }
     ///Finishes adding string and returns object which referes to it
     /**
      * @param mark mark position returned by begin_add()
      * @return string reference
      */
     String end_add(std::size_t mark) {
          std::size_t pos = data->size();
          StringPoolTraits<T>::putEnd(*data);
          return String(data, mark, pos-mark);
     }

     ///Discards all characters added after last begin_add()
     /**
      * @param mark mark returned by begin_add()
      *
      */
     void discard_add(std::size_t mark) {
          data->resize(mark);
     }

     ///Deletes whole pool
     /** Note - ensure, that there is no reference to the strings in the pool
      */
     void clear() {
          data->clear();
     }


     StringPool():data(std::make_shared<PoolData>()) {}

protected:
     PPool data;


     static const T * stGetData(const StringView<T> &x) {return x.data;}
     static std::size_t stGetLength(const StringView<T> &x) {return x.length;}
     template<typename X>
     static const T * stGetData(const X &x) {return x.data();}
     template<typename X>
     static std::size_t stGetLength(const X &x) {return x.size();}
};

#if __cplusplus >= 201703L
template<typename T> using basic_string_pool = StringPool<T, std::basic_string_view<T> >;
using string_pool = StringPool<char, std::string_view >;
#endif

}

#endif
