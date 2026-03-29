// Big integer math helpers
#include "bigint.h"
#include "bitstr.h"

using namespace std;

namespace BigInt {

void left_shift(vector<limb_t>& v, unsigned int bits) {
    if (bits == 0) return;
    int intShift = bits / limb_bits;
    int bitShift = bits % limb_bits;

    if (bitShift) {
        limb_t carry = 0;
        for (size_t i = 0; i < v.size(); ++i){
            wide_limb_t cursor = (wide_limb_t(v[i]) << bitShift) | carry;
            v[i] = static_cast<limb_t>(cursor);
            carry = static_cast<limb_t>(cursor >> limb_bits);
        }
        if (carry) v.push_back(carry);
    }
    if (intShift) v.insert(v.begin(), intShift, 0);
}

void right_shift(vector<limb_t>& v, unsigned int bits) {
    if (bits == 0) return;
    int intShift = bits / limb_bits;
    int bitShift = bits % limb_bits;

    if (intShift >= (int)v.size()) {
        v.assign(1, 0);
        return;
    }

    v.erase(v.begin(), v.begin() + intShift);

    if (bitShift) {
        limb_t carry = 0;
        for (int i = v.size() - 1; i >= 0; --i){
            limb_t newCarry = static_cast<limb_t>(v[i] << (limb_bits - bitShift));
            v[i] = (v[i] >> bitShift) | carry;
            carry = newCarry;
        }
    }
}

int bit_length(const vector<limb_t>& v) {
    if (v.empty()) return 0;
    // Find the highest non-zero word (skip trailing zeros)
    int topIndex = static_cast<int>(v.size()) - 1;
    while (topIndex >= 0 && v[topIndex] == 0) --topIndex;
    if (topIndex < 0) return 0;
    int msbIndex = (limb_bits - 1) - limb_clz(v[topIndex]);
    return topIndex * limb_bits + msbIndex + 1;
}

int bigint_cmp(const vector<limb_t>& a, const vector<limb_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (int i = (int)a.size() - 1; i >= 0; --i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

vector<limb_t> bigint_sub(const vector<limb_t>& a, const vector<limb_t>& b) {
    vector<limb_t> result = a;
    wide_limb_t borrow = 0;
    for (size_t i = 0; i < result.size(); ++i) {
        wide_limb_t aWord = result[i];
        wide_limb_t bWord = i < b.size() ? b[i] : 0;
        wide_limb_t diff = aWord - bWord - borrow;
        borrow = (aWord < bWord + borrow) ? 1 : 0;
        result[i] = static_cast<limb_t>(diff);
    }
    while (result.size() > 1 && result.back() == 0) result.pop_back();
    return result;
}

vector<limb_t> bigint_mul(const vector<limb_t>& a, const vector<limb_t>& b) {
    vector<limb_t> result(a.size() + b.size(), 0);

    for (size_t i = 0; i < a.size(); ++i) {
        wide_limb_t carry = 0;
        for (size_t j = 0; j < b.size(); ++j) {
            wide_limb_t cur = result[i + j] + wide_limb_t(a[i]) * b[j] + carry;
            result[i + j] = static_cast<limb_t>(cur);
            carry = cur >> limb_bits;
        }
        result[i + b.size()] += static_cast<limb_t>(carry);
    }

    while (result.size() > 1 && result.back() == 0) result.pop_back();
    return result;
}

void bigint_add_pow2(vector<limb_t>& v, int bit) {
    int wordIndex = bit / limb_bits;
    int bitIndex = bit % limb_bits;
    wide_limb_t carry = wide_limb_t(1) << bitIndex;
    if (wordIndex >= (int)v.size()) v.resize(wordIndex + 1, 0);
    for (int i = wordIndex; i < (int)v.size() && carry; ++i) {
        wide_limb_t sum = wide_limb_t(v[i]) + carry;
        v[i] = static_cast<limb_t>(sum);
        carry = sum >> limb_bits;
    }
    if (carry) v.push_back(static_cast<limb_t>(carry));
}

void bigint_div(const vector<limb_t>& a, const vector<limb_t>& b, vector<limb_t>& quotient, vector<limb_t>& remainder) {
    vector<limb_t> dividend = a;
    vector<limb_t> divisor = b;
    while (dividend.size() > 1 && dividend.back() == 0) dividend.pop_back();
    while (divisor.size() > 1 && divisor.back() == 0) divisor.pop_back();

    if (divisor.size() == 1 && divisor[0] == 0) {
        throw domain_error("bigint_div by zero");
    }

    if (bigint_cmp(dividend, divisor) < 0) {
        quotient = {0};
        remainder = dividend;
        return;
    }

    // Fast path for one-limb divisor.
    if (divisor.size() == 1) {
        const wide_limb_t divisorWord = divisor[0];
        quotient.assign(dividend.size(), 0);
        wide_limb_t remainderWord = 0;
        for (int i = static_cast<int>(dividend.size()) - 1; i >= 0; --i) {
            wide_limb_t current = (remainderWord << limb_bits) | dividend[i];
            quotient[i] = static_cast<limb_t>(current / divisorWord);
            remainderWord = current % divisorWord;
        }
        while (quotient.size() > 1 && quotient.back() == 0) quotient.pop_back();
        remainder = {static_cast<limb_t>(remainderWord)};
        return;
    }

    // Knuth-style normalized long division (base 2^limb_bits).
    const wide_limb_t base = (wide_limb_t(1) << limb_bits);
    const int divisorLen = static_cast<int>(divisor.size());
    const int quotientLen = static_cast<int>(dividend.size()) - divisorLen;

    const int normalizeShift = limb_clz(divisor.back());
    vector<limb_t> normalizedDivisor = divisor;
    vector<limb_t> normalizedDividend = dividend;
    if (normalizeShift) {
        left_shift(normalizedDivisor, normalizeShift);
        left_shift(normalizedDividend, normalizeShift);
    }
    normalizedDividend.push_back(0);

    quotient.assign(quotientLen + 1, 0);

    for (int j = quotientLen; j >= 0; --j) {
        wide_limb_t u2 = normalizedDividend[j + divisorLen];
        wide_limb_t u1 = normalizedDividend[j + divisorLen - 1];
        wide_limb_t u0 = (divisorLen > 1) ? normalizedDividend[j + divisorLen - 2] : 0;

        wide_limb_t numerator = (u2 << limb_bits) | u1;
        wide_limb_t quotientHat = numerator / normalizedDivisor[divisorLen - 1];
        wide_limb_t remainderHat = numerator % normalizedDivisor[divisorLen - 1];

        if (quotientHat >= base) {
            quotientHat = base - 1;
            remainderHat += normalizedDivisor[divisorLen - 1];
        }

        if (divisorLen > 1) {
            while (quotientHat * normalizedDivisor[divisorLen - 2] > (remainderHat << limb_bits) + u0) {
                --quotientHat;
                remainderHat += normalizedDivisor[divisorLen - 1];
                if (remainderHat >= base) break;
            }
        }

        wide_limb_t borrow = 0;
        wide_limb_t carry = 0;
        for (int i = 0; i < divisorLen; ++i) {
            wide_limb_t p = quotientHat * normalizedDivisor[i] + carry;
            carry = p >> limb_bits;
            wide_limb_t productLow = static_cast<limb_t>(p);

            wide_limb_t current = normalizedDividend[j + i];
            wide_limb_t diff = current - productLow - borrow;
            normalizedDividend[j + i] = static_cast<limb_t>(diff);
            borrow = (current < productLow + borrow) ? 1 : 0;
        }

        wide_limb_t currentTop = normalizedDividend[j + divisorLen];
        wide_limb_t diffTop = currentTop - carry - borrow;
        normalizedDividend[j + divisorLen] = static_cast<limb_t>(diffTop);
        bool underflow = currentTop < carry + borrow;

        if (underflow) {
            --quotientHat;
            wide_limb_t c = 0;
            for (int i = 0; i < divisorLen; ++i) {
                wide_limb_t sum = wide_limb_t(normalizedDividend[j + i]) + normalizedDivisor[i] + c;
                normalizedDividend[j + i] = static_cast<limb_t>(sum);
                c = sum >> limb_bits;
            }
            normalizedDividend[j + divisorLen] = static_cast<limb_t>(wide_limb_t(normalizedDividend[j + divisorLen]) + c);
        }

        quotient[j] = static_cast<limb_t>(quotientHat);
    }

    remainder.assign(normalizedDividend.begin(), normalizedDividend.begin() + divisorLen);
    if (normalizeShift) {
        right_shift(remainder, normalizeShift);
    }

    while (quotient.size() > 1 && quotient.back() == 0) quotient.pop_back();
    while (remainder.size() > 1 && remainder.back() == 0) remainder.pop_back();
}

vector<limb_t> bigint_add(const vector<limb_t>& a, const vector<limb_t>& b) {
    size_t num = max(a.size(), b.size());
    vector<limb_t> result(num);
    wide_limb_t carry = 0;
    for (size_t i = 0; i < num; ++i) {
        wide_limb_t av = i < a.size() ? a[i] : 0;
        wide_limb_t bv = i < b.size() ? b[i] : 0;
        wide_limb_t sum = av + bv + carry;
        result[i] = static_cast<limb_t>(sum);
        carry = sum >> limb_bits;
    }
    if (carry) result.push_back(static_cast<limb_t>(carry));
    return result;
}

void bigint_add_inplace(vector<limb_t>& a, const vector<limb_t>& b) {
    if (b.empty()) return;
    if (a.size() < b.size()) a.resize(b.size(), 0);

    wide_limb_t carry = 0;
    size_t i = 0;

    for (; i < b.size(); ++i) {
        wide_limb_t sum = wide_limb_t(a[i]) + b[i] + carry;
        a[i] = static_cast<limb_t>(sum);
        carry = sum >> limb_bits;
    }

    for (; i < a.size() && carry; ++i) {
        wide_limb_t sum = wide_limb_t(a[i]) + carry;
        a[i] = static_cast<limb_t>(sum);
        carry = sum >> limb_bits;
    }

    if (carry) a.push_back(static_cast<limb_t>(carry));
}

void bigint_sub_inplace(vector<limb_t>& a, const vector<limb_t>& b) {
    wide_limb_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        wide_limb_t aWord = a[i];
        wide_limb_t bWord = i < b.size() ? b[i] : 0;
        wide_limb_t diff = aWord - bWord - borrow;
        borrow = (aWord < bWord + borrow) ? 1 : 0;
        a[i] = static_cast<limb_t>(diff);
    }
    while (a.size() > 1 && a.back() == 0) a.pop_back();
}

void bigint_mul_limb(vector<limb_t>& a, limb_t b) {
    wide_limb_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        wide_limb_t prod = wide_limb_t(a[i]) * b + carry;
        a[i] = static_cast<limb_t>(prod);
        carry = prod >> limb_bits;
    }
    if (carry) a.push_back(static_cast<limb_t>(carry));
}

void bigint_add_limb(vector<limb_t>& a, limb_t b) {
    wide_limb_t carry = b;
    for (size_t i = 0; i < a.size(); ++i) {
        wide_limb_t sum = wide_limb_t(a[i]) + carry;
        a[i] = static_cast<limb_t>(sum);
        carry = sum >> limb_bits;
        if (carry == 0) break;
    }
    if (carry) a.push_back(static_cast<limb_t>(carry));
}

} // namespace BigInt
