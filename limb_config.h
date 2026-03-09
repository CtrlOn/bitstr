#ifndef LIMB_CONFIG_H
#define LIMB_CONFIG_H

#include <cstdint>

// Optional switch: set to 1 to experiment with native-limb storage.
// Keeping default 0 preserves existing behavior while exposing a single
// place to control limb width and related constants.
#ifndef BITSTR_USE_NATIVE_LIMB
#define BITSTR_USE_NATIVE_LIMB 1
#endif

namespace limb_config {

#if defined(__x86_64__) || defined(_M_X64)
using native_limb_t = std::uint64_t;
constexpr int native_limb_bits = 64;
#else
using native_limb_t = std::uint32_t;
constexpr int native_limb_bits = 32;
#endif

#if BITSTR_USE_NATIVE_LIMB
using limb_t = native_limb_t;
constexpr int limb_bits = native_limb_bits;
#else
using limb_t = std::uint32_t;
constexpr int limb_bits = 32;
#endif

using wide_limb_t =
#if BITSTR_USE_NATIVE_LIMB && (defined(__SIZEOF_INT128__) || defined(__GNUC__) || defined(__clang__))
    unsigned __int128;
#else
    std::uint64_t;
#endif

constexpr int word_bytes = limb_bits / 8;

inline int limb_clz(limb_t x) {
    if constexpr (sizeof(limb_t) == 8) {
        return __builtin_clzll(static_cast<unsigned long long>(x));
    }
    return __builtin_clz(static_cast<unsigned int>(x));
}

inline int limb_ctz(limb_t x) {
    if constexpr (sizeof(limb_t) == 8) {
        return __builtin_ctzll(static_cast<unsigned long long>(x));
    }
    return __builtin_ctz(static_cast<unsigned int>(x));
}

} // namespace limb_config

// Expose limb types and constants to the rest of the codebase
using namespace limb_config;

#endif // LIMB_CONFIG_H
