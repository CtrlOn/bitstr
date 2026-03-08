// bitstr_string.cpp
#include "bitstr.h"
#include "bigint.h"

#define DEC_FRAC_OUT 99 // How many decimal digits after comma to output
#define BIN_FRAC_IN 340 // How many binary digits after comma to input (99 * log2(10) + guard bits)

using namespace std;
using namespace BigInt;

static vector<uint32_t> stringToBigInt(const string& str) {
    vector<uint32_t> result = {0};
    for (char c : str) {
        int digit = c - '0';
        bigint_mul_int(result, 10);
        bigint_add_int(result, digit);
    }
    return result;
}

static string bigIntToString(const vector<uint32_t>& num) {
    if (num.size() == 1 && num[0] == 0) return "0";
    vector<uint32_t> temp = num;
    string digits;
    while (!(temp.size() == 1 && temp[0] == 0)) {
        uint64_t remainder = 0;
        for (int i = (int)temp.size() - 1; i >= 0; --i) {
            uint64_t current = (remainder << 32) | temp[i];
            temp[i] = (uint32_t)(current / 10);
            remainder = current % 10;
        }
        digits.push_back('0' + (char)remainder);
        while (temp.size() > 1 && temp.back() == 0) {
            temp.pop_back();
        }
    }
    reverse(digits.begin(), digits.end());
    return digits;
}

BitString BitString::fromString(const string& str, int bitsPrecision) {
    size_t i = 0;
    bool sign = false;
    if (str[i] == '-') {
        sign = true;
        ++i;
    } else if (str[i] == '+') {
        ++i;
    }

    string intPart, fracPart;
    bool seenDot = false;
    for (; i < str.size(); ++i) {
        char c = str[i];
        if (c == '.') {
            if (seenDot) throw invalid_argument("Multiple decimal points");
            seenDot = true;
        } else if (isdigit(c)) {
            if (!seenDot) {
                intPart.push_back(c);
            } else {
                fracPart.push_back(c);
            }
        } else {
            throw invalid_argument("Invalid character in number string");
        }
    }
    if (intPart.empty()) intPart = "0";

    vector<uint32_t> integerPart = stringToBigInt(intPart);

    if (fracPart.empty()) {
        BitString result(sign, integerPart, 0);
        result.normalize();
        return result;
    }

    int fracLen = fracPart.size();
    vector<uint32_t> numerator = stringToBigInt(fracPart);
    vector<uint32_t> denominator = {1};
    for (int j = 0; j < fracLen; ++j) {
        bigint_mul_int(denominator, 10);
    }

    vector<uint32_t> fracBits = {0};
    vector<uint32_t> remainder = numerator;
    for (int bit = 0; bit < bitsPrecision; ++bit) {
        left_shift(remainder, 1);
        left_shift(fracBits, 1);
        if (bigint_cmp(remainder, denominator) >= 0) {
            bigint_add_int(fracBits, 1);
            remainder = bigint_sub(remainder, denominator);
        }
    }

    left_shift(remainder, 1);
    int cmp = bigint_cmp(remainder, denominator);
    bool roundUp = false;
    if (cmp > 0) {
        roundUp = true;
    } else if (cmp == 0) {
        if (!fracBits.empty() && (fracBits[0] & 1)) {
            roundUp = true;
        }
    }
    if (roundUp) {
        uint64_t carry = 1;
        for (size_t i = 0; i < fracBits.size() && carry; ++i) {
            uint64_t sum = uint64_t(fracBits[i]) + carry;
            fracBits[i] = (uint32_t)sum;
            carry = sum >> 32;
        }
        if (carry) fracBits.push_back((uint32_t)carry);
    }

    vector<uint32_t> integerShifted = integerPart;
    left_shift(integerShifted, bitsPrecision);
    vector<uint32_t> mantissa = bigint_add(integerShifted, fracBits);

    BitString result(sign, mantissa, -bitsPrecision);
    result.normalize();
    return result;
}

BitString BitString::fromString(const string& s) {
    int fractionals = 0;
    int point = s.find('.');
    if (point != std::string::npos) {
        fractionals = (int)(s.size() - s.find('.') - 1);
    }
    float precision = s.find('.') != string::npos ? BIN_FRAC_IN * (fractionals * 1.414f) : 1;
    return fromString(s, (int)precision + 1);
}

string BitString::toString(const BitString& value, int decFracDigits) {
    if (value.isZero()) return "0.0";
    string result;
    
    if (value.sign) result += '-';

    BitString absValue = value;
    absValue.sign = false;

    int64_t exponent = absValue.exponent;
    vector<uint32_t> mantissa = absValue.mantissa;

    vector<uint32_t> numerator = mantissa;
    vector<uint32_t> denominator = {1};
    if (exponent >= 0) {
        left_shift(numerator, (int)exponent);
    } else {
        left_shift(denominator, (int)(-exponent));
    }

    vector<uint32_t> intPart, remainder;
    bigint_div(numerator, denominator, intPart, remainder);
    string intStr = bigIntToString(intPart);

    if (remainder.empty() || (remainder.size() == 1 && remainder[0] == 0)) {
        result += intStr + ".0";
        return result;
    }

    string fracDigits;
    for (int i = 0; i < decFracDigits + 1; ++i) {
        bigint_mul_int(remainder, 10);
        vector<uint32_t> digitVec, newRem;
        bigint_div(remainder, denominator, digitVec, newRem);
        uint32_t digit = digitVec.empty() ? 0 : digitVec[0];
        fracDigits.push_back('0' + (char)digit);
        remainder = newRem;
        if (remainder.empty() || (remainder.size() == 1 && remainder[0] == 0)) {
            break;
        }
    }
    while (fracDigits.size() < decFracDigits + 1) {
        fracDigits.push_back('0');
    }

    string frac = fracDigits.substr(0, decFracDigits);
    char nextDigit = fracDigits[decFracDigits];
    if (nextDigit >= '5') {
        int carry = 1;
        for (int i = (int)frac.size() - 1; i >= 0 && carry; --i) {
            int digit = frac[i] - '0' + carry;
            if (digit == 10) {
                frac[i] = '0';
                carry = 1;
            } else {
                frac[i] = '0' + digit;
                carry = 0;
            }
        }
        if (carry) {
            vector<uint32_t> intBig = stringToBigInt(intStr);
            bigint_add_int(intBig, 1);
            intStr = bigIntToString(intBig);
            frac = string(decFracDigits, '0');
        }
    }

    while (!frac.empty() && frac.back() == '0') {
        frac.pop_back();
    }

    result += intStr;
    if (frac.empty()) {
        result += ".0";
        if (intStr == "0") return "0.0"; // Fix '-0.0'
    } else {
        result += "." + frac;
    }

    return result;
}

string BitString::toString(const BitString& value) {
    return toString(value, DEC_FRAC_OUT);
}