// Prime calculations
#include "bitstr.h"
#include "bigint.h"

#include <cstdint>

using namespace std;
using namespace BigInt;

static const vector<BitString> LOW_PRIMES_200 = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37,
    41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83,
    89, 97, 101, 103, 107, 109, 113, 127, 131,
    137, 139, 149, 151, 157, 163, 167, 173, 179,
    181, 191, 193, 197, 199
};

static bool isPrimeU64(uint64_t n) {
    if (n < 2) return false;
    if ((n & 1ULL) == 0ULL) return n == 2;
    if (n % 3ULL == 0ULL) return n == 3;

    for (uint64_t d = 5; d <= n / d; d += 6) {
        if (n % d == 0ULL || n % (d + 2ULL) == 0ULL) {
            return false;
        }
    }
    return true;
}

static bool tryToU64Integer(const BitString& x, uint64_t& out) {
    if (x.getSign() || x.getExponent() < 0) {
        return false;
    }

    const vector<limb_t>& m = x.getMantissa();
    const int mBits = bit_length(m);
    const int64_t totalBits = static_cast<int64_t>(mBits) + x.getExponent();
    if (totalBits > 64) {
        return false;
    }

    uint64_t value = 0;
    if constexpr (limb_bits == 64) {
        if (m.size() > 1) {
            return false;
        }
        value = static_cast<uint64_t>(m[0]);
    } else {
        for (int i = static_cast<int>(m.size()) - 1; i >= 0; --i) {
            value = (value << limb_bits) | static_cast<uint64_t>(m[i]);
        }
    }

    if (x.getExponent() > 0) {
        value <<= x.getExponent();
    }

    out = value;
    return true;
}

static bool isEvenInteger(const BitString& x) {
    if (x.getExponent() < 0) return false;
    if (x.getExponent() > 0) return true;
    const vector<limb_t>& m = x.getMantissa();
    return (m[0] & 1u) == 0u;
}

// Modular exponentiation: (base^exp) % mod
static BitString powMod(const BitString& base, const BitString& exp, const BitString& mod) {
    BitString result(BitString::ONE);
    BitString b = base % mod;
    BitString e = exp;
    const BitString two = BitString::TWO;
    const BitString one = BitString::ONE;
    while (e > 0) {
        // For normalized positive integers: exponent == 0 => odd, exponent > 0 => even.
        // Fall back to modulo parity if e is unexpectedly non-integer.
        const int64_t eExp = e.getExponent();
        const bool eOdd = (eExp == 0) || (eExp < 0 && (e % two == one));

        if (eOdd)
            result = (result * b) % mod;
        b = (b * b) % mod;

        // Integer floor-halving without modulo for integer exponents.
        if (eExp == 0) {
            e = (e - one) / two;
        } else {
            e = e / two;
        }
    }
    return result;
}

// Miller‑Rabin test for a single base a (a < n, n odd > 2)
static bool millerRabinTest(const BitString& n, const BitString& a) {
    // Write n-1 = d * 2^s
    BitString d = n - 1;
    int s = 0;
    while (d % BitString::TWO == 0) {
        d = d / BitString::TWO;
        s++;
    }
    BitString x = powMod(a, d, n);
    if (x == 1 || x == n - 1)
        return true;
    for (int i = 0; i < s - 1; ++i) {
        x = (x * x) % n;
        if (x == n - 1)
            return true;
        if (x == 1)   // composite
            return false;
    }
    return false;
}

// Deterministic primality test using trial division + Miller‑Rabin
bool BitString::isPrime() const {
    const BitString& n = *this;
    // Handle small cases
    if (n.sign || n <= 1) return false;

    // Only integers can be prime.
    if (n.exponent < 0) return false;

    uint64_t n64 = 0;
    if (tryToU64Integer(n, n64)) {
        return isPrimeU64(n64);
    }

    // In normalized form, exponent > 0 implies an even integer (factor 2^exponent).
    // The only even prime is exactly 2.
    if (n.exponent > 0) {
        return n.mantissa.size() == 1 && n.mantissa[0] == 1 && n.exponent == 1;
    }

    // At this point exponent == 0, so n is an odd-integer candidate.
    if (n == 2) return true;

    static const vector<BitString> lowPrimes(
        LOW_PRIMES_200.begin(), LOW_PRIMES_200.begin() + 25
    );

    // Trial division by small primes
    for (const BitString& p : lowPrimes) {
        if (p * p > n) break;
        if (n % p == 0) return false;
    }

    static const vector<BitString> bases(
        LOW_PRIMES_200.begin(), LOW_PRIMES_200.begin() + 12
    );

    for (const BitString& a : bases) {
        if (a >= n) break;
        if (!millerRabinTest(n, a))
            return false;
    }
    return true;
}

// Next prime strictly greater than n
BitString BitString::nextP(const BitString& n) {
    if (n < 2) return BitString(2);

    uint64_t n64 = 0;
    if (tryToU64Integer(n, n64) && n64 < (UINT64_MAX - 2)) {
        uint64_t candidate64 = n64 + 1;
        if ((candidate64 & 1ULL) == 0ULL) {
            ++candidate64;
        }
        while (!isPrimeU64(candidate64)) {
            candidate64 += 2;
        }
        return BitString(to_string(candidate64));
    }

    BitString candidate = n;

    // Ceil to integer for non-integer inputs so the loop cannot get stuck on fractional values.
    if (candidate.exponent < 0) {
        BitString frac = BitString::rem(candidate, ONE);
        candidate = candidate - frac;
        if (candidate < n) {
            candidate = candidate + ONE;
        }
    }

    candidate = candidate + ONE;

    // Ensure we start with an odd integer.
    if (isEvenInteger(candidate)) {
        candidate = candidate + ONE;
    }

    while (!candidate.isPrime()) {
        candidate = candidate + TWO;
    }
    return candidate;
}