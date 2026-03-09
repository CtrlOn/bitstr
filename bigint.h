#ifndef BIGINT_H
#define BIGINT_H

#include "limb_config.h"
#include <vector>
#include <cstdint>

namespace BigInt {

constexpr int klimb_tBits = limb_config::limb_bits;

void left_shift(std::vector<limb_t>& v, unsigned int bits);

void right_shift(std::vector<limb_t>& v, unsigned int bits);

/// Returns the number of bits needed to represent the value
int bit_length(const std::vector<limb_t>& v);

/// Compares two big integers: returns -1 if a < b, 0 if a == b, 1 if a > b
int bigint_cmp(const std::vector<limb_t>& a, const std::vector<limb_t>& b);

/// Adds 2^bit to the given vector
void bigint_add_pow2(std::vector<limb_t>& v, int bit);

/// Binary big integer division a / b => quot and rem
/// TODO: optimize this with higher shifts
void bigint_div(const std::vector<limb_t>& a, const std::vector<limb_t>& b, std::vector<limb_t>& quot, std::vector<limb_t>& rem);

void bigint_mul_limb(std::vector<limb_t>& a, limb_t b);

void bigint_add_limb(std::vector<limb_t>& a, limb_t b);

std::vector<limb_t> bigint_add(const std::vector<limb_t>& a, const std::vector<limb_t>& b);

std::vector<limb_t> bigint_sub(const std::vector<limb_t>& a, const std::vector<limb_t>& b);

std::vector<limb_t> bigint_mul(const std::vector<limb_t>& a, const std::vector<limb_t>& b);

} // namespace

#endif // BIGINT_H
