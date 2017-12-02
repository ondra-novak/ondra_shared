/*
 * ini_config.h
 *
 *  Created on: 25. 11. 2017
 *      Author: ondra
 */

#ifndef _ONDRA_SHARED_INI_CONFIG_23123148209810_
#define _ONDRA_SHARED_INI_CONFIG_23123148209810_

namespace ondra_shared {


class IniConfig {
public:


	typedef StringPool<char> StrPool;
	typedef StrPool::String String;

	struct Value {
		String v;
		String p;

		Value(String v,String p):v(v),p(p) {}
		std::string getPath() const;
		std::size_t getUInt() const;
		std::intptr_t getInt() const;
		StrViewA getString() const {return v.getView();}
		const char *c_str() const {return v.getView().data;}
	};

	typedef std::map<String, Value> KeyValueMap;
	typedef std::map<String, KeyValueMap> SectionMap;


	const SectionMap &operator[](const StrViewA &sectionName) const;
	SectionMap::const_iterator begin() const;
	SectionMap::const_iterator end() const;


	template<typename Fn, typename D>
	void load(const Fn &fn, StrViewA path, const D &directives);

	template<typename D>
	bool load(const std::string &pathname, const D &directives);

	void load(const std::string &pathname);


	void load(const IniItem &item);

	IniConfig() {}
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
	s.reserve(v.length + p.length);
	s.append(p.data,p.length);
	s.append(v.data,v.length);
	return s;
}

inline const SectionMap& IniConfig::operator [](const StrViewA& sectionName) const {
	auto itr = smap.find(sectionName);
	if (itr == smap.end()) {
		return emptyMap;
	} else{
		return itr->second;
	}
}

inline SectionMap::const_iterator IniConfig::begin() const {
	return smap.begin();
}

inline SectionMap::const_iterator IniConfig::end() const {
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
		StrViewA newpath = StrViewA(pathname).substr(0,sep);
		if (newpath[0] == '/') {
			curPath = pool.add(newpath);
		} else {
			dummy.resize(curPath.length+newpath.length);
			dummy.append(curPath.data,curPath.length);
			dummy.append(newpath.data, newpath.length);
			curPath = pool.add(dummy);
		}
	}
	auto processFn = [this,directives](const IniItem &item) {
		if (item.type == IniItem::data)	this->load(item);
		else if (item.type == IniItem::directive) {
			if (item.key == "include") {
				Value v;
				v.p = curPath;
				v.v = item.value;
				if (this->load(v.getPath(),directives)) return;
			}
			directives(item);
		}
	};

	IniParser<decltype(processFn)> parser;
	int i = input.get();
	while (i != -1) {
		parser(i);
		i = input.get();
	}

	return true;
}


inline void IniConfig::load(const std::string& pathname) {
	load(pathname, [](IniItem &){} );
}

inline std::size_t IniConfig::Value::getUInt() const {
	std::size_t x = 0;
	for (char c : v) {
		if (isdigit(c)) x = x * 10 + (c - '0');
		else break;
	}
	return x;
}

inline std::intptr_t IniConfig::Value::getInt() const {
	if (v.empty()) return 0;
	if (v[0] == '-') {
		Value z;
		z.v = v.substr(1);
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
		kv = &smap[sname];
	} else {
		kv = &s->second;
	}
	String k = pool.add(item.key);
	String v = pool.add(item.value);
	Value vv;
	vv.p = curPath;
	vv.v = v;
	(*kv)[k] = vv;

}

}


#endif /* INI_CONFIG_H_ */
