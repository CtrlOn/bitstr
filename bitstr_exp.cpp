// Exponential calculations
#include "bitstr.h"

#define SQRT_PRECISION 340 // How many bits of precision to use for ln and sqrt (99 decimal digits + guard bits)

using namespace std;

BitString BitString::pow(const BitString& n, int e) {
    if (e < 0) {
        return div(BitString(1), pow(n, -e));
    }
    BitString result(1);
    BitString base = n;
    while (e > 0) {
        if (e & 1) {
            result = mul(result, base);
        }
        base = mul(base, base);
        e >>= 1;
    }
    return result;
}

BitString BitString::ln(const BitString& n) {
    if (n.isZero() || n.sign) {
        throw domain_error("Log undefined for non-positive values");
    }
    return BitString(); // TODO:
}

BitString BitString::sqrt(const BitString& n) {
    if (n.sign) throw std::domain_error("Sqrt undefined for negative values");
    if (n.isZero()) return BitString();

    // Initial guess: use half the exponent and mantissa = 1
    int64_t exp = n.exponent;
    int64_t guessExp = exp / 2;
    std::vector<uint32_t> guessMant = {1};
    BitString guess(false, guessMant, guessExp);
    guess.normalize();

    BitString two("2");
    BitString epsilon(false, {1}, -SQRT_PRECISION);   // 2^(-SQRT_PRECISION)
    BitString prev;

    for (int i = 0; true; ++i) {
        // Newton iteration: next = (guess + n/guess) / 2
        BitString inv = div(n, guess, SQRT_PRECISION + 10);
        BitString next = div(guess + inv, two, SQRT_PRECISION + 10);

        if (i > 0) {
            BitString diff = abs(next - prev);
            if (diff < epsilon) break;
        }
        prev = next;
        guess = next;
    }
    guess.normalize();
    return guess;
}