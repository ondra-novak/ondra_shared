/*
 * ini_config.h
 *
 *  Created on: 25. 11. 2017
 *      Author: ondra
 */

#ifndef _ONDRA_SHARED_INI_CONFIG_23123148209810_
#define _ONDRA_SHARED_INI_CONFIG_23123148209810_
#include <map>
#include <string>
#include <fstream>

#include "finally.h"


#include "ini_parser.h"

#include "stringpool.h"
#include "virtualMember.h"

namespace ondra_shared {


class IniConfig {
public:


	typedef StringPool<char> StrPool;
	typedef StrPool::String String;

	struct Value {
		String v;
		String p;

		Value(){}
		Value(String v,String p):v(v),p(p) {}
		std::string getPath() const;
		///retrieves current path for this value
		StrViewA getCurPath() const;
		std::size_t getUInt() const;
		std::intptr_t getInt() const;
		bool getBool() const;
		double getNumber() const;
		StrViewA getString() const {return v.getView();}
		const char *c_str() const {return v.getView().data;}

		std::string getPath(std::string &&default_path) const;
		StrViewA getCurPath(StrViewA defval) const;
		std::size_t getUInt(std::size_t default_value) const;
		bool getBool(bool default_value) const;
		std::intptr_t getInt(std::size_t default_value) const;
		double getNumber(double default_value) const;
		StrViewA getString(const StrViewA &default_value) const;
		const char *c_str(const char *default_value) const;

		template<typename T>
		static void checkSuffix(char c, T &v);

		static const Value &undefined() {
			static Value udef(String(StrViewA("undef",0)), String());
			return udef;
		}
		bool defined() const {
			return this->v.getData() != undefined().v.getData();
		}
	};

	class NotFoundException: public std::exception {
	public:
		NotFoundException(std::string &&section, std::string &&key)
			:section(std::move(section)),key(std::move(key))
		{
			msg = std::string("Config option ") + this->section+"."+this->key+" is mandatory but missing.";
		}

		const char *what() const throw() {return msg.c_str();}

	protected:
		std::string section;
		std::string key;
		std::string msg;

	};

	class ConfigLoadError: public std::exception {
	public:
		ConfigLoadError(const std::string &msg):msg("Failed to load config: "+msg) {}
		const char *what() const throw() {return msg.c_str();}
	protected:
		std::string msg;
	};

	class KeyValueMap: public std::map<String, Value> {
	public:
		class Mandatory:public VirtualMember<KeyValueMap> {
		public:
			using VirtualMember<KeyValueMap>::VirtualMember;
			const Value &operator[](StrViewA name) const {
				return getMaster()->getMandatory(name);
			}
		};

		Mandatory mandatory;

		KeyValueMap(const String &section):mandatory(this),section(section) {}
		KeyValueMap(const KeyValueMap &other):std::map<String, Value>(other),mandatory(this),section(other.section) {}
		KeyValueMap(KeyValueMap &&other):std::map<String, Value>(std::move(other)),mandatory(this),section(other.section) {}

		const Value &operator[](StrViewA name) const {
			auto iter = find(String(name));
			if (iter == end()) return Value::undefined();
			else return iter->second;

		}

	private:
		const Value &getMandatory(StrViewA name) const {
			auto iter = find(String(name));
			if (iter == end()) throw NotFoundException(section.getView(), name);
			else return iter->second;
		}

		void changeName(const StrViewA &name) const {section = name;}



		mutable String section;

		friend class IniConfig;
	};


	using Section = KeyValueMap;

	typedef std::map<String, KeyValueMap> SectionMap;


	const Section &operator[](const StrViewA &sectionName) const;
	SectionMap::const_iterator begin() const;
	SectionMap::const_iterator end() const;


	template<typename Fn, typename D>
	void load(const Fn &fn, const StrViewA &path, D &&directives, const StrViewA &curSection);

	template<typename D>
	bool load(const std::string &pathname,  D &&directives, const StrViewA &curSection = StrViewA());

	///Loads config but using reference path to interpret relative paths in the config
	template<typename D>
	bool load_setpath(const std::string &pathname, const StrViewA &refpath,  D &&directives, const StrViewA &curSection = StrViewA());

	void load(const std::string &pathname);

	///Loads config but using reference path to interpret relative paths in the config
	void load_setpath(const std::string &pathname, const StrViewA &refpath);

	Value createValue(const String &value);

	void load(const IniItem &item);

