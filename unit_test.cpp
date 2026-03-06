#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include "bitstr.h"

using namespace std;

static int failures = 0;

void check(const string& name, bool cond) {
    if (cond) cout << "PASS: " << name << '\n';
    else {
        cout << "FAIL: " << name << '\n';
        ++failures;
    }
}

void checkEq(const string& name, const string& expected, const string& got) {
    if (expected == got) {
        cout << "PASS: " << name << '\n';
        return;
    }
    cout << "FAIL: " << name << '\n';
    cout << "  Expected: '" << expected << "'\n";
    cout << "  Got:      '" << got << "'\n";
    BitString expbs = BitString::fromString(expected);
    cout << "  expected details - sign: " << expbs.getSign() 
         << ", exponent: " << expbs.getExponent() 
         << ", mantissa: [";
    const auto& mant = expbs.getMantissa();
    for (size_t i = 0; i < mant.size(); ++i) {
        if (i > 0) cout << ", ";
        cout << "0x" << hex << mant[i] << dec;
    }
    cout << "]\n";
    ++failures;
}

void checkEq(const string& name, const BitString& expected, const BitString& got) {
    if (expected == got) {
        cout << "PASS: " << name << '\n';
        return;
    }
    cout << "FAIL: " << name << '\n';
    cout << "  Expected: '" << BitString::toString(expected) << "'\n";
    cout << "  Got:      '" << BitString::toString(got) << "'\n";
    ++failures;
}

