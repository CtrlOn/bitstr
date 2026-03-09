// bitstr_string.cpp
#include "bitstr.h"
#include "bigint.h"

#define DEC_FRAC_OUT 99 // How many decimal digits after comma to output
#define BIN_FRAC_IN 448 // Input precision guard for stable 99-digit output through arithmetic chains

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

static bool isZeroVec(const vector<uint32_t>& v) {
    return v.size() == 1 && v[0] == 0;
}

static vector<uint32_t> lowBits(const vector<uint32_t>& v, int bits) {
    if (bits <= 0) {
        return {0};
    }

    vector<uint32_t> out((bits + 31) / 32, 0);
    const size_t copyWords = min(out.size(), v.size());
    for (size_t i = 0; i < copyWords; ++i) {
        out[i] = v[i];
    }

    const int remBits = bits & 31;
    if (remBits != 0 && !out.empty()) {
        out.back() &= ((1u << remBits) - 1u);
    }

    while (out.size() > 1 && out.back() == 0) {
        out.pop_back();
    }
    return out;
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
    size_t point = s.find('.');
    if (point != string::npos) {
        fractionals = (int)(s.size() - point - 1);
    }

    int precision = 1;
    if (point != string::npos) {
        // 1 decimal digit needs log2(10) bits; 4 bits per digit + guard keeps conversion accurate.
        precision = max(BIN_FRAC_IN, fractionals * 4 + 16);
    }

    return fromString(s, precision);
}

string BitString::toString(const BitString& value, int decFracDigits) {
    if (value.isZero()) return "0.0";
    string result;

    if (value.sign) result += '-';

    BitString absValue = value;
    absValue.sign = false;

    int64_t exponent = absValue.exponent;
    vector<uint32_t> mantissa = absValue.mantissa;

    vector<uint32_t> intPart;
    vector<uint32_t> remainder;
    int fracBits = 0;

    // For normalized BitString values, denominator is always a power of two.
    if (exponent >= 0) {
        intPart = mantissa;
        left_shift(intPart, (int)exponent);
        remainder = {0};
    } else {
        fracBits = (int)(-exponent);
        intPart = mantissa;
        right_shift(intPart, fracBits);
        remainder = lowBits(mantissa, fracBits);
    }

    string intStr = bigIntToString(intPart);

    if (isZeroVec(remainder)) {
        result += intStr + ".0";
        return result;
    }

    string fracDigits;
    const int totalFracDigits = decFracDigits + 1;

    for (int i = 0; i < totalFracDigits; ++i) {
        bigint_mul_int(remainder, 10);

        vector<uint32_t> digitVec = remainder;
        right_shift(digitVec, fracBits);
        uint32_t digit = digitVec.empty() ? 0 : digitVec[0];
        if (digit > 9) {
            digit = 9;
        }

        fracDigits.push_back('0' + (char)digit);

        vector<uint32_t> sub = {digit};
        left_shift(sub, fracBits);
        remainder = bigint_sub(remainder, sub);

        if (isZeroVec(remainder)) {
            break;
        }
    }

    while (fracDigits.size() < (size_t)totalFracDigits) {
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
