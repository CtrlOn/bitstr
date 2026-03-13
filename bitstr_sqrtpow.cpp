// Exponential calculations
#include "bitstr.h"
#include "bigint.h"

#define SQRT_PRECISION 384 // default target precision bits for sqrt

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

BitString BitString::sqrt(const BitString& n, int precision) {
    if (n.isZero()) return BitString();
    if (n.sign) throw domain_error("Square root undefined for negative values");

    const int totalBits = bit_length(n.mantissa);
    int64_t initial2exp = (n.exponent + (int64_t)totalBits) / 2;

    // Better initial guess than 1.5: midpoint of sqrt([1,2)) narrows Newton steps.
    BitString x("1.2071067811865475");
    x.exponent += initial2exp;

    BitString half("0.5");
    const BitString eps(0, {1}, -precision);
    const int maxIter = (precision / 8) + 12;

    for (int iter = 0; iter < maxIter; ++iter) {
        BitString newX = (x + div(n, x, precision + limb_bits)) * half;
        if (abs(newX - x) < eps) {
            x = newX;
            break;
        }
        x = newX;
    }

    return x;
}

BitString BitString::sqrt(const BitString& n) {
    return sqrt(n, SQRT_PRECISION);
}