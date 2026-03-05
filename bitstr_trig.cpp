// Trigonometrics
#include "bitstr.h"

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

BitString BitString::sin(const BitString& n) {
    return BitString(); // TODO:
}