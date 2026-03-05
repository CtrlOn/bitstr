// bitstr_string.cpp
#include "bitstr.h"

#define DEC_FRAC_OUT 99 // How many decimal digits after comma to output
#define BIN_FRAC_IN 340 // How many binary digits after comma to input (100 * log2(10) + guard bits)

using namespace std;

namespace {  // internal helpers

static vector<uint32_t> intvec_add(const vector<uint32_t>& a, const vector<uint32_t>& b) {
    size_t num = max(a.size(), b.size());
    vector<uint32_t> result(num);
    uint64_t carry = 0;
    for (size_t i = 0; i < num; ++i) {
        uint64_t av = i < a.size() ? a[i] : 0;
        uint64_t bv = i < b.size() ? b[i] : 0;
        uint64_t sum = av + bv + carry;
        result[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    if (carry) result.push_back((uint32_t)carry);
    return result;
}

static void intvec_mul_uint32(vector<uint32_t>& a, uint32_t b) {
    uint64_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t prod = (uint64_t)a[i] * b + carry;
        a[i] = (uint32_t)prod;
        carry = prod >> 32;
    }
    if (carry) a.push_back((uint32_t)carry);
}

static void intvec_add_uint32(vector<uint32_t>& a, uint32_t b) {
    uint64_t carry = b;
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t sum = (uint64_t)a[i] + carry;
        a[i] = (uint32_t)sum;
        carry = sum >> 32;
        if (carry == 0) break;
    }
    if (carry) a.push_back((uint32_t)carry);
}

static vector<uint32_t> string_to_intvec(const string& s) {
    vector<uint32_t> result = {0};
    for (char c : s) {
        int digit = c - '0';
        intvec_mul_uint32(result, 10);
        intvec_add_uint32(result, digit);
    }
    return result;
}

static string intvec_to_string(const vector<uint32_t>& a) {
    if (a.size() == 1 && a[0] == 0) return "0";
    vector<uint32_t> num = a;
    string digits;
    while (!(num.size() == 1 && num[0] == 0)) {
        uint64_t remainder = 0;
        for (int i = (int)num.size() - 1; i >= 0; --i) {
            uint64_t current = (remainder << 32) | num[i];
            num[i] = (uint32_t)(current / 10);
            remainder = current % 10;
        }
        digits.push_back('0' + (char)remainder);
        while (num.size() > 1 && num.back() == 0)
            num.pop_back();
    }
    reverse(digits.begin(), digits.end());
    return digits;
}

static int bit_length(const vector<uint32_t>& v) {
    if (v.empty()) return 0;
    // Find the highest non-zero word (skip trailing zeros)
    int top = v.size() - 1;
    while (top >= 0 && v[top] == 0) --top;
    if (top < 0) return 0;
    int msb = 31 - __builtin_clz(v[top]);
    return top * 32 + msb + 1;
}

static int cmp_vec(const vector<uint32_t>& a, const vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (int i = (int)a.size() - 1; i >= 0; --i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

static vector<uint32_t> sub_vec(const vector<uint32_t>& a, const vector<uint32_t>& b) {
    vector<uint32_t> res = a;
    uint64_t borrow = 0;
    for (size_t i = 0; i < res.size(); ++i) {
        uint64_t av = res[i];
        uint64_t bv = i < b.size() ? b[i] : 0;
        uint64_t sub = av - bv - borrow;
        borrow = (av < bv + borrow) ? 1 : 0;
        res[i] = (uint32_t)sub;
    }
    while (res.size() > 1 && res.back() == 0) res.pop_back();
    return res;
}

static void add_pow2(vector<uint32_t>& v, int bit) {
    int word = bit / 32;
    int subbit = bit % 32;
    uint64_t carry = uint64_t(1) << subbit;
    if (word >= (int)v.size()) v.resize(word + 1, 0);
    for (int i = word; i < (int)v.size() && carry; ++i) {
        uint64_t sum = uint64_t(v[i]) + carry;
        v[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    if (carry) v.push_back((uint32_t)carry);
}

/// Binary long division: A / B => quot and R
static void div_bin(const vector<uint32_t>& a, const vector<uint32_t>& b, vector<uint32_t>& quot, vector<uint32_t>& rem) {
    if (cmp_vec(a, b) < 0) {
        quot = {0};
        rem = a;
        return;
    }
    int lenA = bit_length(a);
    int lenB = bit_length(b);
    int qbits = lenA - lenB; // max possible bit index of quotient
    quot.assign(1, 0);
    rem = a;
    for (int i = qbits; i >= 0; --i) {
        vector<uint32_t> Bshift = b;
        BitString::leftShift(Bshift, i);   // b << i
        if (cmp_vec(rem, Bshift) >= 0) {
            rem = sub_vec(rem, Bshift);
            add_pow2(quot, i);
        }
    }
    while (quot.size() > 1 && quot.back() == 0) quot.pop_back();
    while (rem.size() > 1 && rem.back() == 0) rem.pop_back();
}

} // namespace

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

    // Convert integer part to big integer
    vector<uint32_t> I = string_to_intvec(int_part);

    // If no fractional part, we are done
    if (frac_part.empty()) {
        BitString result(sign, I, 0);
        result.normalize();
        return result;
    }

    int frac_len = frac_part.size();
    // Numerator of the fractional part (as an integer)
    vector<uint32_t> num = string_to_intvec(frac_part);
    // Denominator = 10^frac_len
    vector<uint32_t> dem = {1};
    for (int j = 0; j < frac_len; ++j) {
        intvec_mul_uint32(dem, 10);
    }

    // Generate BIN_FRAC_IN binary bits for the fractional part
    vector<uint32_t> fracBits = {0};
    vector<uint32_t> remainder = num;
    for (int bit = 0; bit < BIN_FRAC_IN; ++bit) {
        leftShift(remainder, 1);             // remainder *= 2
        leftShift(fracBits, 1);                      // fracBits *= 2
        if (cmp_vec(remainder, dem) >= 0) {
            intvec_add_uint32(fracBits, 1);          // set current bit to 1
            remainder = sub_vec(remainder, dem);
        }
        // else bit remains 0
    }

    // Rounding (half to even) using the next bit
    leftShift(remainder, 1);                  // 2 * remainder
    int cmp = cmp_vec(remainder, dem);
    bool round_up = false;
    if (cmp > 0) {
        round_up = true;
    } else if (cmp == 0) {
        // Tie: round to even – look at the least significant bit of fracBits
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
    leftShift(I_shifted, BIN_FRAC_IN);
    vector<uint32_t> M = intvec_add(I_shifted, fracBits);

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
    string int_str = intvec_to_string(int_part);

    // If no remainder, return integer with ".0"
    if (rem.empty() || (rem.size() == 1 && rem[0] == 0)) {
        result += int_str + ".0";
        return result;
    }

    // Generate fractional digits (including one extra for rounding)
    vector<uint32_t> remainder = rem;
    string frac_digits;
    for (int i = 0; i < DEC_FRAC_OUT + 1; ++i) {
        intvec_mul_uint32(remainder, 10);               // remainder *= 10
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
            vector<uint32_t> int_big = string_to_intvec(int_str);
            intvec_add_uint32(int_big, 1);
            int_str = intvec_to_string(int_big);
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