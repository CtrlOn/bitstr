#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "bitstr.h"

namespace {

volatile std::size_t g_sink = 0;

BitString bs(const std::string& s) {
    return BitString::fromString(s);
}

BitString bs(const std::string& s, int precision) {
    return BitString(s, precision);
}

std::string str(const BitString& v) {
    return BitString::toString(v);
}

bool endsWith(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) {
        return false;
    }
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string canonicalDecimal(const std::string& input) {
    if (input.empty()) {
        return "0.0";
    }

    std::string s = input;
    bool neg = false;
    if (!s.empty() && s[0] == '-') {
        neg = true;
        s.erase(s.begin());
    }

    std::size_t dot = s.find('.');
    if (dot == std::string::npos) {
        s += ".0";
    } else {
        std::string intPart = s.substr(0, dot);
        std::string fracPart = s.substr(dot + 1);
        while (!fracPart.empty() && fracPart.back() == '0') {
            fracPart.pop_back();
        }
        if (fracPart.empty()) {
            s = intPart + ".0";
        } else {
            s = intPart + "." + fracPart;
        }
    }

    if (s == "0.0") {
        return "0.0";
    }
    return neg ? ("-" + s) : s;
}

int commonPrefixLen(const std::string& a, const std::string& b) {
    const std::size_t n = std::min(a.size(), b.size());
    std::size_t i = 0;
    while (i < n && a[i] == b[i]) {
        ++i;
    }
    return static_cast<int>(i);
}

void consume(const BitString& v) {
    g_sink += v.getMantissa().size();
    g_sink += static_cast<std::size_t>(v.getExponent() & 0xFF);
    g_sink += v.getSign() ? 1U : 0U;
}

struct TestRunner {
    int failures = 0;

    void check(const std::string& name, bool ok) {
        if (ok) {
            std::cout << "PASS: " << name << '\n';
            return;
        }
        std::cout << "FAIL: " << name << '\n';
        ++failures;
    }

    void checkEq(const std::string& name, const std::string& expected, const std::string& got) {
        const std::string normExpected = canonicalDecimal(expected);
        const std::string normGot = canonicalDecimal(got);

        if (normExpected == normGot) {
            std::cout << "PASS: " << name << '\n';
            return;
        }
        std::cout << "FAIL: " << name << '\n';
        std::cout << "  Expected: '" << expected << "' (canonical: '" << normExpected << "')\n";
        std::cout << "  Got:      '" << got << "' (canonical: '" << normGot << "')\n";
        ++failures;
    }

    void checkEqRaw(const std::string& name, const std::string& expected, const std::string& got) {
        if (expected == got) {
            std::cout << "PASS: " << name << '\n';
            return;
        }
        std::cout << "FAIL: " << name << '\n';
        std::cout << "  Expected: '" << expected << "'\n";
        std::cout << "  Got:      '" << got << "'\n";
        ++failures;
    }

