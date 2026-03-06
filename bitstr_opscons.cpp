// Constructors and Operators (Not implementations)
#include "bitstr.h"
#include <cstring>

using namespace std;

BitString::BitString()
    : sign(false), mantissa(1,0), exponent(0) {}

/// TODO: private this
BitString::BitString(const bool sign, const vector<uint32_t>& mantissa, int64_t exponent)
    : sign(sign), mantissa(mantissa), exponent(exponent) {}

BitString::BitString(const BitString& other)
    : sign(other.sign), mantissa(other.mantissa), exponent(other.exponent) {}

BitString::BitString(const string& str) {
    *this = fromString(str);
}

BitString::BitString(const int32_t n)
    : sign(n < 0), mantissa(1, (uint32_t)(sign ? -n : n)), exponent(0) {
    normalize();
}

BitString::BitString(const double d) {
    // reinterpret bits so we can extract sign, exponent and mantissa exactly
    uint64_t bits;
    memcpy(&bits, &d, sizeof(double));

    sign = (bits >> 63) != 0; // first bit sign
    uint64_t expRaw = (bits >> 52) & 0x7FFULL; // next 11 bits exponent
    uint64_t mant = bits & 0xFFFFFFFFFFFFFULL; // last 52 bits mantissa

    if (expRaw == 0) {
        // zero or subnormal
        if (mant == 0) {
            // exactly zero
            *this = BitString();
            return;
        }
        exponent = -1022 - 52;
    } else if (expRaw == 0x7FFULL) {
        // Inf or NaN
        throw invalid_argument("NaN and infinity not supported");
    } else {
        // implicit leading 1
        mant |= (1ULL << 52);
        // unbiased exponent
        int64_t e = (int64_t)expRaw - 1023;
        exponent = e - 52; // because mantissa represents an integer with 52 fractional bits
    }

    uint32_t low = (uint32_t)(mant & 0xFFFFFFFFULL);
    uint32_t high = (uint32_t)(mant >> 32);
    mantissa.clear();
    mantissa.push_back(low);
    if (high != 0) {
        mantissa.push_back(high);
    }

    normalize();
}

BitString& BitString::operator=(const BitString& other) {
    if (this != &other) {
        sign = other.sign;
        mantissa = other.mantissa;
        exponent = other.exponent;
    }
    return *this;
}

BitString BitString::operator+(const BitString& other) const {
    return add(*this, other);
}

BitString BitString::operator-() const {
    BitString result=*this;
    if (!result.isZero()) result.sign=!result.sign;
    return result;
}

BitString BitString::operator-(const BitString& other) const {
    return *this + (-other);
}

BitString BitString::operator*(const BitString& other) const {
    return mul(*this, other);
}

/// WARNING - hardcoded precision, use div(a,b,precision) for more control
BitString BitString::operator/(const BitString& other) const {
    return div(*this, other);
}

// Left/Right shifts are procedural, no ops for them.

bool BitString::operator==(const BitString& other) const {
    return sign==other.sign && exponent==other.exponent && mantissa==other.mantissa;
}

bool BitString::operator!=(const BitString& o) const {
    return !(*this==o);
}

bool BitString::operator<(const BitString& o) const {
    if (sign!=o.sign) return sign;

    int c = compareMag(*this,o);
    return sign ? c > 0 : c < 0;
}

bool BitString::operator<=(const BitString& o) const {
    return !(*this>o);
}

bool BitString::operator>(const BitString& o) const {
    return o<*this;
}

bool BitString::operator>=(const BitString& o) const {
    return !(*this<o);
}

