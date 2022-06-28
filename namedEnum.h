#pragma once
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>

#if __cplusplus >= 201703L
#include <string_view>
#endif



///Defines table of enums with their names
/**
 * It is expected that this is statically allocated object. You construct
 * this object by passing a table to the constructor. The table has two collums
 * {<enum>,<name>}
 *
 * where <enum> is value of enumeration type and <name> is name of that value
 * (a string representation)
 *
 * @code
 * NamedEnum<int> numbers({
 *  {1,"one"},
 *  {2,"two"},
 *  {3,"three"},
 *  ....
 *
 * }
 *
 * There is no requirement to have recors unique. You can assign multiple names
 * to single enum, and vice verse. However, if there are multiple records
 * for single items to be returned, a random one can be returned.
 *
 *
 * @endcode
 *
 */
template<typename EnumType>
class NamedEnum {

public:
    ///Definition of row of the table
    /** Is public allows you tu fill the table */
     struct Def {
          EnumType value;
          std::string name;
     };

     ///Constructs the NamedEnum instance using the table
     /**
      * @param x array containing rows for each enum
      */
     template<std::size_t N>
     explicit NamedEnum(const Def(&x)[N]);

     ///Object can be copied - however, it is not recommended
     NamedEnum(const NamedEnum &other);

     ///Object cannot be assigned
     NamedEnum &operator=(const NamedEnum &other) = delete;

    ///Find enum by name
    /**
     * @param name name (as std::string)
     * @return pointer to EnumType or nullptr, if there is no such record
     */
    const EnumType *find(const std::string &name) const {
        return findT(name);
    }
#if __cplusplus >= 201703L

    ///Find enum by name
    /**
     * @param name name (as string_view)
     * @return pointer to EnumType or nullptr, if there is no such record
     */
    const EnumType *find(const std::string_view &name) const {
        return findT(name);
    }
#endif
    ///Find enum by name
    /**
     * @param name name (const char *)
     * @return pointer to EnumType or nullptr, if there is no such record
     */
    const EnumType *find(const char *name) const {
        return findT(name);
    }
    ///Returns EnumType by string
    /**
     * @param name name (as std::string)
     * @return associated enum value
     * @exception UnknownEnumException
     */
    EnumType get(const std::string &name) const {
        auto x = find(name);
        return except(name,x);
    }
#if __cplusplus >= 201703L
    ///Returns EnumType by string
    /**
     * @param name name (as string_view)
     * @return associated enum value
     * @exception UnknownEnumException
     */
    EnumType get(const std::string_view &name) const {
        auto x = find(name);
        return except(name,x);
    }
#endif
    ///Returns EnumType by string
    /**
     * @param name name (as const char *)
     * @return associated enum value
     * @exception UnknownEnumException
     */
    EnumType get(const char *name) const {
        auto x = find(name);
        return except(name,x);
    }

    ///Returns EnumType for given name. If not defined, returns default value
    /**
     * @param name name (as std::string);
     * @param def default value in case, that name cannot be resolved
     * @return associated or default enum
     */
    EnumType get(const std::string &name, const EnumType &def) const {
        auto x = find(name);
        return x?*x:def;
    }
#if __cplusplus >= 201703L

    ///Returns EnumType for given name. If not defined, returns default value
    /**
     * @param name name (as std::string_view);
     * @param def default value in case, that name cannot be resolved
     * @return associated or default enum
     */
    EnumType get(const std::string_view &name, const EnumType &def) const {
        auto x = find(name);
        return x?*x:def;
    }
#endif
    ///Returns EnumType for given name. If not defined, returns default value
    /**
     * @param name name (as const char *);
     * @param def default value in case, that name cannot be resolved
     * @return associated or default enum
     */
    EnumType get(const char *name, const EnumType &def) const {
        auto x = find(name);
        return x?*x:def;
    }

