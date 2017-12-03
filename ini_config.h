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
		std::size_t getUInt() const;
		std::intptr_t getInt() const;
		StrViewA getString() const {return v.getView();}
		const char *c_str() const {return v.getView().data;}

		static const Value &undefined() {
			static Value udef;
			return udef;
		}
		bool defined() const {return this != &undefined();}
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
	typedef std::map<String, KeyValueMap> SectionMap;


	const KeyValueMap &operator[](const StrViewA &sectionName) const;
	SectionMap::const_iterator begin() const;
	SectionMap::const_iterator end() const;


	template<typename Fn, typename D>
	void load(const Fn &fn, StrViewA path, const D &directives);

	template<typename D>
	bool load(const std::string &pathname, const D &directives);

	void load(const std::string &pathname);


	void load(const IniItem &item);

	IniConfig():emptyMap(StrViewA()) {}
	IniConfig(const IniConfig &other);
	IniConfig(IniConfig &&other);

	IniConfig &operator=(const IniConfig &other);
	IniConfig &operator=(IniConfig &&other);


#ifdef _WIN32
	char pathSeparator='\\';
#else
	char pathSeparator='/';
#endif

protected:

	SectionMap smap;
	StrPool pool;
	KeyValueMap emptyMap;
	String curPath;

};



inline std::string IniConfig::Value::getPath() const {
	std::string s;
	s.reserve(v.getLength() + p.getLength());
	s.append(p.getData(),p.getLength());
	s.append(v.getData(),v.getLength());
	return s;
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
inline void IniConfig::load(const Fn& fn, StrViewA path, const D& directives) {

	auto processFn = [this,directives](const IniItem &item) {
		if (item.type == IniItem::data)	this->load(item);
		else if (item.type == IniItem::directive) directives(item);
	};
	IniParser<decltype(processFn)> parser;
	int i = fn();
	while (i != -1) {
		parser(i);
		i = fn();
	}

}

template<typename D>
inline bool IniConfig::load(const std::string& pathname,const D& directives) {


	String prevPath = curPath;
	auto f1 = finally([&]{curPath = prevPath;});
	std::string dummy;

	std::ifstream input(pathname);
	if (!input) return false;
	auto sep = pathname.find(pathSeparator);
	if (sep != pathname.npos) {
		StrViewA newpath = StrViewA(pathname).substr(0,sep+1);
		curPath = pool.add(newpath);
	}
	auto processFn = [this,directives](const IniItem &item) {
		if (item.type == IniItem::data)	this->load(item);
		else if (item.type == IniItem::directive) {
			if (item.key == "include") {
				Value v(item.value, curPath);
				if (this->load(v.getPath(),directives)) return;
			}
			directives(item);
		}
	};

	IniParser<decltype(processFn)> parser(processFn);
	int i = input.get();
	while (i != -1) {
		parser(i);
		i = input.get();
	}

	return true;
}


inline void IniConfig::load(const std::string& pathname) {
	load(pathname, [](const IniItem &){} );
}

inline std::size_t IniConfig::Value::getUInt() const {
	std::size_t x = 0;
	for (char c : v.getView()) {
		if (isdigit(c)) x = x * 10 + (c - '0');
		else break;
	}
	return x;
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

inline void IniConfig::load(const IniItem& item) {
	auto s = smap.find(item.section);
	KeyValueMap *kv;
	if (s == smap.end()) {
		String sname = pool.add(item.section);
		s = smap.insert(std::make_pair(sname, KeyValueMap(sname))).first;
	}
	kv = &s->second;
	String k = pool.add(item.key);
	String v = pool.add(item.value);
	Value vv;
	vv.p = curPath;
	vv.v = v;
	kv->insert(std::make_pair(k,vv));

}

}


#endif /* INI_CONFIG_H_ */
