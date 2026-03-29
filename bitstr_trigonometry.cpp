// Trigonometry calculations
#include "bitstr.h"

#define SIN_PRECISION 384 // default target precision bits for sin/cos

using namespace std;

static pair<BitString, BitString> sincosCore(const BitString& n) {
    BitString x = n % BitString::TAU;
    if (x < 0) x = x + BitString::TAU;

    static const BitString threeHalfPi = BitString::PI + BitString::HALF_PI;
    int quadrant = 0;

    // x is in [0, TAU), so three comparisons determine the quadrant without
    // requiring any BitString-to-int conversion.
    if (x < BitString::HALF_PI) {
        quadrant = 0;
    } else if (x < BitString::PI) {
        quadrant = 1;
        x = x - BitString::HALF_PI;
    } else if (x < threeHalfPi) {
        quadrant = 2;
        x = x - BitString::PI;
    } else {
        quadrant = 3;
        x = x - threeHalfPi;
    }

    // keep halving until angle is tiny enough for short Taylor series
    int k = 0;
    static const int HALVING_COEFF = 40;
    BitString smallAngle(0, {1}, -HALVING_COEFF);

    while (x > smallAngle) {
        x = x / 2;
        ++k;
    }

    BitString sinX = x;
    BitString cosX = BitString::ONE;
    BitString termSin = x;
    BitString termCos = BitString::ONE;
    BitString x2 = BitString::mul(x, x, SIN_PRECISION * 1.4f);
    const BitString epsilon(0, {1}, -SIN_PRECISION);

    for (int i = 1; i * i < SIN_PRECISION; ++i) {
        termSin = BitString::div(-termSin * x2, BitString(((2 * i) * (2 * i + 1))), SIN_PRECISION);
        sinX = sinX + termSin;
        if (BitString::abs(termSin) < BitString::abs(sinX) * epsilon) break;
    }

    for (int i = 1; i * i < SIN_PRECISION; ++i) {
        termCos = BitString::div(-termCos * x2, BitString(((2 * i - 1) * (2 * i))), SIN_PRECISION);
        cosX = cosX + termCos;
        if (BitString::abs(termCos) < BitString::abs(cosX) * epsilon) break;
    }

    for (int i = 0; i < k; ++i) {
        BitString newSin = BitString::TWO * BitString::mul(sinX, cosX, SIN_PRECISION * 1.4f);
        BitString newCos = BitString::TWO * BitString::mul(cosX, cosX, SIN_PRECISION * 1.4f) - BitString::ONE;
        sinX = newSin;
        cosX = newCos;
    }

    if (quadrant == 0) return {sinX, cosX};
    if (quadrant == 1) return {cosX, -sinX};
    if (quadrant == 2) return {-sinX, -cosX};
    return {-cosX, sinX};
}

const BitString BitString::PI = BitString::fromString(
    "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067"
    "982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381"
);

const BitString BitString::TAU = BitString(PI.sign, PI.mantissa, PI.exponent + 1);

const BitString BitString::HALF_PI = BitString(PI.sign, PI.mantissa, PI.exponent - 1);


BitString BitString::sin(const BitString& n) {
    return sincosCore(n).first;
}

BitString BitString::cos(const BitString& n) {
    return sincosCore(n).second;
}