    ///Returns name of given enum value
    /**
     * @param val enum value
     * @return return associated string with the value, or empty string, if value is unknown
     */
    const std::string &get(EnumType val) const;


    ///Implements array operator
    /**
     * @see get()
     */
     EnumType operator[](const std::string &name) const {
         return get(name);
     }
#if __cplusplus >= 201703L

    ///Implements array operator
    /**
     * @see get()
     */
    EnumType operator[](const std::string_view &name) const {
        return get(name);
    }
#endif
    ///Implements array operator
    /**
     * @see get()
     */
    EnumType operator[](const char *name) const {
        return get(name);
    }

    ///Implements array operator
    /**
     * @see get()
     */
    const std::string &operator[](EnumType val) const;


    ///Retrieves begin of iteratable container
    /**
     * @note order is not preserved
     */
     typename std::vector<Def>::const_iterator begin() const {return byVal.begin();}
    ///Retrieves end of iteratable container
    /**
     * @note order is not preserved
     */
     typename std::vector<Def>::const_iterator end() const {return byVal.end();}

     ///Returns count of definitions
     std::size_t size() const {return byVal.size();}

#if __cplusplus >= 201703L

     ///Converts to string
     /**
      * It returns list of enums separated by specified separator
      *
      * @param separator
      */
     std::string toString(const std::string_view &separator = ", ") const;
#else

     std::string toString(const std::string &separator = ", ") const;
#endif

protected:

     ///protected to avoid empty enums
     NamedEnum() = default;

     template<typename Q>
    const EnumType *findT(const Q &name) const;


     std::vector<Def> byVal;
     std::vector<const Def *> byName;

     static bool cmpByVal(const Def &a, const Def &b) {
          return a.value < b.value;
     }

     struct CmpByName {
         bool operator()(const Def *a, const Def *b) const {
              return a->name < b->name;
         }
        bool operator()(const Def *a, const std::string &b) const {
            return a->name < b;
        }
        bool operator()(const std::string &a, const Def *b) {
            return a < b->name;
        }
#if __cplusplus >= 201703L
        bool operator()(const Def *a, const std::string_view &b) const {
            return a->name < b;
        }
        bool operator()(const std::string_view &a, const Def *b) {
            return a < b->name;
        }
#endif
        bool operator()(const Def *a, const char *b) const {
            return a->name.compare(b) < 0;
        }
        bool operator()(const char *a, const Def *b) {
            return b->name.compare(a) < 0;
        }
     };

     void updateByName();

     template<typename T>
    EnumType except(const T &name, const EnumType *findRes) const;

};


class UnknownEnumException: public std::exception {
public:

     UnknownEnumException(std::string &&errorEnum, std::string &&allEnums)
         :errorEnum(std::move(errorEnum))
         ,allEnums(std::move(allEnums)) {}


     virtual const char *what() const throw() {
         if (whatMsg.empty()) {
             whatMsg =  "Unknown enum: '" + errorEnum + "'. Missing in following list: '" + allEnums + "'";
         }
         return whatMsg.c_str();
     }

     const std::string& getErrorEnum() const {
          return errorEnum;
     }


protected:
     mutable std::string whatMsg;
     std::string errorEnum;
     std::string allEnums;

};

///Allows to declare enum and prepare table where each enum is mapped to its literal representation
/**
 * @param Typename Type of enum (must not exist, it will be declared
 * @param ... args Comma separated list of enumerated items, similar to enum declaration. Each item
 * can contain = <index> to reassing index - like real 'enum'. However, only numeric constant
 * is allowed here, expressions are not supported. You also cannot use defined constants and macros
 *
 * Result of this macro is two declarations. The enum itself and the class which
 * is able to convert enum to string and vice versa. The class is extension of NamedEnum;
 *
 * @code
 *
 *   // declare enum Colors
 *   NAMED_ENUM(Colors, red, green, blue=10, yellow=0x23, white=032)
 *   // declare object contains conversion to string
 *   NamedEnumColors strColor;
 *
 * @endcode
 */

