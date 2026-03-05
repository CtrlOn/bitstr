// Division and Modulo
#include "bitstr.h"
#include "bitstr_bigint.hpp"

using namespace std;
using namespace BigInt;

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