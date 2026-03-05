// Division and Modulo

#include "bitstr.h"

using namespace std;

namespace {  // hide helpers

// Return the number of bits needed to represent the absolute value of v (0 if v is zero)
int bit_length(const vector<uint32_t>& v) {
    if (v.empty()) return 0;
    // Find the highest non-zero word (skip trailing zeros)
    int top = v.size() - 1;
    while (top >= 0 && v[top] == 0) --top;
    if (top < 0) return 0;
    int msb = 31 - __builtin_clz(v[top]);
    return top * 32 + msb + 1;
}

// Compare two absolute vectors (LSB first). Returns -1 if a < b, 0 if equal, 1 if a > b.
int cmp_vec(const vector<uint32_t>& a, const vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (int i = int(a.size()) - 1; i >= 0; --i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

// Subtract b from a, assuming a >= b. Returns result vector.
vector<uint32_t> sub_vec(const vector<uint32_t>& a, const vector<uint32_t>& b) {
    vector<uint32_t> res = a;
    uint64_t borrow = 0;
    for (size_t i = 0; i < res.size(); ++i) {
        uint64_t av = res[i];
        uint64_t bv = i < b.size() ? b[i] : 0;
        uint64_t sub = av - bv - borrow;
        borrow = (av < bv + borrow) ? 1 : 0;
        res[i] = uint32_t(sub);
    }
    // Remove leading zeros
    while (res.size() > 1 && res.back() == 0) res.pop_back();
    return res;
}

// Add 2^bit to v (bit counted from LSB).
void add_pow2(vector<uint32_t>& v, int bit) {
    int word = bit / 32;
    int subbit = bit % 32;
    uint64_t carry = uint64_t(1) << subbit;
    if (word >= int(v.size())) v.resize(word + 1, 0);
    for (int i = word; i < int(v.size()) && carry; ++i) {
        uint64_t sum = uint64_t(v[i]) + carry;
        v[i] = uint32_t(sum);
        carry = sum >> 32;
    }
    if (carry) v.push_back(uint32_t(carry));
}

// Binary long division: compute Q and R such that A = B * Q + R, 0 <= R < B.
// Both A and B are positive vectors (LSB first).
void div_bin(const vector<uint32_t>& A, const vector<uint32_t>& B,
             vector<uint32_t>& Q, vector<uint32_t>& R) {
    if (cmp_vec(A, B) < 0) {
        Q = {0};
        R = A;
        return;
    }
    int lenA = bit_length(A);
    int lenB = bit_length(B);
    int qbits = lenA - lenB; // max possible bit index of quotient
    Q.assign(1, 0);
    R = A;
    for (int i = qbits; i >= 0; --i) {
        vector<uint32_t> Bshift = B;
        BitString::leftShift(Bshift, i); // B << i
        if (cmp_vec(R, Bshift) >= 0) {
            R = sub_vec(R, Bshift);
            add_pow2(Q, i);
        }
    }
    // Remove leading zeros from Q
    while (Q.size() > 1 && Q.back() == 0) Q.pop_back();
    while (R.size() > 1 && R.back() == 0) R.pop_back();
}

} // unnamed namespace

// ==================== BitString::div ====================

BitString BitString::div(const BitString& a, const BitString& b, int precision) {
    if (b.isZero()) throw std::domain_error("Division by zero");
    if (a.isZero()) return BitString();

    bool result_sign = a.sign ^ b.sign;

    // Work with absolute mantissas
    std::vector<uint32_t> A = a.mantissa;
    std::vector<uint32_t> B = b.mantissa;

    // Choose p so that the quotient has at least precision+1 bits.
    // p = precision + 64 + bit_length(B) is a safe overestimate.
    int p = precision + 64 + bit_length(B);

    std::vector<uint32_t> dividend = A;
    leftShift(dividend, p);
    std::vector<uint32_t> divisor = B;

    std::vector<uint32_t> Q, R;
    div_bin(dividend, divisor, Q, R);

    int qbits = bit_length(Q);
    // If we didn't get enough bits, increase p and try again (should rarely happen)
    while (qbits < precision + 1) {
        p += (precision + 1 - qbits) + 32;
        dividend = A;
        leftShift(dividend, p);
        div_bin(dividend, divisor, Q, R);
        qbits = bit_length(Q);
    }

    // Number of extra low bits to discard
    int rbits = qbits - precision;
    bool round_up = false;

    if (rbits > 0) {
        // Determine the most significant discarded bit (round bit)
        int round_bit_pos = rbits - 1;
        int round_word = round_bit_pos / 32;
        int round_shift = round_bit_pos % 32;
        bool round_bit = false;
        uint32_t low_mask = 0;
        if (round_word < (int)Q.size()) {
            round_bit = (Q[round_word] >> round_shift) & 1;
            low_mask = Q[round_word] & ((1U << round_shift) - 1);
        }
        bool any_lower = false;
        for (int i = 0; i < round_word; ++i) {
            if (Q[i] != 0) { any_lower = true; break; }
        }
        if (!any_lower && low_mask != 0) any_lower = true;

        if (round_bit) {
            if (any_lower) {
                round_up = true;               // > 1/2
            } else {
                // exactly half: round to even based on the last kept bit
                int last_kept_pos = rbits;
                int last_kept_word = last_kept_pos / 32;
                int last_kept_shift = last_kept_pos % 32;
                bool last_kept_bit = false;
                if (last_kept_word < (int)Q.size()) {
                    last_kept_bit = (Q[last_kept_word] >> last_kept_shift) & 1;
                }
                if (last_kept_bit) round_up = true;
            }
        }
    }

    // Extract the top 'precision' bits
    std::vector<uint32_t> mant = Q;
    rightShift(mant, rbits);

    if (round_up) {
        uint64_t carry = 1;
        for (size_t i = 0; i < mant.size() && carry; ++i) {
            uint64_t sum = uint64_t(mant[i]) + carry;
            mant[i] = (uint32_t)sum;
            carry = sum >> 32;
        }
        if (carry) mant.push_back((uint32_t)carry);
        // If rounding caused a new most‑significant word, we may have exceeded 'precision' bits
        int new_bits = bit_length(mant);
        if (new_bits > precision) {
            rightShift(mant, 1);
            rbits += 1;
        }
    }

    // Remove possible leading zeros
    while (mant.size() > 1 && mant.back() == 0) mant.pop_back();

    // Result exponent: a.exponent - b.exponent - p + rbits
    int64_t result_exp = a.exponent - b.exponent - p + rbits;

    BitString result(result_sign, mant, result_exp);
    result.normalize();
    return result;
}