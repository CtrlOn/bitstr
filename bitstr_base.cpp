// Base arithmetics and utils
#include "bitstr.h"
#include "bigint.h"

using namespace std;
using namespace BigInt;

/// Values must be normalized before going in pretty much everywhere else
/// WARNING: normalized means mantissa first element (least significant) is off, and last element is non-zero
void BitString::normalize() {
    // If became zero, reset sign and exponent
    if (isZero()) {
        sign = false;
        exponent = 0;
        return;
    }

    // Remove trailing zeros in 32 chunks
    while (mantissa[0] == 0 && mantissa.size() > 1) {
        mantissa.erase(mantissa.begin());
        exponent += 32;
    }

    // Remove remaining trailing zeros
    uint32_t low = mantissa[0];
    int tz = __builtin_ctz(low);
    if (tz > 0) {
        right_shift(mantissa, tz);
        exponent += tz;
    }

    // Remove leading zeros
    while (mantissa.size() > 1 && mantissa.back() == 0) {
        mantissa.pop_back();
    }
}

/// Remove until 'bits' bits remain
BitString BitString::truncate(int bits) const {
    BitString result = *this;

    int totalBits = result.mantissa.size() * 32;

    if (totalBits <= bits)
        return result;

    int removeBits = totalBits - bits;
    right_shift(result.mantissa, removeBits);
    result.exponent += removeBits;
    result.normalize();

    return result;
}

/// Compares absolute values (e.g. |-3| > |2|)
int BitString::compareMag(const BitString& a, const BitString& b) {
    bool aZero = a.isZero();
    bool bZero = b.isZero();
    if (aZero && bZero) return 0;
    if (aZero) return -1;
    if (bZero) return 1;

    int aLength = a.mantissa.size() * 32 + a.exponent - __builtin_clz(a.mantissa.back());
    int bLength = b.mantissa.size() * 32 + b.exponent - __builtin_clz(b.mantissa.back());

    if (aLength != bLength) return aLength < bLength ? -1 : 1;

    int expDiff = a.exponent - b.exponent;
    vector<uint32_t> aMant = a.mantissa;
    vector<uint32_t> bMant = b.mantissa;
    if (expDiff > 0) {
        // a has more leading zeros, shift it right
        left_shift(aMant, expDiff);
    } else if (expDiff < 0) {
        // b has more leading zeros, shift it right
        left_shift(bMant, -expDiff);
    }
    for (int i = (int)aMant.size() - 1; i >= 0; --i) {
        if (aMant[i] != bMant[i]) {
            return aMant[i] < bMant[i] ? -1 : 1;
        }
    }

    return 0;
}

/// Both sub and add here
BitString BitString::add(const BitString& l, const BitString& r) {
    BitString a = l;
    BitString b = r;
    // Align exponents to the smaller one by left‑shifting the larger
    if (a.exponent > b.exponent) {
        left_shift(a.mantissa, a.exponent - b.exponent);
        a.exponent = b.exponent;
    } else if (b.exponent > a.exponent) {
        left_shift(b.mantissa, b.exponent - a.exponent);
        b.exponent = a.exponent;
    }

    BitString result;
    result.exponent = a.exponent;

    if (a.sign == b.sign) {
        // Magnitude addition
        result.sign = a.sign;
        result.mantissa = BigInt::bigint_add(a.mantissa, b.mantissa);
    } else {
        // Magnitude subtraction
        int cmp = compareMag(a, b);
        if (cmp == 0) return BitString(); // a - a = 0
        const BitString* great = &a;
        const BitString* low  = &b;
        if (cmp < 0) swap(great, low);
        result.sign = great->sign;
        result.mantissa = BigInt::bigint_sub(great->mantissa, low->mantissa);
    }

    result.normalize();
    return result;
}

///TODO: this is O(n^2), migrate to Karatsuba later
BitString BitString::mul(const BitString& a, const BitString& b, int limitBits) {
    // If a limit is given (not INT_MAX), use truncated multiplication
    if (limitBits != INT_MAX) {                       // extra bits to preserve accuracy
        int targetBits = limitBits * 2.01f;
        BitString a_trunc = a.truncate(targetBits);
        BitString b_trunc = b.truncate(targetBits);
        // Recursively multiply with no further limit
        BitString result = mul(a_trunc, b_trunc, INT_MAX);
        // Finally truncate to the desired precision
        return result.truncate(limitBits);
    }

    // Original full multiplication (no limit)
    BitString result;
    result.sign = a.sign ^ b.sign;
    result.exponent = a.exponent + b.exponent;
    result.mantissa.assign(a.mantissa.size() + b.mantissa.size(), 0);

    for (size_t i = 0; i < a.mantissa.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.mantissa.size(); ++j) {
            uint64_t cur = result.mantissa[i + j] +
                           (uint64_t)a.mantissa[i] * b.mantissa[j] + carry;
            result.mantissa[i + j] = (uint32_t)cur;
            carry = cur >> 32;
        }
        result.mantissa[i + b.mantissa.size()] += carry;
    }

    result.normalize();
    return result;
}

BitString BitString::mul(const BitString& a, const BitString& b) {
    return mul(a, b, INT_MAX);
}

BitString BitString::fact(const uint32_t n) {
    BitString result(1);
    int i = n;
    while (i > 1) {
        BigInt::bigint_mul_int(result.mantissa, i--);
    }
    return result;
}

BitString BitString::abs(const BitString& x) {
    BitString result = x;
    result.sign = false;
    return result;
}


bool BitString::isZero() const {
    return mantissa.size() == 1 && mantissa[0] == 0;
}

bool BitString::isOne() const {
    return !sign && exponent == 0 && mantissa.size() == 1 && mantissa[0] == 1;
}