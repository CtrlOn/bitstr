// Big integer math helpers
#include "bigint.h"
#include "bitstr.h"

namespace BigInt {

void left_shift(std::vector<uint32_t>& v, unsigned int bits) {
    if (bits == 0) return;
    int intShift = bits / 32;
    int bitShift = bits % 32;

    if (bitShift) {
        uint32_t carry = 0;
        for (size_t i = 0; i < v.size(); ++i){
            uint64_t cursor = ((uint64_t)v[i] << bitShift) | carry;
            v[i] = (uint32_t)cursor;
            carry = cursor >> 32;
        }
        if (carry) v.push_back(carry);
    }
    if (intShift) v.insert(v.begin(), intShift, 0);
}

void right_shift(std::vector<uint32_t>& v, unsigned int bits) {
    if (bits == 0) return;
    int intShift = bits / 32;
    int bitShift = bits % 32;

    if (intShift >= (int)v.size()) {
        v.assign(1, 0);
        return;
    }

    v.erase(v.begin(), v.begin() + intShift);

    if (bitShift) {
        uint32_t carry = 0;
        for (int i = v.size() - 1; i >= 0; --i){
            uint32_t newCarry = v[i] << (32 - bitShift);
            v[i] = (v[i] >> bitShift) | carry;
            carry = newCarry;
        }
    }
}

int bit_length(const std::vector<uint32_t>& v) {
    if (v.empty()) return 0;
    // Find the highest non-zero word (skip trailing zeros)
    int topIndex = static_cast<int>(v.size()) - 1;
    while (topIndex >= 0 && v[topIndex] == 0) --topIndex;
    if (topIndex < 0) return 0;
    int msbIndex = 31 - __builtin_clz(v[topIndex]);
    return topIndex * 32 + msbIndex + 1;
}

int bigint_cmp(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (int i = (int)a.size() - 1; i >= 0; --i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

std::vector<uint32_t> bigint_sub(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> result = a;
    uint64_t borrow = 0;
    for (size_t i = 0; i < result.size(); ++i) {
        uint64_t aWord = result[i];
        uint64_t bWord = i < b.size() ? b[i] : 0;
        uint64_t diff = aWord - bWord - borrow;
        borrow = (aWord < bWord + borrow) ? 1 : 0;
        result[i] = (uint32_t)diff;
    }
    while (result.size() > 1 && result.back() == 0) result.pop_back();
    return result;
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
    int wordIndex = bit / 32;
    int bitIndex = bit % 32;
    uint64_t carry = uint64_t(1) << bitIndex;
    if (wordIndex >= (int)v.size()) v.resize(wordIndex + 1, 0);
    for (int i = wordIndex; i < (int)v.size() && carry; ++i) {
        uint64_t sum = uint64_t(v[i]) + carry;
        v[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    if (carry) v.push_back((uint32_t)carry);
}

void bigint_div(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, std::vector<uint32_t>& quotient, std::vector<uint32_t>& remainder) {
    std::vector<uint32_t> dividend = a;
    std::vector<uint32_t> divisor = b;
    while (dividend.size() > 1 && dividend.back() == 0) dividend.pop_back();
    while (divisor.size() > 1 && divisor.back() == 0) divisor.pop_back();

    if (divisor.size() == 1 && divisor[0] == 0) {
        throw std::domain_error("bigint_div by zero");
    }

    if (bigint_cmp(dividend, divisor) < 0) {
        quotient = {0};
        remainder = dividend;
        return;
    }

    // Fast path for one-limb divisor.
    if (divisor.size() == 1) {
        const uint64_t divisorWord = divisor[0];
        quotient.assign(dividend.size(), 0);
        uint64_t remainderWord = 0;
        for (int i = static_cast<int>(dividend.size()) - 1; i >= 0; --i) {
            uint64_t current = (remainderWord << 32) | dividend[i];
            quotient[i] = static_cast<uint32_t>(current / divisorWord);
            remainderWord = current % divisorWord;
        }
        while (quotient.size() > 1 && quotient.back() == 0) quotient.pop_back();
        remainder = {static_cast<uint32_t>(remainderWord)};
        return;
    }

    // Knuth-style normalized long division (base 2^32).
    const uint64_t base = (1ULL << 32);
    const int divisorLen = static_cast<int>(divisor.size());
    const int quotientLen = static_cast<int>(dividend.size()) - divisorLen;

    const int normalizeShift = __builtin_clz(divisor.back());
    std::vector<uint32_t> normalizedDivisor = divisor;
    std::vector<uint32_t> normalizedDividend = dividend;
    if (normalizeShift) {
        left_shift(normalizedDivisor, normalizeShift);
        left_shift(normalizedDividend, normalizeShift);
    }
    normalizedDividend.push_back(0);

    quotient.assign(quotientLen + 1, 0);

    for (int j = quotientLen; j >= 0; --j) {
        uint64_t u2 = normalizedDividend[j + divisorLen];
        uint64_t u1 = normalizedDividend[j + divisorLen - 1];
        uint64_t u0 = (divisorLen > 1) ? normalizedDividend[j + divisorLen - 2] : 0;

        uint64_t numerator = (u2 << 32) | u1;
        uint64_t quotientHat = numerator / normalizedDivisor[divisorLen - 1];
        uint64_t remainderHat = numerator % normalizedDivisor[divisorLen - 1];

        if (quotientHat >= base) {
            quotientHat = base - 1;
            remainderHat += normalizedDivisor[divisorLen - 1];
        }

        if (divisorLen > 1) {
            while (quotientHat * normalizedDivisor[divisorLen - 2] > (remainderHat << 32) + u0) {
                --quotientHat;
                remainderHat += normalizedDivisor[divisorLen - 1];
                if (remainderHat >= base) break;
            }
        }

        uint64_t borrow = 0;
        uint64_t carry = 0;
        for (int i = 0; i < divisorLen; ++i) {
            uint64_t p = quotientHat * normalizedDivisor[i] + carry;
            carry = p >> 32;
            uint64_t productLow = static_cast<uint32_t>(p);

            uint64_t current = normalizedDividend[j + i];
            uint64_t diff = current - productLow - borrow;
            normalizedDividend[j + i] = static_cast<uint32_t>(diff);
            borrow = (current < productLow + borrow) ? 1 : 0;
        }

        uint64_t currentTop = normalizedDividend[j + divisorLen];
        uint64_t diffTop = currentTop - carry - borrow;
        normalizedDividend[j + divisorLen] = static_cast<uint32_t>(diffTop);
        bool underflow = currentTop < carry + borrow;

        if (underflow) {
            --quotientHat;
            uint64_t c = 0;
            for (int i = 0; i < divisorLen; ++i) {
                uint64_t sum = static_cast<uint64_t>(normalizedDividend[j + i]) + normalizedDivisor[i] + c;
                normalizedDividend[j + i] = static_cast<uint32_t>(sum);
                c = sum >> 32;
            }
            normalizedDividend[j + divisorLen] = static_cast<uint32_t>(static_cast<uint64_t>(normalizedDividend[j + divisorLen]) + c);
        }

        quotient[j] = static_cast<uint32_t>(quotientHat);
    }

    remainder.assign(normalizedDividend.begin(), normalizedDividend.begin() + divisorLen);
    if (normalizeShift) {
        right_shift(remainder, normalizeShift);
    }

    while (quotient.size() > 1 && quotient.back() == 0) quotient.pop_back();
    while (remainder.size() > 1 && remainder.back() == 0) remainder.pop_back();
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
