// Constructors and Operators (Not implementations)
#include "bitstr.h"

using namespace std;

BitString::BitString()
    : sign(false), mantissa(1,0), exponent(0) {}

/// TODO: private this
BitString::BitString(const bool sign, const vector<limb_t>& mantissa, int64_t exponent)
    : sign(sign), mantissa(mantissa), exponent(exponent) {}

BitString::BitString(const BitString& other)
    : sign(other.sign), mantissa(other.mantissa), exponent(other.exponent) {}

BitString::BitString(const string& str) {
    *this = fromString(str);
}

BitString::BitString(const string& str, int precision) {
    *this = fromString(str, precision);
}

BitString::BitString(const int n)
    : sign(false), mantissa(1, 0), exponent(0) {
    int64_t value = static_cast<int64_t>(n);
    if (value == 0) {
        return;
    }

    sign = value < 0;
    uint64_t mag = sign
        ? static_cast<uint64_t>(-(value + 1)) + 1ULL
        : static_cast<uint64_t>(value);

    mantissa.clear();
    if (limb_bits == 64) {
        mantissa.push_back(static_cast<limb_t>(mag));
    } else {
        const unsigned int chunkBits = limb_bits >= 63 ? 63U : static_cast<unsigned int>(limb_bits);
        const uint64_t limbMask = (1ULL << chunkBits) - 1ULL;
        while (mag != 0) {
            mantissa.push_back(static_cast<limb_t>(mag & limbMask));
            mag >>= chunkBits;
        }
    }

    if (mantissa.empty()) {
        mantissa.push_back(0);
        sign = false;
    }

    normalize();
}

BitString::BitString(const double d) {
    *this = fromString(doubleToString(d));
}

BitString::BitString(const double d, int precision) {
    *this = fromString(doubleToString(d), precision);
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

