// Division and Modulo
#include "bitstr.h"
#include "bigint.h"

#define DIV_PRECISION 448 // default target precision bits for arithmetic results

using namespace std;
using namespace BigInt;

// Determines if should round up based on the quotient bits
static bool shouldRoundUp(const vector<limb_t>& quotient, int discardBits) {
    if (discardBits <= 0) return false;
    
    int roundBitPos = discardBits - 1;
    int roundWord = roundBitPos / limb_bits;
    int roundShift = roundBitPos % limb_bits;
    
    if (roundWord >= (int)quotient.size()) return false;
    
    bool roundBit = (quotient[roundWord] >> roundShift) & 1;
    if (!roundBit) return false;
    
    // Check if there are any lower bits set (means > 1/2)
    limb_t lowMask = 0;
    if (roundShift > 0) {
        const wide_limb_t mask = (wide_limb_t(1) << roundShift) - 1;
        lowMask = quotient[roundWord] & static_cast<limb_t>(mask);
    }
    for (int i = 0; i < roundWord; ++i) {
        if (quotient[i] != 0) return true;
    }
    if (lowMask != 0) return true;
    
    // Tiebreaker - round to even
    int lastKeptWord = discardBits / limb_bits;
    int lastKeptShift = discardBits % limb_bits;
    if (lastKeptWord < (int)quotient.size()) {
        return (quotient[lastKeptWord] >> lastKeptShift) & 1;
    }
    return false;
}

BitString BitString::div(const BitString& a, const BitString& b, int precision) {
    if (b.isZero()) throw domain_error("Division by zero");
    if (a.isZero()) return BitString();
    if (b.isOne()) return a;

    // Fast path: divisor is exactly +/-2^k, so division is only exponent adjustment.
    if (b.mantissa.size() == 1 && b.mantissa[0] == 1) {
        BitString out = a;
        out.sign = a.sign ^ b.sign;
        out.exponent = a.exponent - b.exponent;
        out.normalize();
        return out;
    }

    bool sign = a.sign ^ b.sign;

    // Work with absolute mantissas
    vector<limb_t> aMantissa = a.mantissa;
    vector<limb_t> bMantissa = b.mantissa;

    // Choose p so that the quotient has at least precision+1 bits.
    int p = precision + bit_length(bMantissa);

    vector<limb_t> dividend = aMantissa;
    left_shift(dividend, p);

    vector<limb_t> quotient, remainder;
    bigint_div(dividend, bMantissa, quotient, remainder);

    int quotientBits = bit_length(quotient);
    int discardBits = quotientBits - precision;

    // Extract the highest precision bits
    vector<limb_t> mantissa = quotient;
    right_shift(mantissa, discardBits);

    if (shouldRoundUp(quotient, discardBits)) {
        wide_limb_t carry = 1;
        for (size_t i = 0; i < mantissa.size() && carry; ++i) {
            wide_limb_t sum = wide_limb_t(mantissa[i]) + carry;
            mantissa[i] = static_cast<limb_t>(sum);
            carry = sum >> limb_bits;
        }
        if (carry) mantissa.push_back(static_cast<limb_t>(carry));
        
        // If rounding caused overflow, shift right and adjust exponent
        int newBits = bit_length(mantissa);
        if (newBits > precision) {
            right_shift(mantissa, 1);
            discardBits += 1;
        }
    }

    // Remove possible leading zeros
    while (mantissa.size() > 1 && mantissa.back() == 0) {
        mantissa.pop_back();
    }

    // Result exponent: a.exponent - b.exponent - p + discardBits
    int64_t exp = a.exponent - b.exponent - p + discardBits;

    BitString result(sign, mantissa, exp);
    result.normalize();
    return result;
}

/// Smart auto precision
BitString BitString::div(const BitString& a, const BitString& b) {
    // Magnitude-aware default: tiny quotients need extra bits to keep decimal output stable.
    const int aMagBits = bit_length(a.mantissa) + (int)a.exponent;
    const int bMagBits = bit_length(b.mantissa) + (int)b.exponent;
    const int scaleGap = max(0, bMagBits - aMagBits);

    int precision = DIV_PRECISION + scaleGap + 16;
    if (precision < DIV_PRECISION) precision = DIV_PRECISION;
    if (precision > 4096) precision = 4096;

    return div(a, b, precision);
}

BitString BitString::rem(const BitString& a, const BitString& b) {
    if (b.isZero())
        throw domain_error("Remainder of zero");

    // Remainder takes the sign of the dividend
    bool resultSign = a.sign;

    // Work with absolute values
    BitString absA = a;
    BitString absB = b;
    absA.sign = false;
    absB.sign = false;

    // Align exponents to the smaller one so both become integers
    int64_t minExp = min(absA.exponent, absB.exponent);
    vector<limb_t> aMant = absA.mantissa;
    vector<limb_t> bMant = absB.mantissa;

    if (absA.exponent > minExp)
        left_shift(aMant, (int)(absA.exponent - minExp));
    if (absB.exponent > minExp)
        left_shift(bMant, (int)(absB.exponent - minExp));

    // Compute integer remainder
    vector<limb_t> quot, rem;
    bigint_div(aMant, bMant, quot, rem);

    BitString result(resultSign, rem, minExp);
    result.normalize();
    return result;
}

BitString BitString::mod(const BitString& a, const BitString& b) {
    if (b.isZero())
        throw domain_error("Modulo by zero");

    BitString r = rem(a, b);
    if (r.sign != b.sign && !r.isZero()) {
        r = r + b;
    }
    return r;
}