#define NAMED_ENUM(Typename, ...) enum class Typename { __VA_ARGS__}; \
    class NamedEnum_##Typename: public NamedEnumAuto<Typename> {\
    public: \
        NamedEnum_##Typename():NamedEnumAuto(#__VA_ARGS__) {} \
        NamedEnum_##Typename(const char *prefix, const char *suffix):NamedEnumAuto(#__VA_ARGS__, prefix, suffix) {} \
}




template<typename EnumType>
class NamedEnumAuto: public NamedEnum<EnumType> {

    using TextIterator = const char *;
public:
    NamedEnumAuto(const char *textDef, const char *prefix = "", const char *suffix = "");

private:
    static int parseToken(TextIterator &x, std::string &buffer);
    static int parseIndex(TextIterator &x);
    static int parseIndexHex(TextIterator &x);
    static int parseIndexOctal(TextIterator &x);
    static void checkSeparator(TextIterator &x);
    void addEnum(std::string &buffer, int index, const char *suffix);
};

class SyntaxtErrorNamedEnumException: public std::exception {
public:
    SyntaxtErrorNamedEnumException(const std::string &errorPart):errorPart(errorPart) {}

    void setWholeDef(const std::string &wholeDef) {
        this->wholeDef = wholeDef;
    }
    const char *what() const noexcept {
        if (msg.empty()) {
            msg = "NamedEnum: Syntax error at position " + std::to_string(wholeDef.length() - errorPart.length())
                    + " of definition " + wholeDef;
        }
        return msg.c_str();
    }
protected:
    std::string errorPart;
    std::string wholeDef;
    mutable std::string msg;


};




template<typename EnumType>
template<std::size_t N>
inline NamedEnum<EnumType>::NamedEnum(const Def (&x)[N])
   :byVal(std::begin(x), std::end(x))
{
    updateByName();
}


template<typename EnumType>
inline NamedEnum<EnumType>::NamedEnum(const NamedEnum &other)
:byVal(other.byVal) {
    updateByName();
}

template<typename EnumType>
inline const std::string& NamedEnum<EnumType>::get(EnumType val) const {
    auto iter = std::lower_bound(byVal.begin(), byVal.end(), Def{val}, cmpByVal);
    if (iter == byVal.end() || iter->value != val) {
        static std::string emptyString;
        return emptyString;
    } else {
        return iter->name;
    }
}

template<typename EnumType>
inline const std::string& NamedEnum<EnumType>::operator [](EnumType val) const {
    return get(val);
}

