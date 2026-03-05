#ifndef BITSTR_H
#define BITSTR_H

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <cctype>

class BitString {
private:
    bool sign;
    std::vector<uint32_t> mantissa;
    int64_t exponent;

    void normalize();

    static int compareMag(const BitString& a, const BitString& b);
    BitString truncate(int bits) const;

    static const BitString POW5_N100;
public:
    BitString();
    BitString(const bool sign, const std::vector<uint32_t>& mantissa, int64_t exponent);//TODO: hide
    BitString(const BitString& other);
    BitString(const std::string& s);
    BitString(const int32_t n);

    // String conversions
    static BitString fromString(const std::string& s);
    static std::string toString(const BitString& value);

    BitString operator+(const BitString& other) const;
    BitString operator-(const BitString& other) const;
    BitString operator-() const;
    BitString operator*(const BitString& other) const;
    BitString operator/(const BitString& other) const;
    BitString operator%(const BitString& other) const;
    //BitString operator>>(const int bits) const;
    //BitString operator<<(const int bits) const;

    BitString& operator=(const BitString& other);

    bool operator==(const BitString& other) const;
    bool operator!=(const BitString& other) const;
    bool operator<=(const BitString& other) const;
    bool operator>=(const BitString& other) const;
    bool operator<(const BitString& other) const;
    bool operator>(const BitString& other) const;

    // Base arithmetics
    static void leftShift(std::vector<uint32_t>& v, int bits);
    static void rightShift(std::vector<uint32_t>& v, int bits);
    static BitString mul(const BitString& a, const BitString& b);
    static BitString div(const BitString& a, const BitString& b, int precision);
    static BitString add(const BitString& a, const BitString& b);

    // Trigonometrics
    static BitString sin(const BitString& n);

    // Exponentials
    static BitString pow(const BitString& n, int e);
    static BitString ln(const BitString& n);
    static BitString sqrt(const BitString& n);

    // Specials
    static BitString fact(const BitString& n);
    static BitString nextP(const BitString& n);
    static BitString pi(int digits);

    // Vector utils
    static BitString avg(const std::vector<BitString>& v);
    static int find(const std::vector<BitString>& v, const BitString& target);
    static void sort(std::vector<BitString>& v);

    // Properties
    bool isZero() const;
    bool isOne() const;
    bool isPrime() const;

    // Getters for debugging
    bool getSign() const { return sign; }
    int64_t getExponent() const { return exponent; }
    const std::vector<uint32_t>& getMantissa() const { return mantissa; }
};

#endif