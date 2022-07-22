/*
 * signals.h
 *
 *  Created on: 20. 7. 2022
 *      Author: ondra
 */

#ifndef SRC_LIBS_SHARED_SIGNALS_H_129sopiqwjs12098ew1jisj1sqwdwq[pqod
#define SRC_LIBS_SHARED_SIGNALS_H_129sopiqwjs12098ew1jisj1sqwdwq[pqod
#include <vector>
#include "move_only_function.h"
#include "fastparam.h"

namespace ondra_shared {


template<typename T> class Signal;



///Works as function, but calls connected functions
/**
 * @tparam Args arguments of signals
 *
 * @note functions can't return a value. They only returns bool to stay connected
 * or disconnect
 *
 * @retval true stay connected
 * @retval false disconnect
 */
template<typename ... Args>
class Signal<bool(Args...)> {
public:

    using Fn = move_only_function<bool(Args...) noexcept>;
    using Connection = const void *;

    ///Construct empty collector
    Signal();
    ///Move constructor
    Signal(Signal &&other):fns(std::move(other.fns)) {}
    ///Assign operator
    Signal &operator=(Signal &&other) {
        if (this != &other) {
            fns = std::move(other.fns);
        }
        return *this;
    }

    ///Connects new slot
    /**
     * @param fn function to connect
     * @return connection identifier. Can be stored to able disconnect function later.
     * Can be also ignored, if disconnection is never required or it is
     * handled differently
     */
    Connection connect(Fn &&fn) {
        Connection out = fn.get_ident();
        fns.push_back(std::move(fn));
        return out;
    }

    ///Disconnects connected slot
    /**
     * @param con connection identifier
     * @retval true disconnected
     * @retval false not found
     */
    bool disconnect(Connection con) {
        auto iter = std::remove_if(fns.begin(), fns.end(), [&](const Fn &f) {
           return f.get_ident() == con;
        });
        if (iter != fns.end()) {
            fns.erase(iter, fns.end());
            return true;
        } else {
            return false;
        }
    }

    ///Send signal
    /***
     * @param args arguments of signal
     * @retval true some function is still connected
     * @retval false no connections
     **/
    bool send(FastParamT<Args> ... args) const {
        auto iter = fns.begin();
        while (iter != fns.end()) {
            bool r = (*iter)(std::forward<FastParamT<Args> >(args)...);
            if (!r) iter = fns.erase(iter);
            else ++iter;
        }
        return !fns.empty();
    }


    ///Send signal
    /***
     * @param args arguments of signal
     * @retval true some function is still connected
     * @retval false no connections
     **/
    bool operator()(FastParamT<Args> ... args) const {
        return send(std::forward<FastParamT<Args> >(args)...);
    }

    ///Determines whether connection is still established
    /**
     * @param conn connection identification
     * @retval true established
     * @retval false disconnected
     */
    bool is_connected(Connection conn) {
        auto iter = std::find_if(fns.begin(), fns.end(),[&](const Fn &fn) {
            return fn.get_ident() == conn;
        });
        return iter != fns.end();
    }

    bool empty() const {return fns.empty();}
    auto size() const {return fns.size();}
    void clear() {fns.clear();}

protected:

    std::vector<Fn> fns;
};



}





#endif /* SRC_LIBS_SHARED_SIGNALS_H_129sopiqwjs12098ew1jisj1sqwdwq[pqod */
