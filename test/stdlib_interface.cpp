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

#include "stdlib_interface.hpp"

#include <eknan/eknan.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#if defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__
#error "stdlib_interface.cpp must be compiled without -ffast-math"
#endif

namespace stdlib_interface {

#ifdef __has_builtin
#define EKNAN_TEST_HAS_BUILTIN(name) __has_builtin(name)
#else
// GCC before 10, which has no extended types other than __float128
#define EKNAN_TEST_HAS_BUILTIN(name) 1
#endif

// Fail loudly if a builtin is missing, instead of silently testing a type less
#if (EKNAN_HAS_FLOAT16 || EKNAN_HAS_FLOAT32 || EKNAN_HAS_FLOAT64 ||          \
     EKNAN_HAS_FLOAT128 || EKNAN_TEST_HAS_GNU_FLOAT128 || EKNAN_HAS_BF16) && \
    !(EKNAN_TEST_HAS_BUILTIN(__builtin_isnan) &&                             \
      EKNAN_TEST_HAS_BUILTIN(__builtin_signbit) &&                           \
      EKNAN_TEST_HAS_BUILTIN(__builtin_inff))
#error "missing __builtin_isnan, __builtin_signbit or __builtin_inff"
#endif

#define EKNAN_TEST_HAS_BUILTINS_FOR_SUFFIX(suffix)     \
    (EKNAN_TEST_HAS_BUILTIN(__builtin_nan##suffix) &&  \
     EKNAN_TEST_HAS_BUILTIN(__builtin_nans##suffix) && \
     EKNAN_TEST_HAS_BUILTIN(__builtin_copysign##suffix))

#if EKNAN_HAS_FLOAT16 && !EKNAN_TEST_HAS_BUILTINS_FOR_SUFFIX(f16)
#error "missing _Float16 builtins; define EKNAN_HAS_FLOAT16=0 to skip it"
#endif

#if EKNAN_HAS_FLOAT32 && !EKNAN_TEST_HAS_BUILTINS_FOR_SUFFIX(f32)
#error "missing _Float32 builtins; define EKNAN_HAS_FLOAT32=0 to skip it"
#endif

#if EKNAN_HAS_FLOAT64 && !EKNAN_TEST_HAS_BUILTINS_FOR_SUFFIX(f64)
#error "missing _Float64 builtins; define EKNAN_HAS_FLOAT64=0 to skip it"
#endif

#if EKNAN_HAS_FLOAT128 && !EKNAN_TEST_HAS_BUILTINS_FOR_SUFFIX(f128)
#error "missing _Float128 builtins; define EKNAN_HAS_FLOAT128=0 to skip it"
#endif

#if EKNAN_TEST_HAS_GNU_FLOAT128 &&                                  \
    !(defined(__clang__) ? EKNAN_TEST_HAS_BUILTINS_FOR_SUFFIX(f128) \
                         : EKNAN_TEST_HAS_BUILTINS_FOR_SUFFIX(q))
#error "missing __float128 builtins; define EKNAN_HAS_GNU_FLOAT128=0 to skip it"
#endif

#if EKNAN_HAS_BF16 &&                                                 \
    !(defined(__clang__) ? EKNAN_TEST_HAS_BUILTIN(__builtin_bit_cast) \
                         : (EKNAN_TEST_HAS_BUILTIN(__builtin_nanf) && \
                            EKNAN_TEST_HAS_BUILTIN(__builtin_nansf16b)))
#error "missing __bf16 builtins; define EKNAN_HAS_BF16=0 to skip it"
#endif

// The compiler builtins for the types without std support.
// Declared outside the anonymous namespace,
// because the specializations for the types with std support
// would be unused and warned about.
template <typename T>
struct builtins;

// For when copysign would quiet a signaling NaN.
// Only for IEEE formats, where the sign is the top bit.
template <typename T>
void set_sign_bit(T& value, bool negative)
{
    using bits_type = std::conditional_t<
        sizeof(T) == 2, std::uint16_t,
        std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>;
    static_assert(sizeof(bits_type) == sizeof(T));
    constexpr auto sign =
        static_cast<bits_type>(bits_type{1} << (sizeof(T) * 8 - 1));

    bits_type bits;
    std::memcpy(&bits, &value, sizeof(T));
    bits = static_cast<bits_type>(negative ? bits | sign : bits & ~sign);
    std::memcpy(&value, &bits, sizeof(T));
}

// Only defined where it's used, because of -Wunused-macros
#if EKNAN_HAS_FLOAT16 || EKNAN_HAS_FLOAT32 || EKNAN_HAS_FLOAT64 || \
    EKNAN_HAS_FLOAT128 || EKNAN_TEST_HAS_GNU_FLOAT128
// set_sign isn't done with negation, because that may convert to float and
// back, and RISC-V drops the sign of a NaN when converting it.
// Its sign constants are constexpr, because Clang 8 crashes when converting a
// double to __float128 at run time.
#define EKNAN_TEST_DEFINE_BUILTINS(T, suffix)                                  \
    template <>                                                                \
    struct builtins<T> {                                                       \
        static constexpr T quiet_nan = __builtin_nan##suffix("");              \
        static constexpr T signaling_nan = __builtin_nans##suffix("");         \
        static constexpr T infinity = static_cast<T>(__builtin_inff());        \
                                                                               \
        static void set_sign(T& value, bool negative)                          \
        {                                                                      \
            static constexpr auto one = static_cast<T>(1.0);                   \
            static constexpr auto minus_one = static_cast<T>(-1.0);            \
            value =                                                            \
                __builtin_copysign##suffix(value, negative ? minus_one : one); \
        }                                                                      \
    };
#endif

#if EKNAN_HAS_FLOAT16
EKNAN_TEST_DEFINE_BUILTINS(_Float16, f16)
#endif

#if EKNAN_HAS_FLOAT32
EKNAN_TEST_DEFINE_BUILTINS(_Float32, f32)
#endif

#if EKNAN_HAS_FLOAT64
EKNAN_TEST_DEFINE_BUILTINS(_Float64, f64)
#endif

#if EKNAN_HAS_FLOAT128
EKNAN_TEST_DEFINE_BUILTINS(_Float128, f128)
#endif

// Clang has no _Float128, and its f128 builtins take __float128
#if EKNAN_TEST_HAS_GNU_FLOAT128 && defined(__clang__)
EKNAN_TEST_DEFINE_BUILTINS(__float128, f128)
#elif EKNAN_TEST_HAS_GNU_FLOAT128
EKNAN_TEST_DEFINE_BUILTINS(__float128, q)
#endif

#if EKNAN_HAS_BF16
template <>
struct builtins<__bf16> {
    static constexpr auto infinity = static_cast<__bf16>(__builtin_inff());

#if defined(__clang__)
    // Clang has no NaN builtins for __bf16. These are the top 16 bits of the
    // float NaNs, and Clang has __bf16 only on IEEE 754-2008 targets.
    static_assert(!EKNAN_HAS_LEGACY_NAN_ENCODING,
                  "The __bf16 NaNs assume the IEEE 754-2008 NaN encoding");
    static constexpr auto quiet_nan =
        __builtin_bit_cast(__bf16, std::uint16_t{0x7fc0});
    static constexpr auto signaling_nan =
        __builtin_bit_cast(__bf16, std::uint16_t{0x7fa0});
#else
    // GCC has no __builtin_nanf16b
    static constexpr auto quiet_nan = static_cast<__bf16>(__builtin_nanf(""));
    static constexpr auto signaling_nan = __builtin_nansf16b("");
#endif
};
#endif

namespace {

// Whether <cmath> and numeric_limits support T. They support the extended
// types only in C++23, so compiler builtins are used for them otherwise.
template <typename T>
constexpr bool has_std_support =
    std::is_same_v<T, float> || std::is_same_v<T, double> ||
    std::is_same_v<T, long double>
#if defined(__STDCPP_FLOAT16_T__) && EKNAN_HAS_FLOAT16
    || std::is_same_v<T, _Float16>
#endif
#if defined(__STDCPP_FLOAT32_T__) && EKNAN_HAS_FLOAT32
    || std::is_same_v<T, _Float32>
#endif
#if defined(__STDCPP_FLOAT64_T__) && EKNAN_HAS_FLOAT64
    || std::is_same_v<T, _Float64>
#endif
#if defined(__STDCPP_FLOAT128_T__) && EKNAN_HAS_FLOAT128
    || std::is_same_v<T, _Float128>
#endif
#if defined(__STDCPP_BFLOAT16_T__) && EKNAN_HAS_BF16
    || std::is_same_v<T, __bf16>
#endif
    ;

// Whether copysign may quiet a signaling NaN. On 32-bit MSVC, it returns in an
// x87 register. On m68k, it loads a float or double into an extended-precision
// register. No compiler has a copysign builtin for __bf16, and libstdc++'s
// std::bfloat16_t overload converts to float.
template <typename T>
constexpr bool copysign_may_quiet =
#if defined(_M_IX86)
    true
#elif defined(__m68k__)
    sizeof(T) < sizeof(long double)
#else
    false
#endif
#if EKNAN_HAS_BF16
    || std::is_same_v<T, __bf16>
#endif
    ;

template <typename T>
void copy_nan(T& out, bool quiet)
{
#if defined(_MSC_VER) && !defined(__clang__)
    // MSVC's numeric_limits<float>::signaling_NaN() is quiet. This is the
    // __builtin_nansf("1") that it's defined as.
    if constexpr (std::is_same_v<T, float>) {
        if (!quiet) {
            constexpr std::uint32_t bits = 0x7f800001;
            std::memcpy(&out, &bits, sizeof(T));
            return;
        }
    }
#endif
    if constexpr (has_std_support<T>) {
        static constexpr T quiet_value = std::numeric_limits<T>::quiet_NaN();
        static constexpr T signaling_value =
            std::numeric_limits<T>::signaling_NaN();
        std::memcpy(&out, quiet ? &quiet_value : &signaling_value, sizeof(T));
    }
    else {
        std::memcpy(
            &out, quiet ? &builtins<T>::quiet_nan : &builtins<T>::signaling_nan,
            sizeof(T));
    }
}

}  // namespace

template <typename T>
bool isnan(const T& value)
{
    if constexpr (has_std_support<T>) {
        return std::isnan(value);
    }
    else {
        return __builtin_isnan(value);
    }
}

template <typename T>
bool signbit(const T& value)
{
    if constexpr (has_std_support<T>) {
        return std::signbit(value);
    }
    else {
        return __builtin_signbit(value);
    }
}

template <typename T>
void set_sign(T& value, bool negative)
{
    if constexpr (copysign_may_quiet<T>) {
        set_sign_bit(value, negative);
    }
    else if constexpr (has_std_support<T>) {
        value = std::copysign(value, static_cast<T>(negative ? -1.0 : 1.0));
    }
    else {
        builtins<T>::set_sign(value, negative);
    }
}

template <typename T>
void quiet_nan(T& out)
{
    copy_nan(out, true);
}

template <typename T>
void signaling_nan(T& out)
{
    copy_nan(out, false);
}

template <typename T>
void make_infinity(T& out)
{
    if constexpr (has_std_support<T>) {
        static constexpr T value = std::numeric_limits<T>::infinity();
        std::memcpy(&out, &value, sizeof(T));
    }
    else {
        std::memcpy(&out, &builtins<T>::infinity, sizeof(T));
    }
}

#define EKNAN_TEST_DEFINE_REFERENCE(T) \
    EKNAN_TEST_REFERENCE_INSTANTIATIONS(template, T)
EKNAN_TEST_FOR_EACH_FLOAT(EKNAN_TEST_DEFINE_REFERENCE)

}  // namespace stdlib_interface
