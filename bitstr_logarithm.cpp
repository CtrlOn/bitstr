// Logarithm calculations
#include "bitstr.h"
#include "bigint.h"

#define LN_PRECISION 340 // number of accurate bits on resulting log

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

///FIXME: beyond this point downwards


static BitString arctanh_series(const BitString& t, int precision) {

    BitString t2 = BitString::mul(t, t, precision * 1.5f);
    BitString term = t;
    BitString sum = term;
    int n = 1;
    BitString epsilon(0, {1}, -precision * 1.5f);

    while (BitString::abs(term) > BitString::abs(sum) * epsilon) {
        term = BitString::mul(term, t2, precision * 1.5f);
        n += 2;
        BitString term_div_n = BitString::div(term, BitString(n), precision * 1.5f);
        sum = sum + term_div_n;
    }
    return sum;
}

/// Compute ln(m) for m in [1,2)
static BitString ln_mantissa(const BitString& m, int precision) {
    BitString one(1);
    BitString t = BitString::div(m - one, m + one, precision * 1.5f); // (m-1)/(m+1)
    BitString sum = arctanh_series(t, precision);
    return BitString::mul(BitString(2), sum, precision * 1.5f);
}

BitString BitString::ln(const BitString& n, int precision) {
    if (n.sign || n.isZero()) {
        throw std::domain_error("ln of non-positive number");
    }
    if (n.isOne()) return BitString(0);

    int L = n.mantissa.size() * 32 - __builtin_clz(n.mantissa.back()); // total bits in mantissa
    int64_t k = n.exponent + L - 1;
    BitString m(n.sign, n.mantissa, -(L-1));
    m.normalize();

    // Clamp m to [1,2) – correct any off-by-one due to rounding
    const BitString one(1);
    const BitString two(2);
    while (m >= two) {
        m = BitString::div(m, two, precision * 2); // m /= 2
        k += 1;
    }
    while (m < one) {
        m = BitString::mul(m, two, precision * 2); // m *= 2
        k -= 1;
    }

    // Compute ln(m)
    BitString ln_m = ln_mantissa(m, precision);

    // Use precomputed high‑precision LN_2 constant
    BitString k_bs((double)k);
    BitString k_term = BitString::mul(LN_2, k_bs);
    BitString result = ln_m + k_term;

    result.normalize();
    return result.truncate(precision);
}

BitString BitString::ln(const BitString& n) {
    return ln(n, LN_PRECISION*2);
}