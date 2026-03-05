// bitstr_string.cpp
#include "bitstr.h"
#include "bitstr_bigint.hpp"

#define DEC_FRAC_OUT 99 // How many decimal digits after comma to output
#define BIN_FRAC_IN 340 // How many binary digits after comma to input (99 * log2(10) + guard bits)

using namespace std;
using namespace BigInt;

BitString BitString::fromString(const string& s) {
    size_t i = 0;
    bool sign = false;
    if (s[i] == '-') {
        sign = true;
        ++i;
    } else if (s[i] == '+') {
        ++i;
    }

    string int_part, frac_part;
    bool seen_dot = false;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '.') {
            if (seen_dot) throw invalid_argument("Multiple decimal points");
            seen_dot = true;
        } else if (isdigit(c)) {
            if (!seen_dot)
                int_part.push_back(c);
            else
                frac_part.push_back(c);
        } else {
            throw invalid_argument("Invalid character in number string");
        }
    }
    if (int_part.empty()) int_part = "0";

    // Convert integer part to a big integer
    vector<uint32_t> I = string_to_bigint(int_part);

    // If no fractional part, we are done
    if (frac_part.empty()) {
        BitString result(sign, I, 0);
        result.normalize();
        return result;
    }

    int frac_len = frac_part.size();
    // Numerator of the fractional part (as an integer)
    vector<uint32_t> num = string_to_bigint(frac_part);
    // Denominator = 10^frac_len
    vector<uint32_t> dem = {1};
    for (int j = 0; j < frac_len; ++j) {
        bigint_mul_int(dem, 10);
    }

    // Generate BIN_FRAC_IN binary bits for the fractional part
    vector<uint32_t> fracBits = {0};
    vector<uint32_t> remainder = num;
    for (int bit = 0; bit < BIN_FRAC_IN; ++bit) {
        BitString::leftShift(remainder, 1); // remainder *= 2
        BitString::leftShift(fracBits, 1);  // fracBits *= 2
        if (cmp_vec(remainder, dem) >= 0) {
            bigint_add_int(fracBits, 1); // set current bit to 1
            remainder = sub_vec(remainder, dem);
        }
        // else bit remains 0
    }

    // Rounding (half to even) using the next bit
    BitString::leftShift(remainder, 1); // remainder *= 2
    int cmp = cmp_vec(remainder, dem);
    bool round_up = false;
    if (cmp > 0) {
        round_up = true;
    } else if (cmp == 0) {
        // Round to even if tie
        if (!fracBits.empty() && (fracBits[0] & 1))
            round_up = true;
    }
    if (round_up) {
        uint64_t carry = 1;
        for (size_t i = 0; i < fracBits.size() && carry; ++i) {
            uint64_t sum = uint64_t(fracBits[i]) + carry;
            fracBits[i] = (uint32_t)sum;
            carry = sum >> 32;
        }
        if (carry) fracBits.push_back((uint32_t)carry);
    }

    // Combine integer and fractional parts: M = (I << BIN_FRAC_IN) + fracBits
    vector<uint32_t> I_shifted = I;
    BitString::leftShift(I_shifted, BIN_FRAC_IN);
    vector<uint32_t> M = bigint_add(I_shifted, fracBits);

    BitString result(sign, M, -BIN_FRAC_IN);
    result.normalize();
    return result;
}

string BitString::toString(const BitString& value) {
    if (value.isZero())
        return "0.0";

    string result;
    if (value.sign) result += '-';

    // Work with absolute value
    BitString abs = value;
    abs.sign = false;

    int64_t exp = abs.exponent;
    vector<uint32_t> mant = abs.mantissa;

    // Build numerator and denominator as big integers
    vector<uint32_t> num = mant;
    vector<uint32_t> den = {1};
    if (exp >= 0) {
        leftShift(num, (int)exp);   // num = mant * 2^exp
    } else {
        leftShift(den, (int)(-exp)); // den = 2^(-exp)
    }

    // Integer part = num / den, remainder = num % den
    vector<uint32_t> int_part, rem;
    div_bin(num, den, int_part, rem);
    string int_str = bigint_to_string(int_part);

    // If no remainder, return integer with ".0"
    if (rem.empty() || (rem.size() == 1 && rem[0] == 0)) {
        result += int_str + ".0";
        return result;
    }

    // Generate fractional digits (including one extra for rounding)
    vector<uint32_t> remainder = rem;
    string frac_digits;
    for (int i = 0; i < DEC_FRAC_OUT + 1; ++i) {
        bigint_mul_int(remainder, 10);               // remainder *= 10
        vector<uint32_t> digit_vec, new_rem;
        div_bin(remainder, den, digit_vec, new_rem);    // digit = floor(remainder / den)
        uint32_t digit = digit_vec.empty() ? 0 : digit_vec[0];
        frac_digits.push_back('0' + (char)digit);
        remainder = new_rem;
        if (remainder.empty() || (remainder.size() == 1 && remainder[0] == 0))
            break;
    }
    // Pad with zeros if we broke early
    while (frac_digits.size() < DEC_FRAC_OUT + 1)
        frac_digits.push_back('0');

    // Round to DEC_FRAC_OUT digits using the next digit
    string frac = frac_digits.substr(0, DEC_FRAC_OUT);
    char next = frac_digits[DEC_FRAC_OUT];
    if (next >= '5') {
        int carry = 1;
        for (int i = (int)frac.size() - 1; i >= 0 && carry; --i) {
            int dem = frac[i] - '0' + carry;
            if (dem == 10) {
                frac[i] = '0';
                carry = 1;
            } else {
                frac[i] = '0' + dem;
                carry = 0;
            }
        }
        if (carry) {
            // Carry into integer part
            vector<uint32_t> int_big = string_to_bigint(int_str);
            bigint_add_int(int_big, 1);
            int_str = bigint_to_string(int_big);
            frac = string(DEC_FRAC_OUT, '0');
        }
    }

    // Remove trailing zeros from the fractional part
    while (!frac.empty() && frac.back() == '0')
        frac.pop_back();

    // Assemble final string
    result += int_str;
    if (frac.empty())
        result += ".0";
    else
        result += "." + frac;

    return result;
}