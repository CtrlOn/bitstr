// Logarithm calculations
#include "bitstr.h"
#include "bigint.h"

#define LN_PRECISION 448 // default target precision bits for logarithm

using namespace std;
using namespace BigInt;

const BitString BitString::LN_2 = BitString::fromString(
    "0.693147180559945309417232121458176568075500134360255254120680009493393621969694715605863326996418687"
    "542001481020570685733685520235758130557032670751635075961930727570828371435190307038623891673471123"
);

static const BitString SQRT2 = BitString::fromString(
    "1.414213562373095048801688724209698078569671875376948073176679737990732478462107038850387534327641572"
    "735013846230912297024924836055850737212644121497099935831413222665927505592755799950501152782060571"
);

static const BitString INV_SQRT2 = BitString::fromString(
    "0.707106781186547524400844362104849039284835937688474036588339868995366239231053519425193767163820786"
    "367506923115456148512462418027925368606322060748549967915706611332963752796377899975250576391030285"
);

///FIXME: beyond this point downwards


static BitString arctanhSeries(const BitString& t, int precision) {

    BitString t2 = BitString::mul(t, t, precision * 1.5f);
    BitString term = t;
    BitString sum = term;
    int n = 1;
    const BitString epsilonAbs(0, {1}, -precision);

    while (BitString::abs(term) > epsilonAbs) {
        term = BitString::mul(term, t2, precision * 1.5f);
        n += 2;
        BitString termDivN = BitString::div(term, BitString(n), precision * 1.5f);
        sum = sum + termDivN;
    }
    return sum;
}

/// Compute ln(m) for m in [1,2)
static BitString lnMantissa(const BitString& m, int precision) {
    BitString one("1");
    BitString t = BitString::div(m - one, m + one, precision * 1.5f); // (m-1)/(m+1)
    BitString sum = arctanhSeries(t, precision);
    return BitString::mul(BitString(2), sum, precision * 1.5f);
}

BitString BitString::ln(const BitString& n, int precision) {
    if (n.sign || n.isZero()) {
        throw domain_error("ln of non-positive number");

    }
    if (n.isOne()) return BitString();

    int L = bit_length(n.mantissa); // total bits in mantissa
    int64_t k = n.exponent + L - 1;
    BitString m(n.sign, n.mantissa, -(L-1));
    m.normalize();

    // Tighten into [1/sqrt(2), sqrt(2)] for faster atanh convergence.
    while (m > SQRT2) {
        m = BitString::div(m, TWO, precision + limb_bits);
        k += 1;
    }
    while (m < INV_SQRT2) {
        m = BitString::mul(m, TWO, precision + limb_bits);
        k -= 1;
    }

    // Compute ln(m)
    BitString lnM = lnMantissa(m, precision);

    // Use precomputed high‑precision LN_2 constant
    BitString kBs(to_string(k));
    BitString kTerm = BitString::mul(LN_2, kBs);
    BitString result = lnM + kTerm;

    result.normalize();
    return result.truncate(precision);
}

BitString BitString::ln(const BitString& n) {
    return ln(n, LN_PRECISION);
}