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
    if (b.empty() || (b.size() == 1 && b[0] == 0))
        throw std::domain_error("Division by zero");
    if (a.empty() || (a.size() == 1 && a[0] == 0)) {
        quot = {0};
        rem = {0};
        return;
    }
    int cmp = bigint_cmp(a, b);
    if (cmp < 0) {
        quot = {0};
        rem = a;
        return;
    }
    if (cmp == 0) {
        quot = {1};
        rem = {0};
        return;
    }

    // Normalize so that the most significant word of b has its high bit set
    int norm_shift = 0;
    uint32_t b_msb = b.back();
    while ((b_msb & 0x80000000) == 0) {
        b_msb <<= 1;
        norm_shift++;
    }
    std::vector<uint32_t> a_norm = a;
    std::vector<uint32_t> b_norm = b;
    if (norm_shift > 0) {
        BitString::leftShift(a_norm, norm_shift);
        BitString::leftShift(b_norm, norm_shift);
    }
    // Add a leading zero word to a_norm to simplify indexing
    a_norm.push_back(0);
    int m = (int)a_norm.size();
    int n = (int)b_norm.size();
    // Quotient length = m - n
    quot.assign(m - n, 0);
    std::vector<uint32_t> r = a_norm;   // remainder (will be modified)

    for (int i = m - n - 1; i >= 0; --i) {
        // Extract the current slice of r (n+1 words starting at i)
        std::vector<uint32_t> slice(r.begin() + i, r.begin() + i + n + 1);

        // Guess quotient word using top two words of slice and top word of divisor
        uint64_t r_top = ((uint64_t)slice[n] << 32) | slice[n - 1];
        uint64_t b_top = b_norm.back();
        uint64_t qhat = r_top / b_top;
        if (qhat > 0xFFFFFFFF) qhat = 0xFFFFFFFF;

        // Compute product = qhat * b_norm, padded to n+1 words
        std::vector<uint32_t> prod = b_norm;
        bigint_mul_int(prod, (uint32_t)qhat);
        prod.resize(n + 1, 0);

        // Adjust qhat if product is too large
        while (bigint_cmp(prod, slice) > 0) {
            qhat--;
            prod = b_norm;
            bigint_mul_int(prod, (uint32_t)qhat);
            prod.resize(n + 1, 0);
        }

        // Subtract product from the current slice of r
        int64_t borrow = 0;
        for (int j = 0; j <= n; ++j) {
            uint64_t diff = (uint64_t)r[i + j] - prod[j] - borrow;
            if (diff > 0xFFFFFFFF) {
                diff += 0x100000000;
                borrow = 1;
            } else {
                borrow = 0;
            }
            r[i + j] = (uint32_t)diff;
        }
        // borrow must be zero because product <= slice
        quot[i] = (uint32_t)qhat;
    }

    // Remove the extra zero word from r
    r.pop_back();
    // Unnormalize the remainder
    if (norm_shift > 0) {
        BitString::rightShift(r, norm_shift);
    }
    rem = r;
    // Remove leading zeros from quotient and remainder
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
