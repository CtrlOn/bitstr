// Division and Modulo
#include "bitstr.h"
#include "bigint.h"

#define DIV_PRECISION 340 // How many binary digits to use for division (99 decimal digits + guard bits)

using namespace std;
using namespace BigInt;

// Determines if should round up based on the quotient bits
static bool shouldRoundUp(const vector<uint32_t>& quotient, int discardBits) {
    if (discardBits <= 0) return false;
    
    int roundBitPos = discardBits - 1;
    int roundWord = roundBitPos / 32;
    int roundShift = roundBitPos % 32;
    
    if (roundWord >= (int)quotient.size()) return false;
    
    bool roundBit = (quotient[roundWord] >> roundShift) & 1;
    if (!roundBit) return false;
    
    // Check if there are any lower bits set (means > 1/2)
    uint32_t lowMask = quotient[roundWord] & ((1U << roundShift) - 1);
    for (int i = 0; i < roundWord; ++i) {
        if (quotient[i] != 0) return true;
    }
    if (lowMask != 0) return true;
    
    // Tiebreaker - round to even
    int lastKeptWord = discardBits / 32;
    int lastKeptShift = discardBits % 32;
    if (lastKeptWord < (int)quotient.size()) {
        return (quotient[lastKeptWord] >> lastKeptShift) & 1;
    }
    return false;
}

BitString BitString::div(const BitString& a, const BitString& b, int precision) {
    if (b.isZero()) throw domain_error("Division by zero");
    if (a.isZero()) return BitString();
    if (b.isOne()) return a;

    bool sign = a.sign ^ b.sign;

    // Work with absolute mantissas
    vector<uint32_t> aMantissa = a.mantissa;
    vector<uint32_t> bMantissa = b.mantissa;

    // Choose p so that the quotient has at least precision+1 bits.
    int p = precision + bit_length(bMantissa);

    vector<uint32_t> dividend = aMantissa;
    left_shift(dividend, p);

    vector<uint32_t> quotient, remainder;
    bigint_div(dividend, bMantissa, quotient, remainder);

    int quotientBits = bit_length(quotient);
    int discardBits = quotientBits - precision;

    // Extract the highest precision bits
    vector<uint32_t> mantissa = quotient;
    right_shift(mantissa, discardBits);

    if (shouldRoundUp(quotient, discardBits)) {
        uint64_t carry = 1;
        for (size_t i = 0; i < mantissa.size() && carry; ++i) {
            uint64_t sum = uint64_t(mantissa[i]) + carry;
            mantissa[i] = (uint32_t)sum;
            carry = sum >> 32;
        }
        if (carry) mantissa.push_back((uint32_t)carry);
        
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
    return div(a, b, a.exponent - b.exponent + bit_length(a.mantissa) - bit_length(b.mantissa) + DIV_PRECISION);
}

BitString BitString::mod(const BitString& a, const BitString& b) {
    if (b.isZero())
        throw std::domain_error("Modulo by zero");

    // Remainder takes the sign of the dividend
    bool resultSign = a.sign;

    // Work with absolute values
    BitString absA = a;
    BitString absB = b;
    absA.sign = false;
    absB.sign = false;

    // Align exponents to the smaller one so both become integers
    int64_t minExp = std::min(absA.exponent, absB.exponent);
    std::vector<uint32_t> aMant = absA.mantissa;
    std::vector<uint32_t> bMant = absB.mantissa;

    if (absA.exponent > minExp)
        left_shift(aMant, (int)(absA.exponent - minExp));
    if (absB.exponent > minExp)
        left_shift(bMant, (int)(absB.exponent - minExp));

    // Compute integer remainder
    std::vector<uint32_t> quot, rem;
    bigint_div(aMant, bMant, quot, rem);

    BitString result(resultSign, rem, minExp);
    result.normalize();
    return result;
}