    void checkThrows(const std::string& name, const std::function<void()>& fn) {
        try {
            fn();
            std::cout << "FAIL: " << name << " (no exception)\n";
            ++failures;
        } catch (const std::exception&) {
            std::cout << "PASS: " << name << '\n';
        }
    }
};

void runFormattingRuleTests(TestRunner& tr) {
    tr.checkEqRaw("fmt integer 1", "1.0", str(bs("1")));
    tr.checkEqRaw("fmt integer -42", "-42.0", str(bs("-42")));
    tr.checkEqRaw("fmt 0.5 trimmed", "0.5", str(bs("0.5")));
    tr.checkEqRaw("fmt 0.125 trimmed", "0.125", str(bs("0.125")));
    tr.checkEqRaw("fmt -7.25 trimmed", "-7.25", str(bs("-7.25")));

    const std::string oneSixth = "0.166666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666667";
    const std::string twoThirds = "0.666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666667";
    tr.checkEqRaw("fmt rounding 1/6", oneSixth, str(bs("1") / bs("6")));
    tr.checkEqRaw("fmt rounding 2/3", twoThirds, str(bs("2") / bs("3")));

    const std::vector<std::string> exoticCases = {
        "0.0000000000000000000001",
        "999999999999999999999999999999999999.0000000000000000000000001",
        "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068",
        "-0.00000000000000000000000000000000000000000000000001"
    };

    for (const std::string& input : exoticCases) {
        const std::string out = str(bs(input));
        tr.check("fmt no scientific " + input, out.find('e') == std::string::npos && out.find('E') == std::string::npos);
        const std::size_t dot = out.find('.');
        tr.check("fmt has dot " + input, dot != std::string::npos);
        if (dot != std::string::npos) {
            const std::size_t fracLen = out.size() - dot - 1;
            tr.check("fmt <=99 fractional digits " + input, fracLen <= 99);
            if (fracLen > 0 && !endsWith(out, ".0")) {
                tr.check("fmt no trailing zero " + input, out.back() != '0');
            }
        }
    }
}

void runRoundtripTests(TestRunner& tr) {
    const std::vector<std::string> values = {
        "0", "1", "-1", "10", "1000000000000000000",
        "1.0", "0.5", "-123.456", "123.456789",
        "0.0000000000000000000001",
        "10.000000000000090000000000000900000000000009",
        "0.9999999999999999999999"
    };

    for (const std::string& input : values) {
        std::string expected = input;
        if (expected.find('.') == std::string::npos) {
            expected += ".0";
        }
        tr.checkEq("roundtrip " + input, expected, str(bs(input)));
    }
}

void runArithmeticTests(TestRunner& tr) {
    tr.checkEq("add 123+456", "579.0", str(bs("123") + bs("456")));
    tr.checkEq("sub 1000-1", "999.0", str(bs("1000") - bs("1")));
    tr.checkEq("add -5+3", "-2.0", str(bs("-5") + bs("3")));

    tr.checkEq("add 1.2+3.4", "4.6", str(bs("1.2") + bs("3.4")));
    tr.checkEq("sub 5.5-2.2", "3.3", str(bs("5.5") - bs("2.2")));
    tr.checkEq("add 0.1+0.2", "0.3", str(bs("0.1") + bs("0.2")));

    tr.checkEq("mul 1.5*2.0", "3.0", str(bs("1.5") * bs("2.0")));
    tr.checkEq("mul 0.5*0.5", "0.25", str(bs("0.5") * bs("0.5")));
    tr.checkEq("mul 1.23*4.56", "5.6088", str(bs("1.23") * bs("4.56")));
    tr.checkEq("mul 9999*9999", "99980001.0", str(bs("9999") * bs("9999")));
    tr.checkEq("mul 123456789*987654321", "121932631112635269.0", str(bs("123456789") * bs("987654321")));
    tr.checkEq("mul 0.0000000000000000000001*10000000000000000000", "1.0", str(bs("0.0000000000000000001", 416) * bs("10000000000000000000", 416)));
}

void runComparisonTests(TestRunner& tr) {
    tr.check("cmp 1<2", bs("1") < bs("2"));
    tr.check("cmp 2>1", bs("2") > bs("1"));
    tr.check("cmp 1<=1", bs("1") <= bs("1"));
    tr.check("cmp 1>=1", bs("1") >= bs("1"));
    tr.check("cmp -1<0", bs("-1") < bs("0"));
    tr.check("cmp 0.02>0.019", bs("0.02") > bs("0.019"));
}

void runDivisionAndModuloTests(TestRunner& tr) {
    const std::vector<std::tuple<std::string, std::string, std::string>> divCases = {
        {"10", "2", "5.0"},
        {"7", "2", "3.5"},
        {"50", "8", "6.25"},
        {"1", "3", "0.333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333"},
        {"2", "1.5", "1.333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333"},
        {"1", "6", "0.166666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666667"}
    };

    for (const auto& c : divCases) {
        const std::string& a = std::get<0>(c);
        const std::string& b = std::get<1>(c);
        const std::string& expected = std::get<2>(c);
        tr.checkEq("div " + a + "/" + b, expected, str(bs(a) / bs(b)));
    }

    tr.checkEq("mod 10%3", "1.0", str(BitString::mod(bs("10"), bs("3"))));
    tr.checkEq("mod -10%3", "2.0", str(BitString::mod(bs("-10"), bs("3"))));
    tr.checkEq("mod 100000000000000000000%42", "16.0", str(BitString::mod(bs("100000000000000000000"), bs("42"))));
}

void runConstantsAndExponentialTests(TestRunner& tr) {
    tr.checkEq(
        "PI 100 digits",
        "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068",
        str(BitString::PI)
    );
    tr.checkEq(
        "LN_2 100 digits",
        "0.693147180559945309417232121458176568075500134360255254120680009493393621969694715605863326996418688",
        str(BitString::LN_2)
    );

    tr.checkEq("pow 2^10", "1024.0", str(BitString::pow(bs("2"), 10)));
    tr.checkEq("pow 2^-2", "0.25", str(BitString::pow(bs("2"), -2)));
    tr.checkEq("pow 1.5^3", "3.375", str(BitString::pow(bs("1.5"), 3)));
    tr.checkEq("pow 5^0", "1.0", str(BitString::pow(bs("5"), 0)));
    tr.checkEq("pow 0^5", "0.0", str(BitString::pow(bs("0"), 5)));
    tr.checkEq("pow 0^0", "1.0", str(BitString::pow(bs("0"), 0)));
    tr.checkEq("pow out of precision", "0.0", str(BitString::pow(bs("0."+std::string(98, '0')+"1"), 2)));
    tr.checkEq("pow just outside precision", "0.0", str(BitString::pow(bs("0."+std::string(49, '0')+"1"), 2)));
    tr.checkEq("pow just inside precision", "0."+std::string(97, '0')+"1", str(BitString::pow(bs("0."+std::string(48, '0')+"1"), 2)));
    tr.checkEq("pow high exponent", "246993291800582633412408838508522147770973.338523839623486918295183073939037543317536786611645694619197380356118903652336353379872657100896124", str(BitString::pow(bs("1.1", 1200), 1000)));
    tr.checkEq("sqrt 4", "2.0", str(BitString::sqrt(bs("4"))));
    tr.checkEq("sqrt 0.01", "0.1", str(BitString::sqrt(bs("0.01"))));
    tr.checkEq(
        "sqrt 2",
        "1.414213562373095048801688724209698078569671875376948073176679737990732478462107038850387534327641573",
        str(BitString::sqrt(bs("2")))
    );

    tr.checkEq("ln 1", "0.0", str(BitString::ln(bs("1"))));
    tr.checkEq(
        "ln 5",
        "1.609437912434100374600759333226187639525601354268517721912647891474178987707657764630133878093179611",
        str(BitString::ln(bs("5")))
    );
    tr.checkEq(
        "ln 0.5",
        "-0.693147180559945309417232121458176568075500134360255254120680009493393621969694715605863326996418688",
        str(BitString::ln(bs("0.5")))
    );
}

void runSqrtFocusedTests(TestRunner& tr) {
    const int HI_INPUT_PREC = 384+128;
    const int HI_SQRT_PREC = 384;

    const std::vector<std::pair<std::string, std::string>> exactSqrtCases = {
        {"0", "0.0"},
        {"1", "1.0"},
        {"4", "2.0"},
        {"9", "3.0"},
        {"144", "12.0"},
        {"100000000000000000000", "10000000000.0"},
        {"10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", "100000000000000000000000000000000000000000000000000.0"},
        {"15241578750190521", "123456789.0"},
        {"0.0004", "0.02"},
        {"0.00000000000000000000000001", "0.0000000000001"}
    };

    for (const auto& c : exactSqrtCases) {
        tr.checkEq("sqrt exact " + c.first, c.second, str(BitString::sqrt(bs(c.first))));
    }

    const std::string tinyExactSq =
        "0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001";
    tr.checkEq(
        "sqrt exact tiny (hi-precision input)",
        "0.00000000000000000000000000000000000000000000000001",
        str(BitString::sqrt(bs(tinyExactSq, HI_INPUT_PREC), HI_SQRT_PREC))
    );

    tr.checkEq(
        "sqrt 3",
        "1.732050807568877293527446341505872366942805253810380628055806979451933016908800037081146186757248576",
        str(BitString::sqrt(bs("3")))
    );
    tr.checkEq(
        "sqrt 0.2",
        "0.447213595499957939281834733746255247088123671922305144854179449082104185127560979882882881675756455",
        str(BitString::sqrt(bs("0.2", HI_INPUT_PREC), HI_SQRT_PREC))
    );
    tr.checkEq(
        "sqrt 0.5",
        "0.707106781186547524400844362104849039284835937688474036588339868995366239231053519425193767163820786",
        str(BitString::sqrt(bs("0.5", HI_INPUT_PREC), HI_SQRT_PREC))
    );

    const BitString sqrt2 = BitString::sqrt(bs("2"));
    const BitString squared = sqrt2 * sqrt2;
    const BitString err = BitString::abs(squared - bs("2"));
    tr.check("sqrt stability (sqrt(2)^2 ~= 2)", err < bs("0." + std::string(94, '0') + "1"));

    for (int n = 0; n <= 25; ++n) {
        const BitString b = bs(std::to_string(n));
        const BitString sq = b * b;
        tr.checkEq("sqrt(n^2) exact n=" + std::to_string(n), str(b), str(BitString::sqrt(sq)));
    }

    const std::vector<std::string> hardNonSquares = {
        "2", "3", "5", "6", "7", "8", "10", "99", "123456789", "0.02", "0.2", "0.000000000123456789"
    };

    const BitString tightEps = bs("0." + std::string(92, '0') + "1");
    for (const std::string& xStr : hardNonSquares) {
        const BitString x = bs(xStr);
        const BitString r = BitString::sqrt(x);
        const BitString rr = r * r;
        const BitString absErr = BitString::abs(rr - x);
        tr.check("sqrt residual " + xStr, absErr < tightEps);
    }

    const BitString x = bs("12345.6789");
    const BitString s = BitString::sqrt(x);
    tr.checkEq("sqrt scale x*100", str(s * bs("10")), str(BitString::sqrt(x * bs("100"))));
    tr.checkEq("sqrt scale x/100", str(s / bs("10")), str(BitString::sqrt(x / bs("100"))));

    const BitString y = bs("7.25");
    const BitString one = bs("1");
    const BitString invRelErr = BitString::abs((BitString::sqrt(one / y) * BitString::sqrt(y)) - one);
    tr.check("sqrt reciprocal identity", invRelErr < bs("0." + std::string(91, '0') + "1"));

    const std::vector<std::pair<std::string, std::string>> nearPairs = {
        {"0.9999999999999999999999999999", "1.0"},
        {"1.0", "1.0000000000000000000000000001"},
        {"999999999999999999999999", "1000000000000000000000000"}
    };
    for (const auto& p : nearPairs) {
        tr.check("sqrt monotonic near " + p.first + " < " + p.second,
                 BitString::sqrt(bs(p.first)) < BitString::sqrt(bs(p.second)));
    }

    tr.check("sqrt monotonic 2<3", BitString::sqrt(bs("2")) < BitString::sqrt(bs("3")));
    tr.check("sqrt monotonic 99<100", BitString::sqrt(bs("99")) < BitString::sqrt(bs("100")));

    const BitString tiny = bs("0." + std::string(98, '0') + "1");
    const BitString tinyRoot = BitString::sqrt(tiny);
    tr.check("sqrt tiny non-negative", tinyRoot >= bs("0"));
    tr.check("sqrt tiny <= 1", tinyRoot <= bs("1"));

    const BitString huge = bs("9" + std::string(90, '0'));
    const BitString hugeRoot = BitString::sqrt(huge);
    tr.check("sqrt huge non-negative", hugeRoot >= bs("0"));
    tr.check(
        "sqrt huge relative residual",
        (BitString::abs((hugeRoot * hugeRoot) - huge) / huge) < bs("0." + std::string(80, '0') + "1")
    );

    tr.checkThrows("sqrt negative throws", []() {
        (void)BitString::sqrt(bs("-1"));
    });
    tr.checkThrows("sqrt very negative throws", []() {
        (void)BitString::sqrt(bs("-12345678901234567890.5"));
    });

    const std::vector<std::string> precisionProbeInputs = {
        "0.2",
        "0.5",
        "0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
        "2",
        "3"
    };

    for (const std::string& xStr : precisionProbeInputs) {
        const BitString xVal = bs(xStr, HI_INPUT_PREC);
        const std::string s384 = str(BitString::sqrt(xVal, 384));
        const std::string s1024 = str(BitString::sqrt(xVal, 1024));
        const std::string s1536 = str(BitString::sqrt(xVal, 1536));

        const int p1 = commonPrefixLen(s384, s1024);
        const int p2 = commonPrefixLen(s1024, s1536);

        const bool isExtremeTiny = xVal < bs("0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001");
        const int minP1 = isExtremeTiny ? 40 : 70;
        const int minP2 = 90;

        tr.check("sqrt precision probe 384->1024 stable " + xStr, p1 >= minP1);
        if (!isExtremeTiny) {
            tr.check("sqrt precision probe 1024->1536 stable " + xStr, p2 >= minP2);
        }
    }

    const BitString dec02 = bs("0.2", HI_INPUT_PREC);
    const BitString rat15 = bs("1") / bs("5");
    const std::string sqrtDec02 = str(BitString::sqrt(dec02, 1024));
    const std::string sqrtRat15 = str(BitString::sqrt(rat15, 1024));
    tr.check("sqrt parse-vs-rational 0.2 common prefix >= 90",
             commonPrefixLen(sqrtDec02, sqrtRat15) >= 90);

    const BitString parseGap = BitString::abs(dec02 - rat15);
    tr.check("parse gap 0.2 vs 1/5 tiny", parseGap < bs("0." + std::string(95, '0') + "1"));
}

void runTrigTests(TestRunner& tr) {
    tr.checkEq("sin 0", "0.0", str(BitString::sin(bs("0"))));
    tr.checkEq("sin PI/2", "1.0", str(BitString::sin(BitString::HALF_PI)));
    tr.checkEq("sin PI", "0.0", str(BitString::sin(BitString::PI)));

    tr.checkEq("cos 0", "1.0", str(BitString::cos(bs("0"))));
    tr.checkEq("cos PI", "-1.0", str(BitString::cos(BitString::PI)));
}

void runSpecialTests(TestRunner& tr) {
    tr.checkEq("fact 0", "1.0", str(BitString::fact(0)));
    tr.checkEq("fact 10", "3628800.0", str(BitString::fact(10)));
    tr.checkEq("fact 20", "2432902008176640000.0", str(BitString::fact(20)));

    tr.check("isPrime 2", bs("2").isPrime());
    tr.check("isPrime 97", bs("97").isPrime());
    tr.check("isPrime 100 false", !bs("100").isPrime());

    // Cross the uint64 boundary to force big-number primality path.
    tr.check("isPrime >2^64 prime", bs("18446744073709551629").isPrime());
    tr.check("isPrime >2^64 even composite", !bs("18446744073709551616").isPrime());
    tr.check("isPrime >2^64 odd composite", !bs("18446744073709551617").isPrime());

    const std::vector<std::pair<std::string, std::string>> nextPrimeCases = {
        {"-5", "2.0"},
        {"1", "2.0"},
        {"2", "3.0"},
        {"24", "29.0"},
        {"1000", "1009.0"},
        {"18446744073709551615", "18446744073709551629.0"},
        {"18446744073709551616", "18446744073709551629.0"},
        {"18446744073709551617", "18446744073709551629.0"}
    };

    for (const auto& c : nextPrimeCases) {
        const BitString input = bs(c.first);
        const BitString next = BitString::nextP(input);
        tr.checkEq("nextP " + c.first, c.second, str(next));
        tr.check("nextP is prime " + c.first, next.isPrime());
        tr.check("nextP greater " + c.first, next > input);
    }
}

void runVectorUtilTests(TestRunner& tr) {
    std::vector<BitString> values = {bs("3"), bs("1"), bs("2")};

    BitString::bubbleSort(values);
    tr.checkEq("sort[0]", "1.0", str(values[0]));
    tr.checkEq("sort[1]", "2.0", str(values[1]));
    tr.checkEq("sort[2]", "3.0", str(values[2]));

    tr.checkEq("avg 1,2,3", "2.0", str(BitString::avg(values)));
    tr.check("find 2", BitString::find(values, bs("2")) == 1);
    tr.check("find missing", BitString::find(values, bs("4")) == -1);
}

void runBenchCase(
    const std::string& name,
    int iterations,
    const std::function<void()>& fn
) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        fn();
    }
    const auto end = std::chrono::steady_clock::now();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const double avgUs = static_cast<double>(us) / static_cast<double>(iterations);
    std::cout << "BENCH: " << name << " | iters=" << iterations
              << " | total=" << us << " us"
              << " | avg=" << avgUs << " us/op\n";
}

