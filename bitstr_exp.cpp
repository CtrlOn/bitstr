// Exponential calculations
#include "bitstr.h"
#include "bigint.h"

#define SQRT_PRECISION 340 // number of accurate bits for default sqrt (= 99 decimal digits + guard bits)
#define LN_PRECISION 340 // number of accurate...

using namespace std;
using namespace BigInt;

const BitString BitString::LN_2 = {
    false,
    {
        0x1E4DF7DD,
        0x5DBD874D,
        0x9FA0F6D4,
        0x877C3B33,
        0xD2BE86E5,
        0x156554BE,
        0x1B7AEB26,
        0xF9EE1D88,
        0xE2EABE8A,
        0x628345D6,
        0x9CA62D8B,
        0xD03CD0C9,
        0x00FCBDAB,
        0xF278ECE6,
        0xF473DE6A,
        0x2C5C85FD,
    },
    -510
};

BitString BitString::pow(const BitString& n, int e) {
    if (e < 0) {
        return div(BitString(1), pow(n, -e));
    }
    BitString result(1);
    BitString base = n;
    while (e > 0) {
        if (e & 1) {
            result = mul(result, base);
        }
        base = mul(base, base);
        e >>= 1;
    }
    return result;
}

BitString BitString::ln(const BitString& n) {
    if (n.isZero() || n.sign) {
        throw domain_error("Log undefined for non-positive values");
    }
    // extract exponent
    int e = n.exponent;

    BitString m = n;
    m.exponent = 0;

    BitString one = BitString::fromString("1");

    // y = (m-1)/(m+1)
    BitString y = BitString::div(m - one, m + one, LN_PRECISION);
    BitString y2 = y * y;

    BitString term = y;
    BitString sum = term;

    int k = 3;

    while (true)
    {
        term = term * y2;

        BitString denom = BitString::fromString(std::to_string(k));
        BitString add = BitString::div(term, denom, LN_PRECISION);

        if (add.isZero())
            break;

        sum = sum + add;

        k += 2;
    }

    BitString ln_m = sum * BitString::fromString("2");


    BitString expTerm =
        BitString::fromString(std::to_string(e)) * LN_2;

    return ln_m + expTerm;
}

BitString BitString::sqrt(const BitString& n, int precision) {
    if (n.isZero()) return BitString(0);
    if (n.sign) throw domain_error("Square root undefined for negative values");

    int64_t initial2exp = (n.exponent + (int64_t)n.mantissa.size() * 32 - __builtin_clz(n.mantissa.back())) / 2;
    BitString x(1.5); // the legendary 'threehalfs'
    x.exponent += initial2exp;

    BitString half(0.5);

    while (true)
    {
        BitString newX = (x + div(n, x, precision + initial2exp)) * half;
        if (abs(newX - x) < BitString(0, {1}, -precision)) {
            break;
        }
        x = newX;
    }

    return x;
}

BitString BitString::sqrt(const BitString& n) {
    return sqrt(n, SQRT_PRECISION);
}