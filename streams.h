/*
 * streams.h
 *
 *  Created on: 30. 11. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_STREAMS_H_129031_23
#define ONDRA_SHARED_SRC_SHARED_STREAMS_H_129031_23

#include <istream>
#include <ostream>
#include <streambuf>
// for EOF:
#include <cstdio>
// for memmove():
#include <cstring>

namespace ondra_shared {

namespace _details {

template<typename Fn>
class output_adapter : public std::streambuf {
  protected:
    Fn fn;
  public:
    // constructor
    output_adapter (Fn &&fn) : fn(std::move(fn)) {}
  protected:
    virtual int_type overflow (int_type c) override {
    	if (c != EOF) {
    		fn((char)c);
    	}
		return c;
    }
};

template<typename Fn>
class input_adapter : public std::streambuf {
  protected:
    Fn fn;    // file descriptor
  public:
    input_adapter (Fn &&fn) : fn(std::move(fn)) {}
  protected:
    virtual int_type underflow () override {return fn();}
};

}

///std::ostream compatible stream, which writes bytes to a lambda function
/**
 * @tparam Fn type of function void(char)
 */
template<typename Fn>
class ostream : public std::ostream {
  protected:
    _details::output_adapter<Fn> buf;
  public:
    ostream (Fn &&fn) : std::ostream(0), buf(std::move(fn)) {
        rdbuf(&buf);
    }
};


///std::istream compatible stream, which reads bytes from a lambda function
/**
 * @tparam Fn type of function int(void)
 */
template<typename Fn>
class fdistream : public std::istream {
  protected:
    _details::input_adapter<Fn> buf;
  public:
    fdistream (Fn &&fn) : std::istream(0), buf(std::move(fn)) {
        rdbuf(&buf);
    }
};

}







#endif /* ONDRA_SHARED_SRC_SHARED_STREAMS_H_129031_23 */
