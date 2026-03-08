// Exponential calculations
#include "bitstr.h"
#include "bigint.h"

#define SQRT_PRECISION 340 // number of accurate bits for default sqrt (= 99 decimal digits + guard bits)

using namespace std;
using namespace BigInt;

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
    return BitString(); // TODO:
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