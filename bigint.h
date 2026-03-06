#ifndef BIGINT_H
#define BIGINT_H

#include "bitstr.h"
#include <vector>
#include <cstdint>

namespace BigInt {

/// Returns the number of bits needed to represent the value
int bit_length(const std::vector<uint32_t>& v);

/// Compares two big integers: returns -1 if a < b, 0 if a == b, 1 if a > b
int bigint_cmp(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);

/// Adds 2^bit to the given vector
void bigint_add_pow2(std::vector<uint32_t>& v, int bit);

/// Binary big integer division a / b => quot and rem
/// TODO: optimize this with higher shifts
void bigint_div(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, std::vector<uint32_t>& quot, std::vector<uint32_t>& rem);

std::vector<uint32_t> bigint_add(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);

std::vector<uint32_t> bigint_sub(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);

/// Multiplies two big integers
std::vector<uint32_t> bigint_mul(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);

/// Multiplies a big integer by a 32-bit integer
void bigint_mul_int(std::vector<uint32_t>& a, std::uint32_t b);

/// Adds a 32-bit integer to a big integer
void bigint_add_int(std::vector<uint32_t>& a, std::uint32_t b);

} // namespace

#endif // BIGINT_H
