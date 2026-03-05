// Base arithmetics and utils
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

/// Values must be normalized before going in pretty much everywhere else
void BitString::normalize() {
    // Remove leading zeros
    while (mantissa.size() > 1 && mantissa.back() == 0)
        mantissa.pop_back();

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
        rightShift(mantissa, tz);
        exponent += tz;
    }
}

/// Remove until 'bits' bits remain
BitString BitString::truncate(int bits) const {
    BitString result = *this;

    int totalBits = result.mantissa.size() * 32;

    if (totalBits <= bits)
        return result;

    int removeBits = totalBits - bits;
    rightShift(result.mantissa, removeBits);
    result.exponent += removeBits;
    result.normalize();

    return result;
}

/// Compares absolute values (e.g. |-3| > |2|)
int BitString::compareMag(const BitString& a, const BitString& b) {
    if (a.exponent != b.exponent)
        return a.exponent < b.exponent ? -1 : 1;

    if (a.mantissa.size() != b.mantissa.size())
        return a.mantissa.size() < b.mantissa.size() ? -1 : 1;

    for (int i = a.mantissa.size() - 1; i >= 0; --i)
        if (a.mantissa[i] != b.mantissa[i])
            return a.mantissa[i] < b.mantissa[i] ? -1 : 1;

    return 0;
}

/// TODO: make <<
void BitString::leftShift(vector<uint32_t>& v, int bits) {
    if (bits == 0) return;
    int intShift = bits / 32;
    int bitShift = bits % 32;

    if (bitShift) {
        uint32_t carry = 0;
        for (size_t i = 0; i < v.size(); ++i){
            uint64_t cursor = ((uint64_t)v[i] << bitShift) | carry;
            v[i] = (uint32_t)cursor;
            carry = cursor >> 32;
        }
        if (carry) v.push_back(carry);
    }
    if (intShift) v.insert(v.begin(), intShift, 0);
}

/// TODO: make >>
void BitString::rightShift(vector<uint32_t>& v, int bits) {
    if (bits == 0) return;
    int intShift = bits / 32;
    int bitShift = bits % 32;

    if (intShift >= (int)v.size()) {
        v.assign(1, 0);
        return;
    }

    v.erase(v.begin(), v.begin() + intShift);

    if (bitShift) {
        uint32_t carry = 0;
        for (int i = v.size() - 1; i >= 0; --i){
            uint32_t newCarry = v[i] << (32 - bitShift);
            v[i] = (v[i] >> bitShift) | carry;
            carry = newCarry;
        }
    }
}

BitString& BitString::operator=(const BitString& other) {
    if (this != &other) {
        sign = other.sign;
        mantissa = other.mantissa;
        exponent = other.exponent;
    }
    return *this;
}

/// Both magnitude add and sub implemented here
BitString BitString::operator+(const BitString& other) const {
    BitString a = *this;
    BitString b = other;

    // Align exponents to the smaller one by left‑shifting the larger
    if (a.exponent > b.exponent) {
        leftShift(a.mantissa, a.exponent - b.exponent);
        a.exponent = b.exponent;
    } else if (b.exponent > a.exponent) {
        leftShift(b.mantissa, b.exponent - a.exponent);
        b.exponent = a.exponent;
    }
    // Now a.exponent == b.exponent

    BitString result;
    result.exponent = a.exponent;

    if (a.sign == b.sign) {
        // Magnitude addition
        result.sign = a.sign;
        size_t n = max(a.mantissa.size(), b.mantissa.size());
        result.mantissa.resize(n);
        uint64_t carry = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t av = i < a.mantissa.size() ? a.mantissa[i] : 0;
            uint64_t bv = i < b.mantissa.size() ? b.mantissa[i] : 0;
            uint64_t sum = av + bv + carry;
            result.mantissa[i] = (uint32_t)sum;
            carry = sum >> 32;
        }
        if (carry) result.mantissa.push_back(carry);
    } else {
        // Magnitude subtraction
        int cmp = compareMag(a, b);
        if (cmp == 0) return BitString();               // a - a = 0
        const BitString* great = &a;
        const BitString* low  = &b;
        if (cmp < 0) swap(great, low);
        result.sign = great->sign;
        result.mantissa = great->mantissa;
        uint64_t borrow = 0;
        for (size_t i = 0; i < result.mantissa.size(); ++i) {
            uint64_t av = result.mantissa[i];
            uint64_t bv = i < low->mantissa.size() ? low->mantissa[i] : 0;
            uint64_t sub = av - bv - borrow;
            borrow = (av < bv + borrow) ? 1 : 0;
            result.mantissa[i] = (uint32_t)sub;
        }
    }

    result.normalize();
    return result;
}

BitString BitString::operator-() const {
    BitString result=*this;
    if (!result.isZero()) result.sign=!result.sign;
    return result;
}

BitString BitString::operator-(const BitString& other) const {
    return *this + (-other);
}

///TODO: this is O(n^2), migrate to Karatsuba later
BitString BitString::operator*(const BitString& b) const {
    BitString result;
    result.sign = sign ^ b.sign;
    result.exponent=exponent + b.exponent;
    result.mantissa.assign(mantissa.size() + b.mantissa.size(), 0);

    for (size_t i = 0; i < mantissa.size(); ++i){
        uint64_t carry = 0;
        for (size_t j = 0; j < b.mantissa.size(); ++j){
            uint64_t cur = result.mantissa[i+j] + (uint64_t)mantissa[i] * b.mantissa[j] + carry;
            result.mantissa[i+j] = (uint32_t)cur;
            carry = cur >> 32;
        }
        result.mantissa[i+b.mantissa.size()] += carry;
    }

    result.normalize();
    return result;
}

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

bool BitString::isZero() const {
    return mantissa.size() == 1 && mantissa[0] == 0;
}