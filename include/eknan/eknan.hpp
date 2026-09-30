// Copyright 2026 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

#define EKNAN_VERSION_MAJOR 0
#define EKNAN_VERSION_MINOR 1
#define EKNAN_VERSION_PATCH 0

#define EKNAN_VERSION                                                \
    (EKNAN_VERSION_MAJOR * 1'000'000 + EKNAN_VERSION_MINOR * 1'000 + \
     EKNAN_VERSION_PATCH)

// Detect endianness.
// Both macros can be predefined, if we don't recognize the compiler,
// and `__(FLOAT)_BYTE_ORDER__` isn't (pre)defined.

#ifndef EKNAN_IS_BIG_ENDIAN

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define EKNAN_IS_BIG_ENDIAN 1
#else
#define EKNAN_IS_BIG_ENDIAN 0
#endif

#elif defined(_WIN32)
// Every Windows target is little-endian, and MSVC has no __BYTE_ORDER__
#define EKNAN_IS_BIG_ENDIAN 0
#elif defined(__BIG_ENDIAN__)
#define EKNAN_IS_BIG_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__)
#define EKNAN_IS_BIG_ENDIAN 0
#else
#error "eknan: unknown byte order, define EKNAN_IS_BIG_ENDIAN to 0 or 1"
#endif

#endif  // !defined(EKNAN_IS_BIG_ENDIAN)

#ifndef EKNAN_IS_FLOAT_BIG_ENDIAN

#if defined(__FLOAT_WORD_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#if __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
#define EKNAN_IS_FLOAT_BIG_ENDIAN 1
#else
#define EKNAN_IS_FLOAT_BIG_ENDIAN 0
#endif

#else
// Without __FLOAT_WORD_ORDER__, assume the same order as for integers
#define EKNAN_IS_FLOAT_BIG_ENDIAN EKNAN_IS_BIG_ENDIAN
#endif

#endif

// Detect compiler.
// Define to the major version, if we're on that compiler.

#ifdef __clang__
#define EKNAN_CLANG __clang_major__
#else
#define EKNAN_CLANG 0
#endif

// Catch compilers that pretend to be gcc, but aren't
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
    !defined(__NVCOMPILER) && !defined(__EDG__)
#define EKNAN_GCC __GNUC__
#else
#define EKNAN_GCC 0
#endif

// Detect architecture

#if defined(__x86_64__) || defined(__i386__)
#define EKNAN_X86 1
#else
#define EKNAN_X86 0
#endif

#ifdef __aarch64__
#define EKNAN_ARM64 1
#else
#define EKNAN_ARM64 0
#endif

#ifdef __arm__
#define EKNAN_ARM32 1
#else
#define EKNAN_ARM32 0
#endif

#if defined(__mips__) || defined(__mips)
#define EKNAN_MIPS 1
#else
#define EKNAN_MIPS 0
#endif

#if defined(__hppa__) || defined(__hppa)
#define EKNAN_HPPA 1
#else
#define EKNAN_HPPA 0
#endif

#if defined(__m68k__) || defined(__mc68000__)
#define EKNAN_M68K 1
#else
#define EKNAN_M68K 0
#endif

// Detect legacy (pre-IEEE 754-2008) NaN encoding,
// where the quiet bit is 0 in a quiet NaN.
// Used by MIPS without NaN2008, and PA-RISC.
#if (EKNAN_MIPS && !defined(__mips_nan2008)) || EKNAN_HPPA
#define EKNAN_HAS_LEGACY_NAN_ENCODING 1
#else
#define EKNAN_HAS_LEGACY_NAN_ENCODING 0
#endif

// Detect extended float types: _Float16, _Float32, _Float64, _Float128 and
// __bf16. In C++23, std::float16_t, std::float32_t, std::float64_t,
// std::float128_t and std::bfloat16_t are the same types, so they're covered
// too. Each EKNAN_HAS_* macro can be predefined to 0, if the detection is wrong
// for a compiler.
//
// A type is detected by its predefined __FLTN_*/__BFLT16_* macros, but some
// compilers define those without supporting the type in C++. The
// EKNAN_GCC_HAS_*/EKNAN_CLANG_HAS_* macros below exclude those compilers.

// Whether GCC supports _FloatN and __bf16 in C++. GCC 12 already defines the
// __FLTN_* macros, but supports _FloatN only in C.
#if EKNAN_GCC >= 13
#define EKNAN_GCC_HAS_FLOATN 1
#else
#define EKNAN_GCC_HAS_FLOATN 0
#endif

// clang-cl doesn't link compiler-rt, which _Float16 and __bf16 conversions
// need, so they're off there unless EKNAN_HAS_FLOAT16/EKNAN_HAS_BF16 are
// predefined
#if EKNAN_CLANG && defined(_MSC_VER)
#define EKNAN_CLANG_CL 1
#else
#define EKNAN_CLANG_CL 0
#endif

// Whether Clang's __FLT16_* macros mean _Float16 is supported
#if !EKNAN_CLANG || EKNAN_CLANG_CL
#define EKNAN_CLANG_HAS_FLOAT16 0
#elif EKNAN_ARM32
// The conversions need __aeabi_d2h, which libgcc doesn't have
#define EKNAN_CLANG_HAS_FLOAT16 0
#elif EKNAN_X86
// Supported since Clang 15, but older versions (like Clang 8) define the macros
#define EKNAN_CLANG_HAS_FLOAT16 (EKNAN_CLANG >= 15)
#else
#define EKNAN_CLANG_HAS_FLOAT16 1
#endif

// Whether Clang supports __bf16. Clang has no __BFLT16_* macros, so this goes
// by version and target. Clang 17 is the first version that can convert __bf16
// to and from other types. Clang also has __bf16 on 32-bit ARM (only some
// targets) and RISC-V (only newer versions), but those aren't detected.
#if !EKNAN_CLANG || EKNAN_CLANG_CL
#define EKNAN_CLANG_HAS_BF16 0
#elif EKNAN_X86 && defined(__SSE2__)
#define EKNAN_CLANG_HAS_BF16 (EKNAN_CLANG >= 17)
#elif EKNAN_ARM64 && defined(__apple_build_version__)
// AppleClang 17 is LLVM 19
#define EKNAN_CLANG_HAS_BF16 (EKNAN_CLANG >= 17)
#elif EKNAN_ARM64
// Clang 17 and 18 crash when converting to __bf16
#define EKNAN_CLANG_HAS_BF16 (EKNAN_CLANG >= 19)
#else
#define EKNAN_CLANG_HAS_BF16 0
#endif

#ifndef EKNAN_HAS_FLOAT16
#if defined(__FLT16_MANT_DIG__) && \
    (EKNAN_GCC_HAS_FLOATN || EKNAN_CLANG_HAS_FLOAT16)
#define EKNAN_HAS_FLOAT16 1
#else
#define EKNAN_HAS_FLOAT16 0
#endif
#endif

#ifndef EKNAN_HAS_FLOAT32
#if defined(__FLT32_MANT_DIG__) && EKNAN_GCC_HAS_FLOATN
#define EKNAN_HAS_FLOAT32 1
#else
#define EKNAN_HAS_FLOAT32 0
#endif
#endif

#ifndef EKNAN_HAS_FLOAT64
#if defined(__FLT64_MANT_DIG__) && EKNAN_GCC_HAS_FLOATN
#define EKNAN_HAS_FLOAT64 1
#else
#define EKNAN_HAS_FLOAT64 0
#endif
#endif

#ifndef EKNAN_HAS_FLOAT128
#if defined(__FLT128_MANT_DIG__) && EKNAN_GCC_HAS_FLOATN
#define EKNAN_HAS_FLOAT128 1
#else
#define EKNAN_HAS_FLOAT128 0
#endif
#endif

#ifndef EKNAN_HAS_BF16
#if (defined(__BFLT16_MANT_DIG__) && EKNAN_GCC_HAS_FLOATN) || \
    EKNAN_CLANG_HAS_BF16
#define EKNAN_HAS_BF16 1
#else
#define EKNAN_HAS_BF16 0
#endif
#endif

// __float128, distinct from _Float128 in GCC 13 and later
#ifndef EKNAN_HAS_GNU_FLOAT128
#if defined(__SIZEOF_FLOAT128__)
#define EKNAN_HAS_GNU_FLOAT128 1
#else
#define EKNAN_HAS_GNU_FLOAT128 0
#endif
#endif

// Detect uint128. Can be defined to 0 to use uint128_polyfill instead.

#ifndef EKNAN_HAS_INT128
#ifdef __SIZEOF_INT128__
#define EKNAN_HAS_INT128 1
#else
#define EKNAN_HAS_INT128 0
#endif
#endif

namespace eknan {

/**
 * An unsigned 128-bit integer, used as the payload type of 128-bit float
 * formats on platforms without `unsigned __int128`.
 *
 * Behaves like `unsigned __int128`, with comparison, addition, subtraction,
 * bitwise, and shift operators, but no multiplication or division.
 * Unlike `unsigned __int128`, shifting by 128 or more gives 0.
 */
struct uint128_polyfill {
    std::uint64_t low{};
    std::uint64_t high{};

    constexpr uint128_polyfill() noexcept = default;

    // Sign-extends like a conversion to unsigned __int128
    template <
        typename T,
        std::enable_if_t<std::is_integral_v<T> && sizeof(T) <= 8>* = nullptr>
    constexpr uint128_polyfill(T x) noexcept
        : low(static_cast<std::uint64_t>(x)),
          high(x < T{} ? ~std::uint64_t{} : std::uint64_t{})
    {
    }

    constexpr uint128_polyfill(std::uint64_t h, std::uint64_t l) noexcept
        : low(l), high(h)
    {
    }

    constexpr friend bool operator==(const uint128_polyfill& a,
                                     const uint128_polyfill& b) noexcept
    {
        return a.high == b.high && a.low == b.low;
    }

    constexpr friend bool operator!=(const uint128_polyfill& a,
                                     const uint128_polyfill& b) noexcept
    {
        return !(a == b);
    }

    constexpr friend bool operator<(const uint128_polyfill& a,
                                    const uint128_polyfill& b) noexcept
    {
        if (a.high == b.high) {
            return a.low < b.low;
        }
        return a.high < b.high;
    }

    constexpr friend bool operator>(const uint128_polyfill& a,
                                    const uint128_polyfill& b) noexcept
    {
        return b < a;
    }

    constexpr friend bool operator<=(const uint128_polyfill& a,
                                     const uint128_polyfill& b) noexcept
    {
        return !(a > b);
    }

    constexpr friend bool operator>=(const uint128_polyfill& a,
                                     const uint128_polyfill& b) noexcept
    {
        return !(a < b);
    }

    constexpr explicit operator bool() const noexcept
    {
        return low != 0 || high != 0;
    }

    // Truncates like a conversion from unsigned __int128
    template <typename T,
              std::enable_if_t<std::is_integral_v<T> &&
                               !std::is_same_v<T, bool>>* = nullptr>
    constexpr explicit operator T() const noexcept
    {
        return static_cast<T>(low);
    }

    constexpr friend uint128_polyfill operator+(
        const uint128_polyfill& a,
        const uint128_polyfill& b) noexcept
    {
        uint128_polyfill result;
        result.high = a.high + b.high + (a.low + b.low < a.low);
        result.low = a.low + b.low;
        return result;
    }

    constexpr friend uint128_polyfill& operator+=(
        uint128_polyfill& self,
        const uint128_polyfill& x) noexcept
    {
        self = self + x;
        return self;
    }

    constexpr friend uint128_polyfill operator-(
        const uint128_polyfill& a,
        const uint128_polyfill& b) noexcept
    {
        uint128_polyfill result;
        result.high = a.high - b.high - (a.low < b.low);
        result.low = a.low - b.low;
        return result;
    }

    constexpr friend uint128_polyfill& operator-=(
        uint128_polyfill& self,
        const uint128_polyfill& x) noexcept
    {
        self = self - x;
        return self;
    }

    constexpr friend uint128_polyfill operator&(
        const uint128_polyfill& a,
        const uint128_polyfill& b) noexcept
    {
        return {a.high & b.high, a.low & b.low};
    }

    constexpr friend uint128_polyfill& operator&=(
        uint128_polyfill& self,
        const uint128_polyfill& x) noexcept
    {
        self = self & x;
        return self;
    }

    constexpr friend uint128_polyfill operator|(
        const uint128_polyfill& a,
        const uint128_polyfill& b) noexcept
    {
        return {a.high | b.high, a.low | b.low};
    }

    constexpr friend uint128_polyfill& operator|=(
        uint128_polyfill& self,
        const uint128_polyfill& x) noexcept
    {
        self = self | x;
        return self;
    }

    constexpr friend uint128_polyfill operator^(
        const uint128_polyfill& a,
        const uint128_polyfill& b) noexcept
    {
        return {a.high ^ b.high, a.low ^ b.low};
    }

    constexpr friend uint128_polyfill& operator^=(
        uint128_polyfill& self,
        const uint128_polyfill& x) noexcept
    {
        self = self ^ x;
        return self;
    }

    constexpr friend uint128_polyfill operator~(
        const uint128_polyfill& x) noexcept
    {
        return {~x.high, ~x.low};
    }

    // Shifting by 128 or more gives 0, where unsigned __int128 would be
    // undefined. A shift of 0 is handled separately because the opposite
    // half would be shifted by 64, which is undefined for a uint64_t.
    constexpr friend uint128_polyfill operator<<(
        const uint128_polyfill& a,
        const uint128_polyfill& b) noexcept
    {
        uint128_polyfill result{};
        if (b.high || b.low >= 128) {
            return result;
        }

        const auto shift = b.low;
        if (shift == 0) {
            result = a;
        }
        else if (shift < 64) {
            result.high = (a.high << shift) + (a.low >> (64 - shift));
            result.low = a.low << shift;
        }
        else {
            result.high = a.low << (shift - 64);
        }
        return result;
    }

    constexpr friend uint128_polyfill& operator<<=(
        uint128_polyfill& self,
        const uint128_polyfill& x) noexcept
    {
        self = self << x;
        return self;
    }

    constexpr friend uint128_polyfill operator>>(
        const uint128_polyfill& a,
        const uint128_polyfill& b) noexcept
    {
        uint128_polyfill result{};
        if (b.high || b.low >= 128) {
            return result;
        }

        const auto shift = b.low;
        if (shift == 0) {
            result = a;
        }
        else if (shift < 64) {
            result.high = a.high >> shift;
            result.low = (a.high << (64 - shift)) + (a.low >> shift);
        }
        else {
            result.low = a.high >> (shift - 64);
        }
        return result;
    }

    constexpr friend uint128_polyfill& operator>>=(
        uint128_polyfill& self,
        const uint128_polyfill& x) noexcept
    {
        self = self >> x;
        return self;
    }
};

namespace detail {

#if EKNAN_HAS_INT128
// __extension__ silences -Wpedantic
__extension__ typedef unsigned __int128 uint128_builtin;
#endif

#if EKNAN_HAS_INT128
using uint128_t = uint128_builtin;
#else
using uint128_t = uint128_polyfill;
#endif

// Value of the quiet bit (the msb of the fraction) in a quiet NaN
inline constexpr unsigned nan_quiet_bit_value =
    EKNAN_HAS_LEGACY_NAN_ENCODING ? 0u : 1u;

constexpr unsigned quiet_bit_for(bool quiet)
{
    return quiet ? nan_quiet_bit_value : nan_quiet_bit_value ^ 1u;
}

// Whether a quiet (or signaling) NaN with an all-zero payload would have an
// all-zero fraction, making it an infinity instead: a signaling NaN with the
// IEEE 754-2008 encoding, and a quiet NaN with the legacy encoding
constexpr bool zero_payload_is_infinity(bool quiet)
{
    return quiet_bit_for(quiet) == 0u;
}

constexpr unsigned low_bits_mask(int count)
{
    return (1u << count) - 1u;
}

// `digits` matches std::numeric_limits<F>::digits: the payload, the quiet bit,
// and the leading significand bit (implicit, except in x87 80-bit).
//
// set_payload masks the payload to each field's width. The payload has
// already been range-checked, so the masks only show -Wconversion that the
// values fit.

struct nan_repr_f64 {
    using payload_int_type = std::uint64_t;
    static constexpr int payload_bits = 32 + 19;
    static constexpr int exponent_bits = 11;
    static constexpr int digits = payload_bits + 2;

#if !EKNAN_IS_BIG_ENDIAN && !EKNAN_IS_FLOAT_BIG_ENDIAN
    unsigned payload1 : 32;
    unsigned payload0 : 19;
    unsigned quiet_nan : 1;
    unsigned exponent : 11;
    unsigned sign : 1;
#elif !EKNAN_IS_BIG_ENDIAN && EKNAN_IS_FLOAT_BIG_ENDIAN
    // Little-endian words, but the high word of the double comes first
    unsigned payload0 : 19;
    unsigned quiet_nan : 1;
    unsigned exponent : 11;
    unsigned sign : 1;
    unsigned payload1 : 32;
#else
    unsigned sign : 1;
    unsigned exponent : 11;
    unsigned quiet_nan : 1;
    unsigned payload0 : 19;
    unsigned payload1 : 32;
#endif

    void set_payload(payload_int_type p)
    {
        constexpr unsigned payload0_mask = low_bits_mask(payload_bits - 32);
        payload0 = static_cast<unsigned>(p >> 32) & payload0_mask;
        payload1 = static_cast<unsigned>(p);
    }

    [[nodiscard]] payload_int_type get_payload() const
    {
        return (payload_int_type{payload0} << 32) | payload_int_type{payload1};
    }
};

struct nan_repr_f80 {
    using payload_int_type = uint128_t;
    static constexpr int payload_bits = 32 + 30;
    static constexpr int exponent_bits = 15;
    static constexpr int digits = payload_bits + 2;

#if !EKNAN_IS_BIG_ENDIAN && !EKNAN_IS_FLOAT_BIG_ENDIAN
    unsigned payload1 : 32;
    unsigned payload0 : 30;
    unsigned quiet_nan : 1;
    unsigned one : 1;
    unsigned exponent : 15;
    unsigned sign : 1;
    unsigned padding : 16;
#elif !EKNAN_IS_BIG_ENDIAN && EKNAN_IS_FLOAT_BIG_ENDIAN
    unsigned exponent : 15;
    unsigned sign : 1;
    unsigned padding : 16;
    unsigned payload0 : 30;
    unsigned quiet_nan : 1;
    unsigned one : 1;
    unsigned payload1 : 32;
#else
    // m68k: the 16 bits of padding are between the exponent and the
    // significand
    unsigned sign : 1;
    unsigned exponent : 15;
    unsigned padding : 16;
    unsigned one : 1;
    unsigned quiet_nan : 1;
    unsigned payload0 : 30;
    unsigned payload1 : 32;
#endif

    void set_payload(payload_int_type p)
    {
        constexpr unsigned payload0_mask = low_bits_mask(payload_bits - 32);
        // The explicit integer bit is 1 in a normal NaN
        one = 1;
        payload0 = static_cast<unsigned>(p >> 32u) & payload0_mask;
        payload1 = static_cast<unsigned>(p);
    }

    [[nodiscard]] payload_int_type get_payload() const
    {
        return (payload_int_type{payload0} << 32u) | payload_int_type{payload1};
    }
};

struct nan_repr_f128 {
    using payload_int_type = uint128_t;
    static constexpr int payload_bits = 32 * 3 + 15;
    static constexpr int exponent_bits = 15;
    static constexpr int digits = payload_bits + 2;

#if !EKNAN_IS_BIG_ENDIAN && !EKNAN_IS_FLOAT_BIG_ENDIAN
    unsigned payload3 : 32;
    unsigned payload2 : 32;
    unsigned payload1 : 32;
    unsigned payload0 : 15;
    unsigned quiet_nan : 1;
    unsigned exponent : 15;
    unsigned sign : 1;
#elif !EKNAN_IS_BIG_ENDIAN && EKNAN_IS_FLOAT_BIG_ENDIAN
    unsigned payload0 : 15;
    unsigned quiet_nan : 1;
    unsigned exponent : 15;
    unsigned sign : 1;
    unsigned payload1 : 32;
    unsigned payload2 : 32;
    unsigned payload3 : 32;
#else
    unsigned sign : 1;
    unsigned exponent : 15;
    unsigned quiet_nan : 1;
    unsigned payload0 : 15;
    unsigned payload1 : 32;
    unsigned payload2 : 32;
    unsigned payload3 : 32;
#endif

    void set_payload(payload_int_type p)
    {
        constexpr unsigned payload0_mask = low_bits_mask(payload_bits - 96);
        payload0 = static_cast<unsigned>(p >> 96u) & payload0_mask;
        payload1 = static_cast<unsigned>(p >> 64u);
        payload2 = static_cast<unsigned>(p >> 32u);
        payload3 = static_cast<unsigned>(p);
    }

    [[nodiscard]] payload_int_type get_payload() const
    {
        return (payload_int_type{payload0} << 96u) |
               (payload_int_type{payload1} << 64u) |
               (payload_int_type{payload2} << 32u) | payload_int_type{payload3};
    }
};

struct nan_repr_doubledouble {
    using base = nan_repr_f64;
    using payload_int_type = base::payload_int_type;
    static constexpr int payload_bits = base::payload_bits;
    static constexpr int exponent_bits = base::exponent_bits;
    static constexpr int digits = 2 * base::digits;

    base high;
    base low;

    void set_payload(payload_int_type p)
    {
        // Only the high double is inspected; zeroing the low one makes the
        // result independent of its previous value
        high.set_payload(p);
        low = {};
    }

    [[nodiscard]] payload_int_type get_payload() const
    {
        return high.get_payload();
    }
};

// A format that fits in one integer, Int: binary32, binary16 and bfloat16
template <typename Int, int PayloadBits, int ExponentBits>
struct nan_repr_single_word {
    using payload_int_type = Int;
    static constexpr int payload_bits = PayloadBits;
    static constexpr int exponent_bits = ExponentBits;
    static constexpr int digits = payload_bits + 2;

#if !EKNAN_IS_BIG_ENDIAN
    Int payload : PayloadBits;
    Int quiet_nan : 1;
    Int exponent : ExponentBits;
    Int sign : 1;
#else
    Int sign : 1;
    Int exponent : ExponentBits;
    Int quiet_nan : 1;
    Int payload : PayloadBits;
#endif

    void set_payload(payload_int_type p)
    {
        constexpr unsigned payload_mask = low_bits_mask(payload_bits);
        payload = p & payload_mask;
    }

    [[nodiscard]] payload_int_type get_payload() const
    {
        return static_cast<payload_int_type>(payload);
    }
};

using nan_repr_f32 = nan_repr_single_word<std::uint32_t, 22, 8>;
using nan_repr_f16 = nan_repr_single_word<std::uint16_t, 9, 5>;
using nan_repr_bf16 = nan_repr_single_word<std::uint16_t, 6, 8>;

// The parameters of a float type that its repr is checked against, with the
// same meaning as in std::numeric_limits
struct float_format {
    int radix{};
    int digits{};
    int max_exponent{};
    bool has_quiet_nan{};
};

template <typename F>
constexpr float_format float_format_from_limits()
{
    using limits = std::numeric_limits<F>;
    return {limits::radix, limits::digits, limits::max_exponent,
            limits::has_quiet_NaN};
}

// The format of compiler predefined macros, like __FLT16_MANT_DIG__
// for prefix __FLT16
#define EKNAN_DETAIL_FORMAT_FROM_MACROS(radix, prefix)  \
    float_format                                        \
    {                                                   \
        radix, prefix##_MANT_DIG__, prefix##_MAX_EXP__, \
            prefix##_HAS_QUIET_NAN__                    \
    }

// Classify a long double by its significand digits:
//  - 53 is a plain double (e.g. MSVC),
//  - 64 is x87 extended,
//  - 113 is IEEE binary128, and
//  - 106 is double-double
template <int Digits>
using nan_repr_for_digits = std::conditional_t<
    Digits == 53,
    nan_repr_f64,
    std::conditional_t<
        Digits == 64,
        nan_repr_f80,
        std::conditional_t<
            Digits == 113,
            nan_repr_f128,
            std::conditional_t<Digits == 106, nan_repr_doubledouble, void>>>>;

template <typename F, typename = void>
struct float_type_info {
    using repr = void;
    static constexpr auto format = float_format{};
};

template <>
struct float_type_info<float> {
    using repr = nan_repr_f32;

#if defined(__FLT_MANT_DIG__)
    static constexpr auto format =
        EKNAN_DETAIL_FORMAT_FROM_MACROS(__FLT_RADIX__, __FLT);
#else
    static constexpr auto format = float_format_from_limits<float>();
#endif
};

template <>
struct float_type_info<double> {
    using repr = nan_repr_f64;

#if defined(__DBL_MANT_DIG__)
    static constexpr auto format =
        EKNAN_DETAIL_FORMAT_FROM_MACROS(__FLT_RADIX__, __DBL);
#else
    static constexpr auto format = float_format_from_limits<double>();
#endif
};

template <>
struct float_type_info<long double> {
#if defined(__LDBL_MANT_DIG__)
    static constexpr auto format =
        EKNAN_DETAIL_FORMAT_FROM_MACROS(__FLT_RADIX__, __LDBL);
#else
    static constexpr auto format = float_format_from_limits<long double>();
#endif

    // long double is more complicated
    using repr = nan_repr_for_digits<format.digits>;
};

#if EKNAN_HAS_FLOAT16
template <>
struct float_type_info<_Float16> {
    using repr = nan_repr_f16;
    static constexpr auto format = EKNAN_DETAIL_FORMAT_FROM_MACROS(2, __FLT16);
};
#endif

#if EKNAN_HAS_FLOAT32
template <>
struct float_type_info<_Float32> {
    using repr = nan_repr_f32;
    static constexpr auto format = EKNAN_DETAIL_FORMAT_FROM_MACROS(2, __FLT32);
};
#endif

#if EKNAN_HAS_FLOAT64
template <>
struct float_type_info<_Float64> {
    using repr = nan_repr_f64;
    static constexpr auto format = EKNAN_DETAIL_FORMAT_FROM_MACROS(2, __FLT64);
};
#endif

#if EKNAN_HAS_FLOAT128
template <>
struct float_type_info<_Float128> {
    using repr = nan_repr_f128;
    static constexpr auto format = EKNAN_DETAIL_FORMAT_FROM_MACROS(2, __FLT128);
};
#endif

// Clang has no __BFLT16_* macros, and no numeric_limits specialization,
// so we're hardcoding the `format` values.
#if EKNAN_HAS_BF16
template <>
struct float_type_info<__bf16> {
    using repr = nan_repr_bf16;

#if defined(__BFLT16_MANT_DIG__)
    static constexpr auto format = EKNAN_DETAIL_FORMAT_FROM_MACROS(2, __BFLT16);
#else
    static constexpr auto format = std::numeric_limits<__bf16>::is_specialized
                                       ? float_format_from_limits<__bf16>()
                                       : float_format{2, 8, 128, true};
#endif
};
#endif

// __float128 is IEEE binary128, like _Float128. It isn't specialized
// separately if it's the same type as long double (PowerPC with
// -mabi=ieeelongdouble).
// GCC's __FLT128_* macros describe _Float128, which has the same format.
// Clang has neither the macros nor _Float128, so its format comes from
// numeric_limits (libstdc++ has it), or else from the definition of
// binary128 (libc++).
#if EKNAN_HAS_GNU_FLOAT128
template <typename F>
struct float_type_info<F,
                       std::enable_if_t<std::is_same_v<F, __float128> &&
                                        !std::is_same_v<F, long double>>> {
    using repr = nan_repr_f128;

#if defined(__FLT128_MANT_DIG__)
    static constexpr auto format = EKNAN_DETAIL_FORMAT_FROM_MACROS(2, __FLT128);
#else
    static constexpr auto format = std::numeric_limits<F>::is_specialized
                                       ? float_format_from_limits<F>()
                                       : float_format{2, 113, 16384, true};
#endif
};
#endif

#undef EKNAN_DETAIL_FORMAT_FROM_MACROS

// Checks that F is laid out like Repr.
template <typename F, typename Repr>
constexpr bool matches_repr()
{
    if constexpr (std::is_void_v<Repr>) {
        return false;
    }
    else {
        constexpr auto format = float_type_info<F>::format;
        return format.radix == 2 && format.digits == Repr::digits &&
               format.max_exponent == 1 << (Repr::exponent_bits - 1) &&
               sizeof(Repr) <= sizeof(F) && format.has_quiet_nan;
    }
}

template <typename F>
using nan_repr_candidate = typename float_type_info<F>::repr;

template <typename F>
using nan_repr_for =
    std::conditional_t<matches_repr<F, nan_repr_candidate<F>>(),
                       nan_repr_candidate<F>,
                       void>;

template <typename T>
inline constexpr bool is_supported_float_type =
    !std::is_void_v<nan_repr_for<T>>;

// Width of the repr's `padding` field, if it has one
template <typename Repr>
inline constexpr int padding_field_bits = 0;
template <>
inline constexpr int padding_field_bits<nan_repr_f80> = 16;

// Bytes of F after its repr, like the last 6 bytes of an 80-bit long double on
// x86-64
template <typename F>
inline constexpr std::size_t trailing_padding_bytes =
    sizeof(F) - sizeof(nan_repr_for<F>);

template <typename F>
using payload_type = typename nan_repr_for<F>::payload_int_type;

template <typename F>
nan_repr_for<F> load_repr(const F& value)
{
    // F can be larger than its repr:
    // the trailing bytes of long double are ignored
    static_assert(sizeof(nan_repr_for<F>) <= sizeof(F));
    nan_repr_for<F> r{};
    std::memcpy(&r, &value, sizeof(r));
    return r;
}

template <typename F>
void store_repr(F& value, const nan_repr_for<F>& r)
{
    // Leaves the bytes of `value` after the repr untouched
    std::memcpy(&value, &r, sizeof(r));
}

// Returns the part of the repr with the sign, exponent and quiet bit.
// For double-double, uses the high double:
// the value is a NaN if the high double is one.
template <typename Repr>
auto& nan_fields(Repr& r)
{
    if constexpr (std::is_same_v<std::remove_const_t<Repr>,
                                 nan_repr_doubledouble>) {
        return r.high;
    }
    else {
        return r;
    }
}

// Flips the sign. For double-double, flips the sign of both doubles, which
// negates its value, like copysignl in glibc.
template <typename Repr>
void negate_repr(Repr& r)
{
    if constexpr (std::is_same_v<Repr, nan_repr_doubledouble>) {
        negate_repr(r.high);
        negate_repr(r.low);
    }
    else {
        r.sign = !r.sign;
    }
}

template <typename F>
constexpr bool fits_in_payload(payload_type<F> payload)
{
    using repr = nan_repr_for<F>;
    using T = payload_type<F>;
    static_assert(repr::payload_bits < sizeof(T) * 8);
    return !(payload >>
             static_cast<T>(static_cast<unsigned>(repr::payload_bits)));
}

template <typename F>
constexpr bool is_valid_payload(payload_type<F> payload, bool quiet)
{
    return fits_in_payload<F>(payload) &&
           !(!payload && zero_payload_is_infinity(quiet));
}

// NaN: all-ones exponent, and a nonzero fraction.
// For x87 80-bit, the explicit integer bit is ignored, so pseudo-NaNs
// (integer bit 0) are also considered NaNs.
template <typename Repr>
bool is_nan_repr(const Repr& r)
{
    constexpr unsigned max_exponent = (1u << Repr::exponent_bits) - 1u;
    return r.exponent == max_exponent &&
           (r.quiet_nan || static_cast<bool>(r.get_payload()));
}

// Whether r is a NaN with the given quietness
template <typename Repr>
bool is_nan_with_quietness(const Repr& r, bool quiet)
{
    const auto& fields = nan_fields(r);
    return is_nan_repr(fields) && fields.quiet_nan == quiet_bit_for(quiet);
}

template <typename F>
void write_nan(F& out, bool quiet, payload_type<F> payload)
{
    // Doesn't check that the payload is valid
    nan_repr_for<F> r{};
    auto& fields = nan_fields(r);
    constexpr unsigned max_exponent =
        low_bits_mask(std::remove_reference_t<decltype(fields)>::exponent_bits);
    fields.exponent = max_exponent;
    fields.quiet_nan = quiet_bit_for(quiet) & 1u;
    r.set_payload(payload);
    // Zeroes the padding
    std::memset(&out, 0, sizeof(F));
    store_repr(out, r);
}

template <typename F>
bool make_nan(F& out, bool quiet, payload_type<F> payload)
{
    if (!is_valid_payload<F>(payload, quiet)) {
        return false;
    }
    write_nan(out, quiet, payload);
    return true;
}

// Whether the default NaNs have every payload bit set: with GCC on MIPS and
// PA-RISC (legacy encoding), and on m68k. Clang uses the same bits as on other
// platforms.
#if !EKNAN_CLANG && (EKNAN_HAS_LEGACY_NAN_ENCODING || EKNAN_M68K)
inline constexpr bool default_nan_payload_is_all_ones = true;
#else
inline constexpr bool default_nan_payload_is_all_ones = false;
#endif

// The payload of the default NaN, for types without both NaNs in
// numeric_limits.
// Matches the NaNs that GCC and Clang produce with __builtin_nan and
// __builtin_nans. If default_nan_payload_is_all_ones is true,
// the payload will (shockingly) have all its bits set.
// Otherwise, the NaN with a quiet-bit field of 0
// (signaling, except on legacy encodings)
// has only the top payload bit set, and the other has payload 0.
template <typename F>
constexpr payload_type<F> default_payload(bool quiet)
{
    using T = payload_type<F>;
    constexpr auto top_bit_index =
        static_cast<unsigned>(nan_repr_for<F>::payload_bits - 1);
    const auto top_bit = static_cast<T>(T{1} << static_cast<T>(top_bit_index));
    if constexpr (default_nan_payload_is_all_ones) {
        return static_cast<T>(top_bit | static_cast<T>(top_bit - T{1}));
    }
    else {
        return zero_payload_is_infinity(quiet) ? top_bit : T{};
    }
}

template <typename F>
inline constexpr bool has_limits_nans =
    std::numeric_limits<F>::is_specialized &&
    std::numeric_limits<F>::has_quiet_NaN &&
    std::numeric_limits<F>::has_signaling_NaN;

template <typename F>
inline constexpr F limits_quiet_nan = std::numeric_limits<F>::quiet_NaN();
template <typename F>
inline constexpr F limits_signaling_nan =
    std::numeric_limits<F>::signaling_NaN();

template <typename F>
void make_default_nan(F& out, bool quiet)
{
    if constexpr (has_limits_nans<F>) {
        std::memcpy(&out,
                    quiet ? &limits_quiet_nan<F> : &limits_signaling_nan<F>,
                    sizeof(F));
        if (!quiet) {
            // MSVC's numeric_limits<float>::signaling_NaN() is quiet
            auto r = load_repr(out);
            nan_fields(r).quiet_nan = quiet_bit_for(false) & 1u;
            store_repr(out, r);
        }
    }
    else {
        static_assert(is_valid_payload<F>(default_payload<F>(true), true) &&
                      is_valid_payload<F>(default_payload<F>(false), false));
        write_nan(out, quiet, default_payload<F>(quiet));
    }
}

template <typename F>
bool set_quiet_bit(F& value, bool quiet)
{
    auto r = load_repr(value);
    if (!is_nan_repr(nan_fields(r)) ||
        (!r.get_payload() && zero_payload_is_infinity(quiet))) {
        return false;
    }
    nan_fields(r).quiet_nan = quiet_bit_for(quiet) & 1u;
    store_repr(value, r);
    return true;
}

inline std::optional<unsigned> parse_digit(char ch, unsigned base)
{
    unsigned digit{};
    if (ch >= '0' && ch <= '9') {
        digit = static_cast<unsigned>(ch - '0');
    }
    else if (ch >= 'a' && ch <= 'f') {
        digit = static_cast<unsigned>(ch - 'a' + 10);
    }
    else if (ch >= 'A' && ch <= 'F') {
        digit = static_cast<unsigned>(ch - 'A' + 10);
    }
    else {
        return std::nullopt;
    }

    if (digit >= base) {
        return std::nullopt;
    }
    return digit;
}

// Shifts and adds instead of operator*, which uint128_polyfill doesn't have
template <typename T>
T multiply_by_base(T x, unsigned base)
{
    if (base == 8) {
        return static_cast<T>(x << 3u);
    }
    if (base == 16) {
        return static_cast<T>(x << 4u);
    }
    return static_cast<T>((x << 3u) + (x << 1u));
}

// Decimal, hexadecimal (0x or 0X prefix) or octal (0 prefix).
// Underscores are allowed as digit separators after the first digit, or
// after the 0x prefix, but not before or within the prefix.
template <typename F>
std::optional<payload_type<F>> parse_payload_str(std::string_view str)
{
    using T = payload_type<F>;
    // Checking the range after every digit keeps the accumulator from
    // overflowing, as long as there's always room for one more hex digit
    static_assert(nan_repr_for<F>::payload_bits + 4 <= sizeof(T) * 8);

    if (str.empty() || str[0] == '_') {
        return std::nullopt;
    }

    // The 0 of an octal prefix stays in `str` and is parsed as a digit, so
    // "0" alone is a valid payload
    unsigned base = 10;
    if (str[0] == '0') {
        if (str.size() >= 2 && (str[1] == 'x' || str[1] == 'X')) {
            base = 16;
            str.remove_prefix(2);
        }
        else {
            base = 8;
        }
    }

    T accumulator{};
    bool has_digits = false;
    for (char ch : str) {
        if (ch == '_') {
            continue;
        }

        const auto digit = parse_digit(ch, base);
        if (!digit) {
            return std::nullopt;
        }
        accumulator = static_cast<T>(multiply_by_base(accumulator, base) +
                                     static_cast<T>(*digit));
        if (!fits_in_payload<F>(accumulator)) {
            return std::nullopt;
        }
        has_digits = true;
    }

    if (!has_digits) {
        return std::nullopt;
    }
    return accumulator;
}

template <typename T>
inline constexpr bool is_string_like =
    std::is_convertible_v<const T&, std::string_view>;

}  // namespace detail

/**
 * The payload type of `F`: an unsigned integer type wide enough for
 * `get_payload_bit_count<F>()` bits. For 128-bit payloads, it's
 * `unsigned __int128`, or `uint128_polyfill` on platforms without it.
 */
template <typename F>
using payload_type = detail::payload_type<F>;

/**
 * Checks whether `value` is a NaN, quiet or signaling.
 *
 * Inspects the bits directly, so it is not affected by `-ffast-math`.
 * For the x87 80-bit `long double`, pseudo-NaNs (integer bit 0) count as NaNs.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool is_nan(const F& value) noexcept
{
    const auto r = detail::load_repr(value);
    return detail::is_nan_repr(detail::nan_fields(r));
}

/**
 * Sets `out` to the default quiet NaN, `std::numeric_limits<F>::quiet_NaN()`.
 *
 * If `std::numeric_limits<F>` doesn't provide both NaNs, sets it to a positive
 * quiet NaN with the payload that GCC's and Clang's `__builtin_nan` give: 0 on
 * most platforms, and all ones on MIPS and PA-RISC (the legacy NaN encoding)
 * and m68k.
 *
 * The `make_*` functions write into `out` instead of returning a value,
 * because returning a float or double on i386 quiets a signaling NaN.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
void make_qnan(F& out) noexcept
{
    detail::make_default_nan(out, true);
}

/**
 * Sets `out` to the default signaling NaN,
 * `std::numeric_limits<F>::signaling_NaN()`. If that is actually quiet
 * (MSVC's `float`), its made to be a signaling NaN.
 *
 * If `std::numeric_limits<F>` doesn't provide both NaNs, sets it to a positive
 * signaling NaN with the payload that GCC's and Clang's `__builtin_nans` give:
 * only the top payload bit set on most platforms, and all ones on MIPS and
 * PA-RISC (the legacy NaN encoding) and m68k.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
void make_snan(F& out) noexcept
{
    detail::make_default_nan(out, false);
}

/**
 * Sets `out` to a positive quiet NaN with the given payload and zero padding.
 *
 * @return `false`, leaving `out` unchanged, if `payload` doesn't fit in
 * `get_payload_bit_count<F>()` bits, or if the result would be an infinity
 * (payload 0 with the legacy NaN encoding).
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool make_qnan(F& out, payload_type<F> payload) noexcept
{
    return detail::make_nan(out, true, payload);
}

/**
 * Sets `out` to a positive signaling NaN with the given payload and zero
 * padding.
 *
 * @return `false`, leaving `out` unchanged, if `payload` doesn't fit in
 * `get_payload_bit_count<F>()` bits, or if the result would be an infinity
 * (payload 0 with the IEEE 754-2008 NaN encoding).
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool make_snan(F& out, payload_type<F> payload) noexcept
{
    return detail::make_nan(out, false, payload);
}

/**
 * Like `make_qnan(out, payload)`, with the payload parsed from a string.
 *
 * The string is an integer in one of these formats:
 * - decimal
 * - hexadecimal, with a `0x` or `0X` prefix
 * - octal, with a `0` prefix
 *
 * Underscores are allowed as digit separators after the first digit, or after
 * the `0x` prefix.
 *
 * @return `false`, leaving `out` unchanged, if the string is malformed, or
 * for any reason `make_qnan(out, payload)` fails.
 *
 * @note `Str` is a template parameter, instead of `std::string_view`, so that
 * a literal 0 (a null pointer constant) isn't an ambiguous argument when the
 * payload type is `uint128_polyfill`.
 */
template <typename F,
          typename Str,
          std::enable_if_t<detail::is_supported_float_type<F> &&
                           detail::is_string_like<Str>>* = nullptr>
[[nodiscard]] bool make_qnan(F& out, const Str& payload) noexcept(
    std::is_nothrow_constructible_v<std::string_view, const Str&>)
{
    const auto p = detail::parse_payload_str<F>(payload);
    return p && make_qnan(out, *p);
}

/**
 * Like `make_snan(out, payload)`, with the payload parsed from a string.
 * The string format is the same as in the string overload of `make_qnan`.
 *
 * @return `false`, leaving `out` unchanged, if the string is malformed, or
 * for any reason `make_snan(out, payload)` fails.
 */
template <typename F,
          typename Str,
          std::enable_if_t<detail::is_supported_float_type<F> &&
                           detail::is_string_like<Str>>* = nullptr>
[[nodiscard]] bool make_snan(F& out, const Str& payload) noexcept(
    std::is_nothrow_constructible_v<std::string_view, const Str&>)
{
    const auto p = detail::parse_payload_str<F>(payload);
    return p && make_snan(out, *p);
}

/**
 * Sets the sign bit of `value`. Works on any value, not only NaNs.
 *
 * For double-double, flipping the sign flips it in both doubles, so that a
 * number is negated, like with `std::copysign`.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
void set_signbit(F& value, bool bit) noexcept
{
    auto r = detail::load_repr(value);
    if (static_cast<bool>(detail::nan_fields(r).sign) == bit) {
        return;
    }
    detail::negate_repr(r);
    detail::store_repr(value, r);
}

/**
 * Gets the sign bit of `value`. Works on any value, not only NaNs.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool get_signbit(const F& value) noexcept
{
    const auto r = detail::load_repr(value);
    return detail::nan_fields(r).sign;
}

/**
 * Checks whether `value` is a quiet NaN. `false` for non-NaNs.
 *
 * A quiet NaN has the quiet bit set with the IEEE 754-2008 NaN encoding, and
 * clear with the legacy encoding.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool is_quiet(const F& value) noexcept
{
    return detail::is_nan_with_quietness(detail::load_repr(value), true);
}

/**
 * Checks whether `value` is a signaling NaN. `false` for non-NaNs.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool is_signaling(const F& value) noexcept
{
    return detail::is_nan_with_quietness(detail::load_repr(value), false);
}

/**
 * Sets the payload of the NaN `value`, keeping its sign and quietness.
 *
 * @return `false`, leaving `value` unchanged, if:
 * - `value` isn't a NaN, or
 * - `payload` doesn't fit in `get_payload_bit_count<F>()` bits, or
 * - the result would be an infinity.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool set_payload(F& value, payload_type<F> payload) noexcept
{
    auto r = detail::load_repr(value);
    if (!detail::is_nan_repr(detail::nan_fields(r)) ||
        !detail::is_valid_payload<F>(payload, is_quiet(value))) {
        return false;
    }

    r.set_payload(payload);
    detail::store_repr(value, r);
    return true;
}

/**
 * Gets the payload of `value`: the fraction without the quiet bit.
 * Doesn't check that `value` is a NaN.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] payload_type<F> get_payload(const F& value) noexcept
{
    return detail::load_repr(value).get_payload();
}

/**
 * Number of bits in the payload of `F`. Valid payloads are below
 * 2<sup>`get_payload_bit_count<F>()`</sup>.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] constexpr std::size_t get_payload_bit_count() noexcept
{
    return static_cast<std::size_t>(detail::nan_repr_for<F>::payload_bits);
}

/**
 * Number of bits in `F` that aren't part of its value.
 *
 * Only the 80-bit extended `long double` has any: 16 between its fields
 * (m68k) or after them (x86), and on x86-64 another 32 after those.
 * 0 for every other type.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] constexpr std::size_t get_padding_bit_count() noexcept
{
    return static_cast<std::size_t>(
               detail::padding_field_bits<detail::nan_repr_for<F>>) +
           detail::trailing_padding_bytes<F> * 8;
}

/**
 * Gets the padding bits of `value`: those between its fields as the lowest
 * bits, then the bytes after its value, the first byte least significant.
 *
 * Copying a value may not preserve its padding, so it's passed by reference.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] std::uint64_t get_padding(const F& value) noexcept
{
    using repr = detail::nan_repr_for<F>;
    constexpr auto field_bits = detail::padding_field_bits<repr>;
    static_assert(get_padding_bit_count<F>() < 64);

    std::uint64_t padding = 0;
    if constexpr (field_bits != 0) {
        padding = detail::load_repr(value).padding;
    }
    if constexpr (detail::trailing_padding_bytes<F> != 0) {
        unsigned char bytes[sizeof(F)];
        std::memcpy(bytes, &value, sizeof(F));
        for (std::size_t i = 0; i < detail::trailing_padding_bytes<F>; ++i) {
            padding |= std::uint64_t{bytes[sizeof(repr) + i]}
                       << (static_cast<std::size_t>(field_bits) + i * 8);
        }
    }
    return padding;
}

/**
 * Sets the padding bits of `value`, in the same order as `get_padding`.
 *
 * @return `false`, leaving `value` unchanged, if `padding` doesn't fit in
 * `get_padding_bit_count<F>()` bits.
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool set_padding(F& value, std::uint64_t padding) noexcept
{
    using repr = detail::nan_repr_for<F>;
    constexpr auto field_bits = detail::padding_field_bits<repr>;
    // Shifting a uint64_t by 64 or more would be undefined
    static_assert(get_padding_bit_count<F>() < 64);

    if (padding >> get_padding_bit_count<F>()) {
        return false;
    }

    if constexpr (field_bits != 0) {
        auto r = detail::load_repr(value);
        r.padding = static_cast<unsigned>(padding) & ((1u << field_bits) - 1u);
        detail::store_repr(value, r);
    }
    if constexpr (detail::trailing_padding_bytes<F> != 0) {
        unsigned char bytes[sizeof(F)];
        std::memcpy(bytes, &value, sizeof(F));
        for (std::size_t i = 0; i < detail::trailing_padding_bytes<F>; ++i) {
            bytes[sizeof(repr) + i] = static_cast<unsigned char>(
                padding >> (static_cast<std::size_t>(field_bits) + i * 8));
        }
        std::memcpy(&value, bytes, sizeof(F));
    }
    return true;
}

/**
 * Turns the NaN `value` into a quiet NaN, keeping its sign and payload.
 *
 * @return `false`, leaving `value` unchanged, if it isn't a NaN, or if the
 * result would be an infinity (payload 0 with the legacy NaN encoding).
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool set_quiet(F& value) noexcept
{
    return detail::set_quiet_bit(value, true);
}

/**
 * Turns the NaN `value` into a signaling NaN, keeping its sign and payload.
 *
 * @return `false`, leaving `value` unchanged, if it isn't a NaN, or if the
 * result would be an infinity (payload 0 with the IEEE 754-2008 NaN
 * encoding).
 */
template <typename F,
          std::enable_if_t<detail::is_supported_float_type<F>>* = nullptr>
[[nodiscard]] bool set_signaling(F& value) noexcept
{
    return detail::set_quiet_bit(value, false);
}

}  // namespace eknan
