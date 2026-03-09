// Constructors and Operators (Not implementations)
#include "bitstr.h"

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

/// WARNING: double is very unprecise, use string constructors for absulote precision
BitString::BitString(const double d)
    : BitString(to_string(d)) {}

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

BitString BitString::operator%(const BitString& other) const {
    return rem(*this, other);
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