void runBenchmarks() {
    std::cout << "\n=== BENCHMARKS ===\n";

    const BitString a = bs("12345678901234567890.123456789");
    const BitString b = bs("98765432109876543210.987654321");
    const BitString c = bs("12345.6789");
    const BitString d = bs("99.99");

    runBenchCase("fromString+toString", 300, []() {
        const BitString x = bs("1234567890.0987654321");
        g_sink += str(x).size();
    });

    runBenchCase("add", 1000, [a, b]() {
        consume(a + b);
    });

    runBenchCase("sub", 1000, [b, a]() {
        consume(b - a);
    });

    runBenchCase("mul", 300, [c, d]() {
        consume(c * d);
    });

    runBenchCase("div", 200, [b, c]() {
        consume(b / c);
    });

    runBenchCase("mod", 300, [b, d]() {
        consume(BitString::mod(b, d));
    });

    runBenchCase("comparison", 2000, [a, b]() {
        g_sink += (a < b) ? 1U : 0U;
    });

    runBenchCase("pow", 200, [c]() {
        consume(BitString::pow(c, 7));
    });

    runBenchCase("sqrt", 150, [b]() {
        consume(BitString::sqrt(b));
    });

    runBenchCase("ln", 120, []() {
        consume(BitString::ln(bs("1234.56789")));
    });

    runBenchCase("sin", 120, []() {
        consume(BitString::sin(BitString::PI / bs("7")));
    });

    runBenchCase("cos", 120, []() {
        consume(BitString::cos(BitString::PI / bs("7")));
    });

    runBenchCase("factorial", 25, []() {
        consume(BitString::fact(220));
    });

    runBenchCase("isPrime (u64)", 200, []() {
        g_sink += bs("104729").isPrime() ? 1U : 0U;
    });

    runBenchCase("isPrime (>u64)", 30, []() {
        g_sink += bs("18446744073709551629").isPrime() ? 1U : 0U;
    });

    runBenchCase("nextP (u64)", 120, []() {
        consume(BitString::nextP(bs("100000")));
    });

    runBenchCase("nextP (>u64)", 30, []() {
        consume(BitString::nextP(bs("18446744073709551617")));
    });

    runBenchCase("avg/find/sort", 400, []() {
        std::vector<BitString> v = {bs("7"), bs("2"), bs("5"), bs("3")};
        BitString::bubbleSort(v);
        consume(BitString::avg(v));
        g_sink += static_cast<std::size_t>(BitString::find(v, bs("5")) + 1);
    });

    std::cout << "BENCH_SINK=" << g_sink << '\n';
}

} // namespace

int main() {
    try {
        TestRunner tr;

        runRoundtripTests(tr);
        runFormattingRuleTests(tr);
        runArithmeticTests(tr);
        runComparisonTests(tr);
        runDivisionAndModuloTests(tr);
        runConstantsAndExponentialTests(tr);
        runSqrtFocusedTests(tr);
        runTrigTests(tr);
        runSpecialTests(tr);
        runVectorUtilTests(tr);

        if (tr.failures == 0) {
            std::cout << "ALL TESTS PASSED\n";
        } else {
            std::cout << tr.failures << " TEST(S) FAILED\n";
        }

        runBenchmarks();

        return tr.failures == 0 ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cout << "UNEXPECTED EXCEPTION: " << ex.what() << '\n';
        return 2;
    }
}
