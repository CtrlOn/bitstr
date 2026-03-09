// Utilities
#include "bitstr.h"

using namespace std;

BitString BitString::avg(const vector<BitString>& v) {
    if (v.empty()) return BitString();

    BitString sum = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        sum = add(sum, v[i]);
    }
    return div(sum, BitString(static_cast<int>(v.size())));
}

int BitString::find(const vector<BitString>& v, const BitString& target) {
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == target) {
            return (int)i;
        }
    }
    return -1;
}

/// NOTE: yes this is bubble sort
void BitString::bubbleSort(vector<BitString>& v) {
    size_t n = v.size();
    if (n < 2) return;

    for (size_t i = 0; i < n - 1; ++i) {
        for (size_t j = 0; j < n - i - 1; ++j) {
            if (v[j] > v[j + 1]) {
                swap(v[j], v[j + 1]);
            }
        }
    }
}