// Big integer math helpers
#include "bigint.h"
#include "bitstr.h"

namespace BigInt {

int bit_length(const std::vector<uint32_t>& v) {
    if (v.empty()) return 0;
    // Find the highest non-zero word (skip trailing zeros)
    int top = v.size() - 1;
    while (top >= 0 && v[top] == 0) --top;
    if (top < 0) return 0;
    int msb = 31 - __builtin_clz(v[top]);
    return top * 32 + msb + 1;
}

int bigint_cmp(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (int i = (int)a.size() - 1; i >= 0; --i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

std::vector<uint32_t> bigint_sub(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> res = a;
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

std::vector<uint32_t> bigint_mul(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> result(a.size() + b.size(), 0);

    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size(); ++j) {
            uint64_t cur = result[i + j] + (uint64_t)a[i] * b[j] + carry;
            result[i + j] = (uint32_t)cur;
            carry = cur >> 32;
        }
        result[i + b.size()] += carry;
    }

    while (result.size() > 1 && result.back() == 0) result.pop_back();
    return result;
}

void bigint_add_pow2(std::vector<uint32_t>& v, int bit) {
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

void bigint_div(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, std::vector<uint32_t>& quot, std::vector<uint32_t>& rem) {
    if (bigint_cmp(a, b) < 0) {
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
        std::vector<uint32_t> Bshift = b;
        BitString::leftShift(Bshift, i);
        if (bigint_cmp(rem, Bshift) >= 0) {
            rem = bigint_sub(rem, Bshift);
            bigint_add_pow2(quot, i);
        }
    }
    while (quot.size() > 1 && quot.back() == 0) quot.pop_back();
    while (rem.size() > 1 && rem.back() == 0) rem.pop_back();
}

std::vector<uint32_t> bigint_add(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    size_t num = std::max(a.size(), b.size());
    std::vector<uint32_t> result(num);
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

void bigint_mul_int(std::vector<uint32_t>& a, std::uint32_t b) {
    uint64_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t prod = (uint64_t)a[i] * b + carry;
        a[i] = (uint32_t)prod;
        carry = prod >> 32;
    }
    if (carry) a.push_back((uint32_t)carry);
}

void bigint_add_int(std::vector<uint32_t>& a, std::uint32_t b) {
    uint64_t carry = b;
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t sum = (uint64_t)a[i] + carry;
        a[i] = (uint32_t)sum;
        carry = sum >> 32;
        if (carry == 0) break;
    }
    if (carry) a.push_back((uint32_t)carry);
}

} // namespace BigInt