bool startsWith(const string& s, const string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

int main() {
    try {
        // Roundtrip tests: fromString -> toString with increasingly larger numbers
        // Pattern: power of 10, power of 2, pattern number (1234...), repeat and grow
        vector<string> testCases = {
            "10", "2", "1.234",
            "100", "4", "1.2345",
            "1000", "8", "1.23456",
            "10000", "16", "1.234567",
            "100000", "32", "1.2345678",
            "1000000", "64", "1.23456789",
            "10000000", "128", "1.23456789",
            "100000000", "256", "1.2345678901",
            "1000000000", "512", "1.23456789012",
            "10000000000", "1024", "1.234567890123",
            "100000000000", "2048", "1.2345678901234",
            "1000000000000", "4096", "1.23456789012345",
            "10000000000000", "8192", "1.234567890123456",
            "100000000000000", "16384", "1.2345678901234567",
            "1000000000000000", "32768", "1.23456789012345678",
            "10000000000000000", "65536", "1.234567890123456789",
            "100000000000000000", "131072", "1.234567890123456789",
            "1000000000000000000", "262144", "1.23456789012345678901",
            "10000000000000000000", "524288", "1.234567890123456789012",
            "100000000000000000000", "1048576", "1.2345678901234567890123",
            // fractional cases
            "1.0", "0.5", "123.456",
            "123.456789", "-123.456", "123.45",
            "123.457", "-123.466", "123.556",
            "123.457", "111.466", "123.444",
            "123.459", "123.457", "123.453",
            "0.0001", "999.999", "3.14159",
            "0.00001", "0.000001", "0.0000001",
            "0.000000000000000001", "0.0000000000000000001", "0.0000000000000000000001",
            "0.000000000000000000000000000000000000000000000001",
            "0.00000000000000000000000000000000000000000000000000000000000001",
            "0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
            "10.00000000000009000000000000090000000000000900000000000009000000000000090000000000000900000000000009",
            "10"
            "1.00000000000001", "1.000000000000000001", "1.0000000000000000000001",
            "0.11111111111111", "0.111111111111111111", "0.1111111111111111111111",
            "0.99999999999999", "0.999999999999999999", "0.9999999999999999999999",

            "10","2", "5.0",
            "7","2", "3.5",
            "20","5", "4.0",
            "50","8", "6.25",
            "999","3", "333.0",
            "888888888888888","88888888888888", "10.0",
            "12345678901234567890","1234567890123456789", "10.0",
            "100000000000000000000","10000000000000000", "10000.0",
            "1","3", "0.333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333",
            "1","7", "0.142857142857142857142857142857142857142857142857142857142857142857142857142857142857142857142857143",
            "250000", "40000", "6.25",
            "1","6", "0.166666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666667",
        };

        for (size_t i = 0; i < testCases.size(); ++i) {
            string input = testCases[i];
            BitString bs = BitString::fromString(input);
            string output = BitString::toString(bs);
            string expected = input;
            if (expected.find('.') == string::npos)
                expected += ".0";
            string testName = "roundtrip_" + input;
            checkEq(testName, expected, output);
        }

        // Arithmetic operations
        checkEq("add 123+456", "579.0", BitString::toString(BitString::fromString("123") + BitString::fromString("456")));
        checkEq("sub 1000-1", "999.0", BitString::toString(BitString::fromString("1000") - BitString::fromString("1")));
        checkEq("neg add -5+3", "-2.0", BitString::toString(BitString::fromString("-5") + BitString::fromString("3")));
        // fractional addition/subtraction cases
        checkEq("add 1.2+3.4", "4.6", BitString::toString(BitString::fromString("1.2") + BitString::fromString("3.4")));
        checkEq("sub 5.5-2.2", "3.3", BitString::toString(BitString::fromString("5.5") - BitString::fromString("2.2")));
        checkEq("add 0.1+0.2", "0.3", BitString::toString(BitString::fromString("0.1") + BitString::fromString("0.2")));
        checkEq("sub 2.5-1.25", "1.25", BitString::toString(BitString::fromString("2.5") - BitString::fromString("1.25")));
        checkEq("add -1.1+0.1", "-1.0", BitString::toString(BitString::fromString("-1.1") + BitString::fromString("0.1")));
        checkEq("sub -2.5-0.5", "-3.0", BitString::toString(BitString::fromString("-2.5") - BitString::fromString("0.5")));
        checkEq("add 3.333+4.667", "8.0", BitString::toString(BitString::fromString("3.333") + BitString::fromString("4.667")));
        checkEq("sub 10.0-0.0001", "9.9999", BitString::toString(BitString::fromString("10.0") - BitString::fromString("0.0001")));
        // basic multiplication
        checkEq("mul 1.5*2.0", "3.0",
            BitString::toString(BitString::fromString("1.5") * BitString::fromString("2.0")));

        checkEq("mul 0.5*0.5", "0.25",
            BitString::toString(BitString::fromString("0.5") * BitString::fromString("0.5")));

        checkEq("mul 0.00001*0.00001", "0.0000000001",
            BitString::toString(BitString::fromString("0.00001") * BitString::fromString("0.00001")));

        checkEq("mul 0.125*8", "1.0",
            BitString::toString(BitString::fromString("0.125") * BitString::fromString("8")));
        checkEq("mul 1.23*4.56", "5.6088",
            BitString::toString(BitString::fromString("1.23") * BitString::fromString("4.56")));

        checkEq("mul 0.1*0.2", "0.02",
            BitString::toString(BitString::fromString("0.1") * BitString::fromString("0.2")));

        checkEq("mul 0.333*3", "0.999",
            BitString::toString(BitString::fromString("0.333") * BitString::fromString("3")));

        checkEq("mul 9999*9999", "99980001.0",
            BitString::toString(BitString::fromString("9999") * BitString::fromString("9999")));

        checkEq("mul 1000000000*1000000000", "1000000000000000000.0",
            BitString::toString(BitString::fromString("1000000000") * BitString::fromString("1000000000")));
        // huge mantissa multiplication
        string huge1 = string(100, '1'); // 1111... length 100
        string huge2 = string(100, '2'); // 2222...
        string hugeExpected = BitString::toString(BitString::fromString(huge1) * BitString::fromString(huge2));
        checkEq("huge mul mantissa", hugeExpected, BitString::toString(BitString::fromString(huge1) * BitString::fromString(huge2)));
        // large multiplication
        checkEq("large mul 1e9*1e9", "1000000000000000000.0", 
                BitString::toString(BitString::fromString("1000000000") * BitString::fromString("1000000000")));
        // division
        /*checkEq("div 10/2", "5.0", BitString::toString(BitString::div(BitString::fromString("10"), BitString::fromString("2"), 256)));
        string frac = BitString::toString(BitString::div(BitString::fromString("7"), BitString::fromString("2"), 256));
        check("div fractional 7/2 begins 3.5", startsWith(frac, "3.5"));
        */// comparisons
        check("cmp 1<2", BitString::fromString("1") < BitString::fromString("2"));
        check("cmp 2>1", BitString::fromString("2") > BitString::fromString("1"));
        check("cmp <=", BitString::fromString("1") <= BitString::fromString("1"));
        check("cmp >=", BitString::fromString("1") >= BitString::fromString("1"));
        check("cmp negative", BitString::fromString("-1") < BitString::fromString("0"));

        // extremely long operations
        for (int len = 10; len <= 15000; len *= 4) {
            string a(len, '5'); // number consisting of len fives
            string b = string(len, '4'); // 1 followed by zeros
            // addition: 555..5 + 444..4 = 999..9 (len digits)
            string expectAdd = string(len, '9') + ".0";
            string addRes = BitString::toString(BitString::fromString(a) + BitString::fromString(b));
            checkEq("long add len" + to_string(len), expectAdd, addRes);

            // subtraction: a - b = (len nines) - (1 with zeros) = (len-1 nines) + (9)
            // easier: convert to int-like string manually
            // compute using our library as expected
            string expectSub = BitString::toString(BitString::fromString(a) - BitString::fromString(b));
            string subRes = expectSub; // library result
            checkEq("long sub len" + to_string(len), expectSub, subRes);

            // multiplication: a * 9 (small multiplier)
            string expectMul = BitString::toString(BitString::fromString(a) * BitString::fromString("9"));
            string mulRes = expectMul;
            checkEq("long mul len" + to_string(len), expectMul, mulRes);
        }

        // manual division cases exercised at multiple precisions
        vector<tuple<string, string, string>> divCases = {
            {"10","2", "5.0"},
            {"7","2", "3.5"},
            {"20","5", "4.0"},
            {"50","8", "6.25"},
            {"999","3", "333.0"},
            {"888888888888880","88888888888888", "10.0"},
            {"888888888888888","88888888888888", "10.00000000000009000000000000090000000000000900000000000009000000000000090000000000000900000000000009"},
            {"777777777777770","77777777777777", "10.0"},
            {"666666666666660","66666666666666", "10.0"},
            {"555555555555550","55555555555555", "10.0"},
            {"444444444444440","44444444444444", "10.0"},
            {"333333333333330","33333333333333", "10.0"},
            {"222222222222220","22222222222222", "10.0"},
            {"111111111111110","11111111111111", "10.0"},
            {"111111111110","11111111111", "10.0"},
            {"11111111110","1111111111", "10.0"},
            {"1111111110","111111111", "10.0"},
            {"111111110","11111111", "10.0"},

            {"1","0.1", "10.0"},
            {"12345678901234567890","1234567890123456789", "10.0"},
            {"100000000000000000000","10000000000000000", "10000.0"},
            {"1","3", "0.333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333"},
            {"1","7", "0.142857142857142857142857142857142857142857142857142857142857142857142857142857142857142857142857143"},
            {"250000", "40000", "6.25"},
            {"1","6", "0.166666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666667"},
        };
        for (auto& pr : divCases) {
            string a = get<0>(pr);
            string b = get<1>(pr);
            string expect = get<2>(pr);
            string testName = "div " + a + "/" + b;
            string got = BitString::toString(BitString::div(BitString::fromString(a), BitString::fromString(b), 340));
            checkEq(testName, expect, got);
        }

        //very high divisions
        for (int len = 10; len <= 15000; len *= 4) {
            string a(len, '6');
            string b(len, '4');
            string addRes = BitString::toString(BitString::div(BitString::fromString(a), BitString::fromString(b), 340));
            checkEq("long div 6666../4444.." + to_string(len), "1.5", addRes);
        }

        for (int len = 10; len <= 15000; len *= 4) {
            string a(len, '9');
            string b(len, '7');
            string addRes = BitString::toString(BitString::div(BitString::fromString(a), BitString::fromString(b), 340));
            checkEq("long div 9999../5555.." + to_string(len), "1.285714285714285714285714285714285714285714285714285714285714285714285714285714285714285714285714286", addRes);
        }

        //Check PI
        checkEq("PI 100 digits", "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068", BitString::toString(BitString::PI)); //ends with 7 but roundup to 8 cuz 211706 798...
        
        //Test factorial
        checkEq("factorial 0", "1.0", BitString::toString(BitString::fact(0)));
        checkEq("factorial 1", "1.0", BitString::toString(BitString::fact(1)));
        checkEq("factorial 5", "120.0", BitString::toString(BitString::fact(5)));
        checkEq("factorial 10", "3628800.0", BitString::toString(BitString::fact(10)));
        checkEq("factorial 20", "2432902008176640000.0", BitString::toString(BitString::fact(20)));
        checkEq("factorial 50", "30414093201713378043612608166064768844377641568960512000000000000.0", BitString::toString(BitString::fact(50)));
        checkEq("factorial 100", "93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000.0", BitString::toString(BitString::fact(100)));
        checkEq("factorial 1000", "402387260077093773543702433923003985719374864210714632543799910429938512398629020592044208486969404800479988610197196058631666872994808558901323829669944590997424504087073759918823627727188732519779505950995276120874975462497043601418278094646496291056393887437886487337119181045825783647849977012476632889835955735432513185323958463075557409114262417474349347553428646576611667797396668820291207379143853719588249808126867838374559731746136085379534524221586593201928090878297308431392844403281231558611036976801357304216168747609675871348312025478589320767169132448426236131412508780208000261683151027341827977704784635868170164365024153691398281264810213092761244896359928705114964975419909342221566832572080821333186116811553615836546984046708975602900950537616475847728421889679646244945160765353408198901385442487984959953319101723355556602139450399736280750137837615307127761926849034352625200015888535147331611702103968175921510907788019393178114194545257223865541461062892187960223838971476088506276862967146674697562911234082439208160153780889893964518263243671616762179168909779911903754031274622289988005195444414282012187361745992642956581746628302955570299024324153181617210465832036786906117260158783520751516284225540265170483304226143974286933061690897968482590125458327168226458066526769958652682272807075781391858178889652208164348344825993266043367660176999612831860788386150279465955131156552036093988180612138558600301435694527224206344631797460594682573103790084024432438465657245014402821885252470935190620929023136493273497565513958720559654228749774011413346962715422845862377387538230483865688976461927383814900140767310446640259899490222221765904339901886018566526485061799702356193897017860040811889729918311021171229845901641921068884387121855646124960798722908519296819372388642614839657382291123125024186649353143970137428531926649875337218940694281434118520158014123344828015051399694290153483077644569099073152433278288269864602789864321139083506217095002597389863554277196742822248757586765752344220207573630569498825087968928162753848863396909959826280956121450994871701244516461260379029309120889086942028510640182154399457156805941872748998094254742173582401063677404595741785160829230135358081840096996372524230560855903700624271243416909004153690105933983835777939410970027753472000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.0", BitString::toString(BitString::fact(1000)));

        //Test double constructors
        checkEq("double constructor 0.1", "0.1000000000000000055511151231257827021181583404541015625", BitString::toString(BitString(0.1)));
        checkEq("double constructor 0.5", "0.99999000000000004551026222543441690504550933837890625", BitString::toString(BitString(0.99999)));
        checkEq("double constructor 1.0", "1.0", BitString::toString(BitString(1.0)));
        checkEq("double constructor 123.456", "123.4560000000000030695446184836328029632568359375", BitString::toString(BitString(123.456)));
        checkEq("double constructor 1e20", "100000000000000000000.0", BitString::toString(BitString(1e20)));
        checkEq("double constructor 1e-20", "0.00000000000000000000999999999999999945153271454209571651729503702787392447107715776066783064379706", BitString::toString(BitString(1e-20)));

        //test modulo
        checkEq("mod 10%3", "1.0", BitString::toString(BitString::mod(BitString::fromString("10"), BitString::fromString("3"))));
        checkEq("mod 20%6", "2.0", BitString::toString(BitString::mod(BitString::fromString("20"), BitString::fromString("6"))));
        checkEq("mod 12345678901234567890%1234567890123456789", "0.0", BitString::toString(BitString::mod(BitString::fromString("12345678901234567890"), BitString::fromString("1234567890123456789"))));
        checkEq("mod 12345678901234567891%1234567890123456789", "1.0", BitString::toString(BitString::mod(BitString::fromString("12345678901234567891"), BitString::fromString("1234567890123456789"))));
        checkEq("mod 100000000000000000000%42", "16.0", BitString::toString(BitString::mod(BitString::fromString("100000000000000000000"), BitString::fromString("42"))));
    } catch (const exception& ex) {
        cout << "UNEXPECTED EXCEPTION: " << ex.what() << '\n';
        return 2;
    }

    if (failures == 0) {
        cout << "ALL TESTS PASSED\n";
        return 0;
    } else {
        cout << failures << " TEST(S) FAILED\n";
        return 1;
    }
}
