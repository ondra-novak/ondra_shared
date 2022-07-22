/** @file Some function to fast convert numbers to strings (not using locales) */

#ifndef _ONDRA_SHARED_TOSTRING_H_39289204239042_
#define _ONDRA_SHARED_TOSTRING_H_39289204239042_

#include <cmath>

namespace ondra_shared {

template<typename Number, typename Fn>
void unsignedToString(const Number &n, Fn &&fn, int base=10, int leftZeroes=1) {

     if (n == 0 && leftZeroes<1) return;
     unsignedToString(n/base, fn, base, leftZeroes-1);
     unsigned int remainder = (unsigned int)(n % base);
     if (remainder < 10) fn(remainder+'0');
     else if (remainder < 36) fn(remainder+'A'-10);
     else fn(remainder+'a'-36);
}

template<typename Number, typename Fn>
void signedToString(const Number &n, Fn &&fn, int base=10, int leftZeroes=1) {

     if (n < 0) {
          fn('-');
          unsignedToString(-n,fn,base,leftZeroes);
     } else {
          unsignedToString(n,fn,base,leftZeroes);
     }
}



template<typename Number, typename Fn>
void floatToString(Number value, Fn &&fn, int maxPrecisionDigits=8) {
     static std::uintptr_t fracMultTable[10] = {
                    1, //0
                    10, //1
                    100, //2
                    1000, //3
                    10000, //4
                    100000, //5
                    1000000, //6
                    10000000, //7
                    100000000, //8
                    1000000000 //9
          };
     const char *inf = "∞";

          bool sign = value < 0;
          std::uintptr_t precisz = std::min<std::uintptr_t>(maxPrecisionDigits, 9);

          value = std::abs(value);
          //calculate exponent of value
          //123897 -> 5 (1.23897e5)
          //0.001248 -> 3 (1.248e-3)
          double fexp = floor(log10(std::abs(value)));

          if (!std::isfinite(fexp)) {
               if (fexp < 0) {
                    fn('0');
               } else {
                    if (sign) fn('-');
                    const char *z = inf;
                    while (*z) fn(*z++);
               }
               return;
          }

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
          double frac = modf(value, &fint);
          //calculate multiplication of fraction
          std::uintptr_t fractMultiply = fracMultTable[precisz];
          //multiplicate fraction to receive best rounded number to given decimal places
          double fm = floor(frac * fractMultiply +0.5);

          //convert finteger to integer
          std::uintptr_t intp(fint);
          //mantisa as integer number (without left zeroes)
          std::uintptr_t m(fm);


          //if floating multiplied fraction is above or equal fractMultiply
          //something very bad happen, probably rounding error happened, so we need adjust
          if (m >= fractMultiply) {
               //increment integer part number
               intp = intp+1;
               //decrease fm
               m-=fractMultiply;
               //if integer part is equal or above 10
               if (intp >= 10 && iexp) {
                    //set  integer part to 1 (because 9.99999 -> 10 -> 1e1)
                    intp=1;
                    //increase exponent
                    iexp++;
               }
          }

          //write signum for negative number
          if (sign) fn('-');
          //write absolute integer number (remove sign)
          unsignedToString(intp,fn);
          std::uintptr_t digits = precisz;

          if (m) {
               //put dot
               fn('.');
               //remove any rightmost zeroes
               while (m && (m % 10) == 0) {m = m / 10;--digits;}
               //write final number
               unsignedToString(m,fn,10, digits);
          }
          //if exponent is set
          if (iexp) {
               //put E
               fn('e');
               if (iexp > 0) fn('+');
               //write signed exponent
               signedToString(iexp,fn,10);
          }
          //all done
}

}


#endif
