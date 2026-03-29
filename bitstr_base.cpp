// Base arithmetics and utils
#include "bitstr.h"
#include "bigint.h"

using namespace std;
using namespace BigInt;

const BitString BitString::ONE = BitString(false, {1}, 0);
const BitString BitString::TWO = BitString(false, {1}, 1);

/// Values must be normalized before going in pretty much everywhere else
/// WARNING: normalized means mantissa first element (least significant) is off, and last element is non-zero
void BitString::normalize() {
    // If became zero, reset sign and exponent
    if (isZero()) {
        sign = false;
        exponent = 0;
        return;
    }

    // Remove trailing zero limbs with one erase to avoid repeated O(n) shifts.
    size_t lowZeroLimbs = 0;
    while (lowZeroLimbs + 1 < mantissa.size() && mantissa[lowZeroLimbs] == 0) {
        ++lowZeroLimbs;
    }
    if (lowZeroLimbs > 0) {
        mantissa.erase(mantissa.begin(), mantissa.begin() + static_cast<ptrdiff_t>(lowZeroLimbs));
        exponent += static_cast<int64_t>(lowZeroLimbs) * limb_bits;
    }

    // Remove remaining trailing zeros
    limb_t low = mantissa[0];
    int tz = limb_ctz(low);
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

    int totalBits = static_cast<int>(result.mantissa.size()) * limb_bits;

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

    int aLength = bit_length(a.mantissa) + static_cast<int>(a.exponent);
    int bLength = bit_length(b.mantissa) + static_cast<int>(b.exponent);

    if (aLength != bLength) return aLength < bLength ? -1 : 1;

    const int64_t minExp = min(a.exponent, b.exponent);
    const unsigned int aShift = static_cast<unsigned int>(a.exponent - minExp);
    const unsigned int bShift = static_cast<unsigned int>(b.exponent - minExp);

    const size_t aSize = BigInt::shifted_size(a.mantissa, aShift);
    const size_t bSize = BigInt::shifted_size(b.mantissa, bShift);
    const size_t maxSize = max(aSize, bSize);

    for (int i = static_cast<int>(maxSize) - 1; i >= 0; --i) {
        const limb_t aw = BigInt::shifted_limb_at(a.mantissa, aShift, static_cast<size_t>(i));
        const limb_t bw = BigInt::shifted_limb_at(b.mantissa, bShift, static_cast<size_t>(i));
        if (aw != bw) {
            return aw < bw ? -1 : 1;
        }
    }

    return 0;
}

void BitString::addTo(BitString& acc, const BitString& x) {
    if (x.isZero()) return;
    if (acc.isZero()) {
        acc = x;
        return;
    }

    vector<limb_t> shifted;
    const vector<limb_t>* xMantissa = &x.mantissa;

    // Align exponents to the smaller one by left-shifting the larger.
    if (acc.exponent > x.exponent) {
        left_shift(acc.mantissa, static_cast<unsigned int>(acc.exponent - x.exponent));
        acc.exponent = x.exponent;
    } else if (x.exponent > acc.exponent) {
        shifted = x.mantissa;
        left_shift(shifted, static_cast<unsigned int>(x.exponent - acc.exponent));
        xMantissa = &shifted;
    }

    if (acc.sign == x.sign) {
        BigInt::bigint_add_inplace(acc.mantissa, *xMantissa);
    } else {
        const int cmp = BigInt::bigint_cmp(acc.mantissa, *xMantissa);
        if (cmp == 0) {
            acc = BitString();
            return;
        }
        if (cmp > 0) {
            BigInt::bigint_sub_inplace(acc.mantissa, *xMantissa);
        } else {
            vector<limb_t> next = *xMantissa;
            BigInt::bigint_sub_inplace(next, acc.mantissa);
            acc.mantissa.swap(next);
            acc.sign = x.sign;
        }
    }

    acc.normalize();
}

/// Both sub and add here
BitString BitString::add(const BitString& l, const BitString& r) {
    BitString result = l;
    addTo(result, r);
    return result;
}

///TODO: this is O(n^2), migrate to Karatsuba later
/// limitBits is all bits, not just fractional bits!
BitString BitString::mul(const BitString& a, const BitString& b, int limitBits) {
    if (limitBits != INT_MAX) {
        int targetBits = limitBits * 2.01f;
        BitString a_trunc = a.truncate(targetBits);
        BitString b_trunc = b.truncate(targetBits);
        // Recursively multiply with no further limit
        BitString result = mul(a_trunc, b_trunc, INT_MAX);
        // Finally truncate to the desired precision
        return result.truncate(limitBits);
    }
    // full multiplication
    BitString result;
    result.sign = a.sign ^ b.sign;
    result.exponent = a.exponent + b.exponent;
    result.mantissa.assign(a.mantissa.size() + b.mantissa.size(), 0);

    for (size_t i = 0; i < a.mantissa.size(); ++i) {
        wide_limb_t carry = 0;
        for (size_t j = 0; j < b.mantissa.size(); ++j) {
            wide_limb_t cur = result.mantissa[i + j] +
                              wide_limb_t(a.mantissa[i]) * b.mantissa[j] + carry;
            result.mantissa[i + j] = static_cast<limb_t>(cur);
            carry = cur >> limb_bits;
        }
        result.mantissa[i + b.mantissa.size()] += static_cast<limb_t>(carry);
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
        BigInt::bigint_mul_limb(result.mantissa, static_cast<limb_t>(i--));
    }
    return result;
}

BitString BitString::abs(const BitString& x) {
    BitString result = x;
    result.sign = false;
    return result;
}

bool BitString::isZero() const {
    if (mantissa.size() == 1 && mantissa[0] == 0) return true;
    for (limb_t w : mantissa) {
        if (w != 0) return false;
    }
    return true;
}

bool BitString::isOne() const {
    return !sign && exponent == 0 && mantissa.size() == 1 && mantissa[0] == 1;
}