#if __cplusplus >= 201703L
template<typename EnumType>
inline std::string NamedEnum<EnumType>::toString(const std::string_view &separator) const {
#else
template<typename EnumType>
inline std::string NamedEnum<EnumType>::toString(const std::string &separator) const {
#endif
    std::size_t reqSize = std::accumulate(byName.begin(), byName.end(), 0,
        [&](std::size_t sz, const Def *p) {return sz+p->name.length();})
                + separator.size()*byName.size();
    std::string out;
    out.reserve(reqSize);
    auto iter = byName.begin();
    if (iter != byName.end()) {
        out.append((*iter)->name);
        ++iter;
        while (iter != byName.end()) {
            out.append(separator);
            out.append((*iter)->name);
            ++iter;
        }
    }
    return out;

}


template<typename EnumType>
inline void NamedEnum<EnumType>::updateByName() {
    std::sort(byVal.begin(), byVal.end(), cmpByVal);
    byName.reserve(byVal.size());
    byName.clear();
    std::transform(byVal.begin(), byVal.end(), std::back_inserter(byName),
            [&](const Def &def) {
       return &def;
    });
    std::sort(byName.begin(), byName.end(), CmpByName());
}


template<typename EnumType>
template<typename Q>
inline const EnumType* NamedEnum<EnumType>::findT(const Q &name) const {
    auto iter= std::lower_bound(byName.begin(), byName.end(), name, CmpByName());
    if (iter == byName.end() || (*iter)->name.compare(name) != 0) {
        return nullptr;
    } else {
        return &(*iter)->value;
    }
}


template<typename EnumType>
template<typename T>
inline EnumType NamedEnum<EnumType>::except(const T &name,
        const EnumType *findRes) const {
    if (findRes == nullptr) throw UnknownEnumException(std::string(name),toString());
    return *findRes;
}


template<typename EnumType>
inline NamedEnumAuto<EnumType>::NamedEnumAuto(const char *textDef, const char *prefix, const char *suffix) {
    //we need to parse list of enums.
    /*
     * enum defintion: text,text, text, text=number, text (number+1)
     */

    try {

        std::string buffer(prefix);
        TextIterator x = textDef;
        int index = 0;

        int a = parseToken(x, buffer);
        while (a >= 0) {
            if (a == 1) {
                index = parseIndex(x);
            }
            checkSeparator(x);
            addEnum(buffer,index, suffix);
            buffer = prefix;
            index++;
            a = parseToken(x, buffer);
        }
        if (!buffer.empty()) {
            addEnum(buffer,index, suffix);
        }
    } catch (SyntaxtErrorNamedEnumException &err) {
        err.setWholeDef(textDef);
        throw;
    }

    this->updateByName();
}

template<typename EnumType>
inline int NamedEnumAuto<EnumType>::parseToken(TextIterator &x, std::string &buffer)  {
    while (*x && isspace(*x)) ++x;
    if (!*x) return -1;
    while (*x && (isalnum(*x) || *x == '_')) {
        buffer.push_back(*x);
        ++x;
    }
    if (buffer.empty()) throw SyntaxtErrorNamedEnumException(x);
    while (*x && isspace(*x)) ++x;
    if (*x == '=') {
        ++x;
        return 1;
    } else {
        return 0;
    }
}

template<typename EnumType>
inline int NamedEnumAuto<EnumType>::parseIndex(TextIterator &x) {
    while (*x && isspace(*x)) ++x;
    if (*x == '-') {
        ++x; return -parseIndex(x);
    } else if (*x == '+') {
        ++x;
    }
    if (*x == '0') {
        ++x;
        if (*x == 'x' || *x == 'X') {
            ++x;
            return parseIndexHex(x);
        } else {
            return parseIndexOctal(x);
        }
    } else if (isdigit(*x)) {
        int index = 0;
        while (isdigit(*x)) {
            index = index * 10 + (*x - '0');
            ++x;
        }
        return index;
    } else throw SyntaxtErrorNamedEnumException(x);
}
template<typename EnumType>
inline int NamedEnumAuto<EnumType>::parseIndexHex(TextIterator &x) {
        int index = 0;
        while (isxdigit(*x)) {
            index = index * 16 + (isdigit(*x)?(*x - '0'):(toupper(*x)-'A'+10));
            ++x;
        }
        return index;
}



template<typename EnumType>
inline int NamedEnumAuto<EnumType>::parseIndexOctal(TextIterator &x) {
    int index = 0;
    while (isdigit(*x) && *x < '8') {
        index = index * 8 + (*x - '0');
        ++x;
    }
    return index;

}
template<typename EnumType>
inline void NamedEnumAuto<EnumType>::checkSeparator(TextIterator &x) {
    while (*x && isspace(*x)) ++x;
    if (*x && *x != ',') throw SyntaxtErrorNamedEnumException(x);
    if (*x) ++x;
}

template<typename EnumType>
inline void NamedEnumAuto<EnumType>::addEnum(std::string &buffer, int index, const char *suffix) {
    buffer.append(suffix);
    this->byVal.push_back({
        static_cast<EnumType>(index),
                buffer
    });
}



