// Trigonometrics
#include "bitstr.h"

#define SIN_PRECISION 350 // number of accurate bits

using namespace std;

// 512 bits of PI (around 150 decimal digits, def overkill)
const BitString BitString::PI = {
    false,
    {
        0x6D51C245,
        0x4FE1356D,
        0xF25F1437,
        0x302B0A6D,
        0xCD3A431B,
        0xEF9519B3,
        0x8E3404DD,
        0x514A0879,
        0x3B139B22,
        0x020BBEA6,
        0x8A67CC74,
        0x29024E08,
        0x80DC1CD1,
        0xC4C6628B,
        0x2168C234,
        0xC90FDAA2,
    },
    -510
};

const BitString BitString::TAU = {
    PI.sign,
    PI.mantissa,
    PI.exponent + 1
};

const BitString BitString::HALF_PI = {
    PI.sign,
    PI.mantissa,
    PI.exponent - 1
};

BitString BitString::sin(const BitString& n) {
    // --- Step 1: Reduce to [0, 2π) ---
    BitString x = n % TAU;
    if (x < 0) x = x + TAU;

    // --- Step 2: Halve until angle is small ---
    int k = 0;
    const BitString SMALL_ANGLE(0.0000001);  // adjust as needed
    while (x > SMALL_ANGLE) {
        x = x * BitString(0.5);   // divide by 2
        k++;
    }

    // --- Step 3: Compute sin and cos of the tiny angle using Taylor series ---
    // sin(x) = x - x³/6 + x⁵/120 - x⁷/5040 + ...
    // cos(x) = 1 - x²/2 + x⁴/24 - x⁶/720 + ...
    BitString sin_x = x;
    BitString cos_x = BitString(1);
    BitString term_sin = x;
    BitString term_cos = BitString(1);
    BitString x2 = mul(x, x, SIN_PRECISION * 1.5f);
    const int MAX_TERMS = 20;
    const BitString EPS = BitString(0, {1}, -SIN_PRECISION); // 2^{-SIN_PRECISION}

    // Sine series
    for (int i = 1; i < MAX_TERMS; ++i) {
        // term_i = - term_{i-1} * x² / ((2i)*(2i+1))
        term_sin = div(-term_sin * x2, BitString((double)((2*i)*(2*i+1))), SIN_PRECISION);
        sin_x = sin_x + term_sin;
        if (abs(term_sin) < abs(sin_x) * EPS) break;
    }

    // Cosine series (restart from 1)
    for (int i = 1; i < MAX_TERMS; ++i) {
        // term_i = - term_{i-1} * x² / ((2i-1)*(2i))
        term_cos = div(-term_cos * x2, BitString((double)((2*i-1)*(2*i))), SIN_PRECISION);
        cos_x = cos_x + term_cos;
        if (abs(term_cos) < abs(cos_x) * EPS) break;
    }

    // --- Step 4: Apply double-angle k times ---
    for (int i = 0; i < k; ++i) {
        BitString new_sin = BitString(2) * mul(sin_x, cos_x, SIN_PRECISION * 1.5f);
        BitString new_cos = BitString(2) * mul(cos_x, cos_x, SIN_PRECISION * 1.5f) - BitString(1);
        sin_x = new_sin;
        cos_x = new_cos;
    }

    return sin_x;
}

BitString BitString::cos(const BitString& n) {
    return sin(n + HALF_PI);
}