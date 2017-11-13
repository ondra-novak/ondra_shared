/** @file Some function to fast convert numbers to strings (not using locales) */

#ifndef _ONDRA_SHARED_TOSTRING_H_39289204239042_
#define _ONDRA_SHARED_TOSTRING_H_39289204239042_

#include <math.h>

namespace ondra_shared {

template<typename Number, typename Fn>
void unsignedToString(const Number &n, const Fn &fn, int base=10, int leftZeroes=1) {

	if (n == 0 && leftZeroes<1) return;
	unsignedToString(n/base, fn, base, leftZeroes-1);
	unsigned int remainder = (unsigned int)(n % base);
	if (remainder < 10) fn(remainder+'0');
	else if (remainder < 36) fn(remainder+'A'-10);
	else fn(remainder+'a'-36);
}

template<typename Number, typename Fn>
void signedToString(const Number &n, const Fn &fn, int base=10, int leftZeroes=1) {

	if (n < 0) {
		fn('-');
		unsignedToString(-n,fn,base,leftZeroes);
	} else {
		unsignedToString(n,fn,base,leftZeroes);
	}
}



template<typename Number, typename Fn>
void floatToString(const Number &value, const Fn &fn, int maxPrecisionDigits=8) {
	//calculate exponent of value
	//123897 -> 5 (1.23897e5)
	//0.001248 -> 3 (1.248e-3)
	double fexp = floor(log10(fabs(value)));
	//convert it to integer
	std::intptr_t iexp = (std::intptr_t)fexp;
	//if exponent is in some reasonable range, set iexp to 0
	if (iexp > -3 && iexp < 8) {
		iexp = 0;
	}
	else {
		//otherwise normalize number to be between 1 and 10
		value = value * pow(0.1, iexp);
	}
	double fint;
	//separate number to integer and fraction
	double frac = std::abs(modf(value, &fint));
	//write signum for negative number
	if (value < 0) fn('-');
	//write absilute integer number (remove sign)
	unsignedToString(std::uintptr_t(fabs(fint)), fn, 10,1);
	//if frac is not zero (exactly)
	if (frac != 0.0) {

		double fractMultiply = pow(10, maxPrecisionDigits);
		std::uintptr_t digits = maxPrecisionDigits;
		//multiply fraction by maximum fit to integer
		std::uintptr_t m = (std::uintptr_t)floor(frac * fractMultiply +0.5);

		if (m) {
			//put dot
			fn('.');
			//remove any rightmost zeroes
			while (m && (m % 10) == 0) {m = m / 10;--digits;}
			//write final number
			unsignedToString(m, fn, 10,digits);
		}
	}
	//if exponent is set
	if (iexp) {
		//put E
		fn('e');
		if (iexp > 0) fn('+');
		//write signed exponent
		signedToString(iexp,fn);
	}
	//all done
}

}


#endif