	IniConfig():emptyMap(StrViewA()) {}


#ifdef _WIN32
	StrViewA pathSeparator="\\";
	static inline bool isRooted(StrViewA path) {
		return !path.empty() && (path[0] == '\\' ||
				(path.length > 2 && isalpha(path[0]) && path[1] == ':' && path[2] == '\\'));
	}
#else
	static constexpr StrViewA pathSeparator="/";
	static inline bool isRooted(StrViewA path) {
		return !path.empty() && path[0] == '/';
	}
#endif

protected:

	SectionMap smap;
	StrPool pool;
	KeyValueMap emptyMap;
	String curPath;


	KeyValueMap &loadSection(StrViewA section);
};



inline std::string IniConfig::Value::getPath() const {
	std::string s;
	if (v.empty()) return std::string();
	s.reserve(v.getLength() + p.getLength());
	s.append(p.getData(),p.getLength());
	s.append(v.getData(),v.getLength());
	return s;
}

inline StrViewA IniConfig::Value::getCurPath() const {
	return StrViewA(p).substr(0,p.getLength()-1);
}
inline const IniConfig::KeyValueMap& IniConfig::operator [](const StrViewA& sectionName) const {
	auto itr = smap.find(sectionName);
	if (itr == smap.end()) {
		emptyMap.changeName(sectionName);
		return emptyMap;
	} else{
		return itr->second;
	}
}

inline IniConfig::SectionMap::const_iterator IniConfig::begin() const {
	return smap.begin();
}

inline IniConfig::SectionMap::const_iterator IniConfig::end() const {
	return smap.end();
}

template<typename Fn, typename D>
inline void IniConfig::load(const Fn &fn, const StrViewA &path, D &&directives, const StrViewA &curSection) {

	String prevPath = curPath;
	auto f1 = finally([&]{curPath = prevPath;});

	auto sep = path.lastIndexOf(pathSeparator);
	if (sep != path.npos) {
		StrViewA newpath = StrViewA(path).substr(0,sep+1);
		curPath = pool.add(newpath);
	}


	class ProcessFn {
	public:
		IniConfig &owner;
		D &&directives;
		StrViewA curSection;

		void operator()(const IniItem &item) const {
			if (item.section.empty() && !curSection.empty()) {
				IniItem newItem = item;
				newItem.section = curSection;
				operator()(newItem);
			} else {
				if (item.type == IniItem::data)	owner.load(item);
				else if (item.type == IniItem::directive) directives(item);
			}
		}

		ProcessFn(IniConfig &owner, D &&directives, const StrViewA &curSection)
			:owner(owner),directives(std::forward<D>(directives)),curSection(curSection) {}
	};

	ProcessFn processFn(*this,std::forward<D>(directives), curSection);
	IniParser<ProcessFn &&> parse(std::move(processFn));

	int i = fn();
	while (i != -1) {
		parse(i);
		i = fn();
	}

}

template<typename D>
inline bool IniConfig::load(const std::string& pathname,D&& directives, const StrViewA &curSection) {
	return load_setpath(pathname, pathname, std::forward<D>(directives), curSection);
}

template<typename D>
inline bool IniConfig::load_setpath(const std::string& pathname, const StrViewA &path, D&& directives, const StrViewA &curSection) {


	std::ifstream input(pathname);
	if (!input) return false;

	auto mydirect = [this, directives = std::forward<D>(directives)](const IniItem &item) mutable {
		if (item.key == "include") {
			Value v(createValue(item.value));
			if (this->load(v.getPath(),directives, item.section)) return;
		}
		if (item.key == "template") {
			auto& sec = operator[](item.value);
			KeyValueMap &kv = loadSection(item.section);
			kv.insert(sec.begin(), sec.end());
			return;
		}
		directives(item);
	};

	auto reader = [&]{
		return input.get();
	};

	load(reader, path, std::move(mydirect), curSection);
	return true;
}

inline void IniConfig::load_setpath(const std::string& pathname, const StrViewA &path) {
	if (!load_setpath(pathname, path, [](const IniItem &itm){
		std::string msg;
		msg.append("[")
				.append(itm.section.data, itm.section.length)
				.append("] @")
				.append(itm.key.data, itm.key.length)
				.append(" ")
				.append(itm.value.data, itm.value.length);
		throw ConfigLoadError(msg);
	} )) {
		throw ConfigLoadError(pathname);
	}
}


inline void IniConfig::load(const std::string& pathname) {
	load_setpath(pathname, pathname);
}

template<typename T>
inline void IniConfig::Value::checkSuffix(char c, T &v) {
	switch (c) {
	//12s = 12 seconds = 12000 miliseconds
	case 's': v = v * static_cast<T>(1000);break;
	//5m = 5 minutes = 300000 miliseconds
	case 'm': v = v * static_cast<T>(60000);break;
	//4h = 4 hours
	case 'h': v = v * static_cast<T>(3600000);break;
	//2d = 2 days
	case 'd': v = v * static_cast<T>(24*3600000);break;
	//124K = 125 Kilo (1000x)
	case 'K':
	//124k = 125 kilo (1000x)
	case 'k': v = v * static_cast<T>(1000);break;
	//32M = 32 mega (1000x1000)
	case 'M': v = v * static_cast<T>(1000000);break;
	//21G = 21 giga(1000x1000x1000)
	case 'G': v = v * static_cast<T>(1000000000);break;
	}
}

inline std::size_t IniConfig::Value::getUInt() const {
	std::size_t x = 0;
	for (char c : v.getView()) {
		if (isdigit(c)) x = x * 10 + (c - '0');
		else {
			checkSuffix(c, x);
			break;
		}
	}
	return x;
}

inline bool IniConfig::Value::getBool() const {

	constexpr auto cmpstr = [](StrViewA a, StrViewA b) {
		if (a.length == b.length) {
			for (std::size_t i = 0; i < a.length; i++)
				if (toupper(a[i]) != toupper(b[i])) return false;
			return true;
		} else {
			return false;
		}
	};

	constexpr StrViewA yes_forms[] = {"true","1","yes","y","on"};
	auto sv = getString();
	for (auto && x: yes_forms) {
		if (cmpstr(sv,x)) return true;
	}
	return false;
}

inline std::intptr_t IniConfig::Value::getInt() const {
	auto sv = v.getView();
	if (sv.empty()) return 0;
	if (sv[0] == '-') {
		Value z;
		z.v = sv.substr(1);
		return - (std::intptr_t) z.getUInt();
	} else {
		return (std::intptr_t)getUInt();
	}
}

inline IniConfig::Value IniConfig::createValue(const String &value) {
	Value vv;
	if (isRooted(value)) {
		vv.p = StrViewA();
	} else {
		vv.p = curPath;
	}
	vv.v = value;
	return vv;
}

inline IniConfig::KeyValueMap &IniConfig::loadSection(StrViewA section) {
	auto s = smap.find(section);
	KeyValueMap *kv;
	if (s == smap.end()) {
		String sname = pool.add(section);
		s = smap.insert(std::make_pair(sname, KeyValueMap(sname))).first;
	}
	kv = &s->second;
	return *kv;

}
inline void IniConfig::load(const IniItem& item) {
	KeyValueMap &sect = loadSection(item.section);
	String k = pool.add(item.key);
	String v = pool.add(item.value);
	Value vv = createValue(v);
	auto z = sect.insert(std::make_pair(k,vv));
	if (!z.second) {
		z.first->second = vv;
	}
}

inline std::string IniConfig::Value::getPath(std::string&& default_path) const {
	if (defined()) return getPath();
	else return std::move(default_path);
}

inline StrViewA IniConfig::Value::getCurPath(StrViewA default_path) const {
	if (defined()) return getCurPath();
	else return default_path;
}

inline std::size_t IniConfig::Value::getUInt(std::size_t default_value) const {
	if (defined()) return getUInt();
	else return default_value;

}

inline std::intptr_t IniConfig::Value::getInt(std::size_t default_value) const {
	if (defined()) return getInt();
	else return default_value;
}

inline bool IniConfig::Value::getBool(bool default_value) const {
	if (defined()) return getBool();
	else return default_value;
}

inline StrViewA IniConfig::Value::getString(const StrViewA& default_value) const {
	if (defined()) return getString();
	else return default_value;
}

inline double IniConfig::Value::getNumber() const {
	char *c;
	double d = strtod(c_str(),&c);
	if (c) checkSuffix(*c,d);
	return d;
}

inline double IniConfig::Value::getNumber(double default_value) const {
	if (defined()) return getNumber();
	else return default_value;
}

inline const char* IniConfig::Value::c_str(const char* default_value) const {
	if (defined()) return c_str();
	else return default_value;
}

}


#endif /* INI_CONFIG_H_ */
