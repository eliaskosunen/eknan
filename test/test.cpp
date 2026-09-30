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

#include <eknan/eknan.hpp>

// googletest names the type of a typed test with typeid, and libc++abi has no
// typeinfo for _Float16, so linking would fail. Without RTTI, googletest names
// every type "<type>" instead.
#if defined(_LIBCPP_VERSION) && EKNAN_HAS_FLOAT16
#define GTEST_HAS_RTTI 0
#endif

#include <gtest/gtest.h>

#include "stdlib_interface.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if __cplusplus > 202002L && __has_include(<stdfloat>)
#include <stdfloat>
#define EKNAN_TEST_HAS_STDFLOAT 1
#else
#define EKNAN_TEST_HAS_STDFLOAT 0
#endif

#if (defined(__x86_64__) || defined(__i386__)) && \
    defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64
#define EKNAN_TEST_HAS_X87_LONG_DOUBLE 1
#else
#define EKNAN_TEST_HAS_X87_LONG_DOUBLE 0
#endif

namespace {

template <typename T>
using payload_t = eknan::payload_type<T>;

static_assert(
    std::is_same_v<payload_t<double>, decltype(eknan::get_payload(1.0))>);

template <typename T>
payload_t<T> max_payload()
{
    using I = payload_t<T>;
    return static_cast<I>((I{1} << eknan::get_payload_bit_count<T>()) - I{1});
}

template <typename T>
payload_t<T> too_large_payload()
{
    using I = payload_t<T>;
    return static_cast<I>(max_payload<T>() + I{1});
}

template <typename T>
std::uint64_t payload_of(const T& value)
{
    return static_cast<std::uint64_t>(eknan::get_payload(value));
}

template <typename T>
std::string to_hex(T value)
{
    std::string digits;
    do {
        digits.insert(digits.begin(),
                      "0123456789abcdef"[static_cast<unsigned>(value & T{15})]);
        value = static_cast<T>(value >> 4u);
    } while (value);
    return "0x" + digits;
}

template <typename To, typename From>
To bit_cast(const From& from)
{
    static_assert(sizeof(To) == sizeof(From));
    To to;
    std::memcpy(&to, &from, sizeof(To));
    return to;
}

// Copying a long double by value may not preserve its padding bytes, so these
// compare and copy all bytes with memcmp and memcpy
template <typename T>
bool same_bits(const T& a, const T& b)
{
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}

template <typename T>
void copy_bits(T& dest, const T& source)
{
    std::memcpy(&dest, &source, sizeof(T));
}

template <typename T>
constexpr T power_of_two(int exponent)
{
    const auto factor = static_cast<T>(exponent < 0 ? 0.5 : 2.0);
    auto result = static_cast<T>(1.0);
    for (int i = 0; i < (exponent < 0 ? -exponent : exponent); ++i) {
        result = static_cast<T>(result * factor);
    }
    return result;
}

// max, min, and denorm_min, for types without numeric_limits.
// Only correct for IEEE formats: the max of double-double would overflow.
template <typename T, typename = void>
struct finite_limits {
    static constexpr auto format = eknan::detail::float_type_info<T>::format;
    static constexpr T max = static_cast<T>(
        (static_cast<T>(2.0) - power_of_two<T>(1 - format.digits)) *
        power_of_two<T>(format.max_exponent - 1));
    static constexpr T min = power_of_two<T>(2 - format.max_exponent);
    static constexpr T denorm_min =
        power_of_two<T>(3 - format.max_exponent - format.digits);
};

template <typename T>
struct finite_limits<T,
                     std::enable_if_t<std::numeric_limits<T>::is_specialized>> {
    static constexpr T max = std::numeric_limits<T>::max();
    static constexpr T min = std::numeric_limits<T>::min();
    static constexpr T denorm_min = std::numeric_limits<T>::denorm_min();
};

template <typename T>
std::vector<T> non_nan_values()
{
    using limits = finite_limits<T>;

    T inf{};
    stdlib_interface::make_infinity(inf);
    T negative_inf{};
    stdlib_interface::make_infinity(negative_inf);
    stdlib_interface::set_sign(negative_inf, true);
    T negative_zero{};
    stdlib_interface::set_sign(negative_zero, true);
    T lowest{};
    copy_bits(lowest, limits::max);
    stdlib_interface::set_sign(lowest, true);

    // 1.5 has the top bit of the fraction (the quiet bit, in a NaN) set
    std::vector<T> values{
        static_cast<T>(0.0), negative_zero, static_cast<T>(1.0),
        static_cast<T>(1.5), inf,           negative_inf,
        limits::max,         lowest,        limits::min,
        limits::denorm_min};
    // Padding is otherwise indeterminate, so comparing it is unreliable
    for (auto& value : values) {
        EXPECT_TRUE(eknan::set_padding(value, 0));
    }
    return values;
}

// A NaN with an all-zero payload is an infinity when its quiet bit is 0 too:
// a signaling NaN with the IEEE 754-2008 encoding, and a quiet NaN with the
// legacy (MIPS, PA-RISC) encoding
constexpr bool legacy_nan_encoding = EKNAN_HAS_LEGACY_NAN_ENCODING;

constexpr bool zero_payload_is_infinity(bool quiet)
{
    return quiet == legacy_nan_encoding;
}

const char* quietness_name(bool quiet)
{
    return quiet ? "quiet" : "signaling";
}

template <typename T, typename Payload>
bool make_nan_of(T& out, bool quiet, const Payload& payload)
{
    if constexpr (std::is_integral_v<Payload> &&
                  !std::is_same_v<Payload, payload_t<T>>) {
        return make_nan_of(out, quiet, static_cast<payload_t<T>>(payload));
    }
    else {
        return quiet ? eknan::make_qnan(out, payload)
                     : eknan::make_snan(out, payload);
    }
}

template <typename T>
void make_default_nan(T& out, bool quiet)
{
    if (quiet) {
        eknan::make_qnan(out);
    }
    else {
        eknan::make_snan(out);
    }
}

template <typename T>
payload_t<T> default_payload(bool quiet)
{
    return quiet ? eknan::get_default_qnan_payload<T>()
                 : eknan::get_default_snan_payload<T>();
}

template <typename T, typename Payload>
bool can_make_nan(bool quiet, const Payload& payload)
{
    T value{};
    return make_nan_of(value, quiet, payload);
}

template <typename T>
void make_stdlib_nan(T& out, bool quiet)
{
    if (quiet) {
        stdlib_interface::quiet_nan(out);
    }
    else {
        stdlib_interface::signaling_nan(out);
    }
}

template <typename T>
bool set_quietness(T& value, bool quiet)
{
    return quiet ? eknan::set_quiet(value) : eknan::set_signaling(value);
}

template <typename T>
void expect_nan_kind(const T& value, bool quiet)
{
    EXPECT_TRUE(stdlib_interface::isnan(value));
    EXPECT_TRUE(eknan::is_nan(value));
    EXPECT_EQ(eknan::is_quiet(value), quiet);
    EXPECT_EQ(eknan::is_signaling(value), !quiet);
}

template <typename T, typename Payload>
void expect_nan(const T& value, bool quiet, Payload payload)
{
    expect_nan_kind(value, quiet);
    EXPECT_TRUE(eknan::get_payload(value) ==
                static_cast<payload_t<T>>(payload));
}

// Padding is otherwise indeterminate, so comparing it is unreliable
template <typename T>
void expect_same_bits_ignoring_padding(T& value, T& expected)
{
    ASSERT_TRUE(eknan::set_padding(value, 0));
    ASSERT_TRUE(eknan::set_padding(expected, 0));
    EXPECT_TRUE(same_bits(value, expected))
        << to_hex(eknan::get_payload(value)) << " "
        << to_hex(eknan::get_payload(expected));
}

template <typename Tuple>
struct to_gtest_types;

template <typename... Ts>
struct to_gtest_types<std::tuple<Ts...>> {
    using type = testing::Types<Ts...>;
};

}  // namespace

// Every type in EKNAN_TEST_FOR_EACH_FLOAT
#define EKNAN_TEST_TUPLE_OF(T) std::tuple<T>{},
using TypeList = to_gtest_types<decltype(std::tuple_cat(
    EKNAN_TEST_FOR_EACH_FLOAT(EKNAN_TEST_TUPLE_OF) std::tuple<>{}))>::type;

template <typename T>
class MakeNanTest : public testing::Test {};

TYPED_TEST_SUITE(MakeNanTest, TypeList, );

TYPED_TEST(MakeNanTest, Default)
{
    for (const bool quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(quiet));
        TypeParam value;
        std::memset(&value, 0xff, sizeof(TypeParam));
        make_default_nan(value, quiet);
        expect_nan(value, quiet, default_payload<TypeParam>(quiet));
        EXPECT_FALSE(eknan::get_signbit(value));
        EXPECT_EQ(eknan::get_padding(value), 0u);

        TypeParam expected;
        ASSERT_TRUE(
            make_nan_of(expected, quiet, default_payload<TypeParam>(quiet)));
        EXPECT_TRUE(same_bits(value, expected));
    }
}

TYPED_TEST(MakeNanTest, DefaultPayload)
{
    using I = payload_t<TypeParam>;
    const auto top_bit =
        static_cast<I>(I{1} << (eknan::get_payload_bit_count<TypeParam>() - 1));
    if constexpr (legacy_nan_encoding) {
        EXPECT_TRUE(eknan::get_default_qnan_payload<TypeParam>() == top_bit);
        EXPECT_TRUE(eknan::get_default_snan_payload<TypeParam>() == I{0});
    }
    else {
        EXPECT_TRUE(eknan::get_default_qnan_payload<TypeParam>() == I{0});
        EXPECT_TRUE(eknan::get_default_snan_payload<TypeParam>() == top_bit);
    }
}

// Not guaranteed by the API, but documented: the defaults are chosen to have
// the same bits as the standard library's NaNs on common platforms
TYPED_TEST(MakeNanTest, DefaultMatchesPlatformOnIeee2008)
{
#if defined(__m68k__)
    GTEST_SKIP() << "GCC's default NaNs have every payload bit set on m68k";
#endif
    if constexpr (legacy_nan_encoding) {
        GTEST_SKIP() << "The default NaNs vary by compiler on legacy encodings";
    }
    for (const bool quiet : {true, false}) {
#ifdef _MSVC_STL_VERSION
        // The MSVC STL's signaling NaNs are __builtin_nans("1")
        if (!quiet && std::numeric_limits<TypeParam>::has_signaling_NaN) {
            continue;
        }
#endif
        SCOPED_TRACE(quietness_name(quiet));
        TypeParam value;
        make_default_nan(value, quiet);

        TypeParam expected;
        make_stdlib_nan(expected, quiet);
        expect_same_bits_ignoring_padding(value, expected);
    }
}

TEST(DefaultPayloadTest, KnownValues)
{
    constexpr bool legacy = legacy_nan_encoding;
    static_assert(eknan::get_default_qnan_payload<float>() ==
                  (legacy ? 0x200000u : 0u));
    static_assert(eknan::get_default_snan_payload<float>() ==
                  (legacy ? 0u : 0x200000u));
    static_assert(eknan::get_default_qnan_payload<double>() ==
                  (legacy ? std::uint64_t{1} << 50 : 0u));
    static_assert(eknan::get_default_snan_payload<double>() ==
                  (legacy ? 0u : std::uint64_t{1} << 50));
    SUCCEED();
}

TYPED_TEST(MakeNanTest, WithPayload)
{
    for (const bool quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(quiet));
        for (const auto& payload :
             {payload_t<TypeParam>{32}, max_payload<TypeParam>()}) {
            TypeParam from_integer;
            ASSERT_TRUE(make_nan_of(from_integer, quiet, payload));
            expect_nan(from_integer, quiet, payload);

            TypeParam from_string;
            ASSERT_TRUE(make_nan_of(from_string, quiet, to_hex(payload)));
            expect_nan(from_string, quiet, payload);
        }
    }
}

TYPED_TEST(MakeNanTest, ZeroPayload)
{
    for (const bool quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(quiet));
        const bool is_valid = !zero_payload_is_infinity(quiet);

        TypeParam value{};
        EXPECT_EQ(make_nan_of(value, quiet, payload_t<TypeParam>{}), is_valid);
        if (is_valid) {
            expect_nan(value, quiet, 0u);
        }
        EXPECT_EQ(can_make_nan<TypeParam>(quiet, "0"), is_valid);
    }
}

TYPED_TEST(MakeNanTest, PayloadTooLarge)
{
    const auto too_large = too_large_payload<TypeParam>();
    auto original = static_cast<TypeParam>(1.0);
    ASSERT_TRUE(eknan::set_padding(original, 0));

    for (const bool quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(quiet));
        TypeParam value;
        copy_bits(value, original);
        EXPECT_FALSE(make_nan_of(value, quiet, too_large));
        EXPECT_FALSE(make_nan_of(value, quiet, to_hex(too_large)));
        EXPECT_FALSE(
            make_nan_of(value, quiet, to_hex(max_payload<TypeParam>()) + "0"));
        EXPECT_TRUE(same_bits(value, original));
    }
}

TEST(NanEncodingTest, KnownBits)
{
    float qnan_float;
    double qnan_double;
    ASSERT_TRUE(eknan::make_qnan(qnan_float, 1u));
    ASSERT_TRUE(eknan::make_qnan(qnan_double, 1u));
    if constexpr (legacy_nan_encoding) {
        EXPECT_EQ(bit_cast<std::uint32_t>(qnan_float), 0x7f800001u);
        EXPECT_EQ(bit_cast<std::uint64_t>(qnan_double), 0x7ff0000000000001u);
    }
    else {
        EXPECT_EQ(bit_cast<std::uint32_t>(qnan_float), 0x7fc00001u);
        EXPECT_EQ(bit_cast<std::uint64_t>(qnan_double), 0x7ff8000000000001u);
    }

    float negative;
    ASSERT_TRUE(eknan::make_qnan(negative, 0x2abcdeu));
    eknan::set_signbit(negative, true);
    EXPECT_EQ(bit_cast<std::uint32_t>(negative) & 0xffbfffffu, 0xffaabcdeu);
}

TEST(MakeNanParsingTest, Valid)
{
    const std::pair<const char*, std::uint64_t> cases[] = {
        {"32", 32},         {"1_000", 1000},    {"0x1234", 0x1234},
        {"0X1234", 0x1234}, {"0xABCD", 0xabcd}, {"0xabcdef", 0xabcdef},
        {"01234", 01234},   {"0x_1_f", 0x1f},   {"0_17", 017},
        {"1_", 1},          {"1__2", 12},       {"0x_1_2_", 0x12},
        {"0X__f", 0xf},     {"0_7", 7},         {"0__7_", 7},
        {"0", 0},           {"0_", 0},          {"0__0", 0},
        {"0x_0", 0},        {"0x0_", 0},
    };
    for (const auto& [str, payload] : cases) {
        for (const bool quiet : {true, false}) {
            SCOPED_TRACE(std::string{str} + ' ' + quietness_name(quiet));
            if (payload == 0 && zero_payload_is_infinity(quiet)) {
                EXPECT_FALSE(can_make_nan<double>(quiet, str));
                continue;
            }
            double value;
            ASSERT_TRUE(make_nan_of(value, quiet, str));
            expect_nan(value, quiet, payload);
        }
    }
}

TEST(MakeNanParsingTest, Invalid)
{
    for (const char* str :
         {"",     "_",     "__",    "0x_",     "0x",    "0X",  "_1", "_0",
          "_010", "_0x12", "0_x12", "__0X_f",  "-1",    "+1",  " 1", "1 ",
          "1.0",  "12a",   "1f",    "Foo_Bar", "0x12g", "089", "x"}) {
        for (const bool quiet : {true, false}) {
            SCOPED_TRACE(std::string{str} + ' ' + quietness_name(quiet));
            EXPECT_FALSE(can_make_nan<double>(quiet, str));
        }
    }
}

TEST(MakeNanParsingTest, StringTypes)
{
    double from_literal;
    ASSERT_TRUE(eknan::make_qnan(from_literal, "11"));
    EXPECT_EQ(payload_of(from_literal), 11u);

    const char* c_str = "12";
    double from_c_str;
    ASSERT_TRUE(eknan::make_qnan(from_c_str, c_str));
    EXPECT_EQ(payload_of(from_c_str), 12u);

    double from_string;
    ASSERT_TRUE(eknan::make_snan(from_string, std::string{"13"}));
    EXPECT_EQ(payload_of(from_string), 13u);

    double from_string_view;
    ASSERT_TRUE(eknan::make_qnan(from_string_view, std::string_view{"14"}));
    EXPECT_EQ(payload_of(from_string_view), 14u);
}

template <typename T>
class PayloadTest : public testing::Test {};

TYPED_TEST_SUITE(PayloadTest, TypeList, );

TYPED_TEST(PayloadTest, SetPayload)
{
    for (const bool quiet : {true, false}) {
        for (const bool negative : {false, true}) {
            SCOPED_TRACE(std::string{quietness_name(quiet)} +
                         (negative ? " negative" : " positive"));
            TypeParam value;
            make_stdlib_nan(value, quiet);
            stdlib_interface::set_sign(value, negative);

            for (const auto& payload :
                 {payload_t<TypeParam>{42}, payload_t<TypeParam>{48},
                  max_payload<TypeParam>()}) {
                EXPECT_TRUE(eknan::set_payload(value, payload));
                expect_nan(value, quiet, payload);
                EXPECT_EQ(stdlib_interface::signbit(value), negative);
            }
        }
    }
}

TYPED_TEST(PayloadTest, SetPayload_Zero)
{
    for (const bool quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(quiet));
        const bool is_valid = !zero_payload_is_infinity(quiet);

        TypeParam value;
        ASSERT_TRUE(make_nan_of(value, quiet, 48u));
        EXPECT_EQ(eknan::set_payload(value, 0u), is_valid);
        expect_nan(value, quiet, is_valid ? 0u : 48u);
    }
}

TYPED_TEST(PayloadTest, SetPayload_TooLarge)
{
    using I = payload_t<TypeParam>;
    TypeParam value;
    ASSERT_TRUE(eknan::make_qnan(value, 48u));
    EXPECT_FALSE(eknan::set_payload(value, too_large_payload<TypeParam>()));
    EXPECT_FALSE(eknan::set_payload(value, static_cast<I>(~I{})));
    expect_nan(value, true, 48u);
}

TEST(PayloadBitCountTest, KnownValues)
{
    static_assert(eknan::get_payload_bit_count<float>() == 22);
    static_assert(eknan::get_payload_bit_count<double>() == 51);
#if EKNAN_HAS_FLOAT16
    static_assert(eknan::get_payload_bit_count<_Float16>() == 9);
#endif
#if EKNAN_HAS_FLOAT32
    static_assert(eknan::get_payload_bit_count<_Float32>() == 22);
#endif
#if EKNAN_HAS_FLOAT64
    static_assert(eknan::get_payload_bit_count<_Float64>() == 51);
#endif
#if EKNAN_HAS_FLOAT128
    static_assert(eknan::get_payload_bit_count<_Float128>() == 111);
#endif
#if EKNAN_HAS_BF16
    static_assert(eknan::get_payload_bit_count<__bf16>() == 6);
#endif
#if EKNAN_TEST_HAS_GNU_FLOAT128
    static_assert(eknan::get_payload_bit_count<__float128>() == 111);
#endif
    SUCCEED();
}

template <typename T>
class TypeListTest : public testing::Test {};

TYPED_TEST_SUITE(TypeListTest, TypeList, );

// A static check, so that a platform where a type in TypeList unexpectedly
// becomes unsupported fails to build with an error that names the type
TYPED_TEST(TypeListTest, Supported)
{
    static_assert(eknan::detail::is_supported_float_type<TypeParam>);
    SUCCEED();
}

#if EKNAN_TEST_HAS_STDFLOAT
// eknan only specializes for _FloatN and __bf16, which libstdc++ uses for the
// <stdfloat> types. This fails if a standard library makes them distinct types.
// If they're the same types, the typed tests cover them already.
TEST(StdFloatTest, Supported)
{
    using eknan::detail::is_supported_float_type;
#ifdef __STDCPP_FLOAT16_T__
    static_assert(is_supported_float_type<std::float16_t>);
#endif
#ifdef __STDCPP_FLOAT32_T__
    static_assert(is_supported_float_type<std::float32_t>);
#endif
#ifdef __STDCPP_FLOAT64_T__
    static_assert(is_supported_float_type<std::float64_t>);
#endif
#ifdef __STDCPP_FLOAT128_T__
    static_assert(is_supported_float_type<std::float128_t>);
#endif
#ifdef __STDCPP_BFLOAT16_T__
    static_assert(is_supported_float_type<std::bfloat16_t>);
#endif
    SUCCEED();
}
#endif

TEST(SupportedTypeTest, Unsupported)
{
    using eknan::detail::is_supported_float_type;
    static_assert(!is_supported_float_type<int>);
    static_assert(!is_supported_float_type<unsigned char>);
    SUCCEED();
}

TEST(SupportedTypeTest, LayoutMismatch)
{
    using eknan::detail::matches_repr;
    static_assert(matches_repr<float, eknan::detail::nan_repr_f32>());
    static_assert(!matches_repr<float, eknan::detail::nan_repr_f64>());
    static_assert(!matches_repr<double, eknan::detail::nan_repr_f32>());
    static_assert(!matches_repr<double, eknan::detail::nan_repr_f128>());
    static_assert(!matches_repr<float, void>());
    SUCCEED();
}

template <typename T>
class SignbitTest : public testing::Test {};

TYPED_TEST_SUITE(SignbitTest, TypeList, );

TYPED_TEST(SignbitTest, GetSignbit)
{
    TypeParam value;
    stdlib_interface::quiet_nan(value);
    EXPECT_FALSE(eknan::get_signbit(value));
    EXPECT_EQ(eknan::get_signbit(value), stdlib_interface::signbit(value));

    stdlib_interface::set_sign(value, true);
    EXPECT_TRUE(eknan::get_signbit(value));
    EXPECT_EQ(eknan::get_signbit(value), stdlib_interface::signbit(value));
}

TYPED_TEST(SignbitTest, SetSignbit)
{
    for (const bool quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(quiet));
        TypeParam value;
        ASSERT_TRUE(make_nan_of(value, quiet, 32u));

        // Each sign twice, so that setting the current sign is tested too
        for (const bool negative : {true, true, false, false}) {
            eknan::set_signbit(value, negative);
            EXPECT_EQ(eknan::get_signbit(value), negative);
            EXPECT_EQ(stdlib_interface::signbit(value), negative);
            expect_nan(value, quiet, 32u);
        }
    }
}

TYPED_TEST(SignbitTest, SetSignbit_NonNan)
{
    auto values = non_nan_values<TypeParam>();
    // In a double-double, the low double of this is nonzero, and has the same
    // sign as the high double
    values.push_back(static_cast<TypeParam>(static_cast<TypeParam>(1.0) +
                                            power_of_two<TypeParam>(-60)));
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (const bool negative : {true, false}) {
            SCOPED_TRACE(std::to_string(i) +
                         (negative ? " negative" : " positive"));
            TypeParam expected;
            TypeParam actual;
            copy_bits(expected, values[i]);
            copy_bits(actual, values[i]);

            stdlib_interface::set_sign(expected, negative);
            eknan::set_signbit(actual, negative);

            EXPECT_EQ(eknan::get_signbit(actual), negative);
            expect_same_bits_ignoring_padding(actual, expected);
        }
    }
}

template <typename T>
class QuietTest : public testing::Test {};

TYPED_TEST_SUITE(QuietTest, TypeList, );

TYPED_TEST(QuietTest, IsQuiet)
{
    for (const bool quiet : {true, false}) {
        for (const bool negative : {false, true}) {
            SCOPED_TRACE(std::string{quietness_name(quiet)} +
                         (negative ? " negative" : " positive"));
            TypeParam value;
            make_stdlib_nan(value, quiet);
            eknan::set_signbit(value, negative);
            expect_nan_kind(value, quiet);
        }
    }
}

TYPED_TEST(QuietTest, SetQuietness)
{
    for (const bool from_quiet : {true, false}) {
        for (const bool to_quiet : {true, false}) {
            SCOPED_TRACE(std::string{quietness_name(from_quiet)} + " to " +
                         quietness_name(to_quiet));
            TypeParam value;
            ASSERT_TRUE(make_nan_of(value, from_quiet, 32u));
            eknan::set_signbit(value, true);

            EXPECT_TRUE(set_quietness(value, to_quiet));
            expect_nan(value, to_quiet, 32u);
            EXPECT_TRUE(eknan::get_signbit(value));
        }
    }
}

TYPED_TEST(QuietTest, SetQuietness_ZeroPayload)
{
    // The only quietness a NaN with payload 0 can have
    constexpr bool from_quiet = !zero_payload_is_infinity(true);
    for (const bool to_quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(to_quiet));
        TypeParam value;
        ASSERT_TRUE(make_nan_of(value, from_quiet, 0u));

        EXPECT_EQ(set_quietness(value, to_quiet),
                  !zero_payload_is_infinity(to_quiet));
        expect_nan(value, from_quiet, 0u);
    }
}

template <typename T>
class NonNanTest : public testing::Test {};

TYPED_TEST_SUITE(NonNanTest, TypeList, );

TYPED_TEST(NonNanTest, Rejected)
{
    for (const auto& original : non_nan_values<TypeParam>()) {
        TypeParam value;
        copy_bits(value, original);
        EXPECT_FALSE(eknan::is_nan(value));
        EXPECT_FALSE(eknan::is_quiet(value));
        EXPECT_FALSE(eknan::is_signaling(value));
        EXPECT_FALSE(eknan::set_payload(value, 1u));
        EXPECT_FALSE(eknan::set_quiet(value));
        EXPECT_FALSE(eknan::set_signaling(value));
        EXPECT_TRUE(same_bits(value, original));
    }
}

template <typename T>
class PaddingTest : public testing::Test {};

TYPED_TEST_SUITE(PaddingTest, TypeList, );

template <typename T>
std::uint64_t max_padding()
{
    const auto bits = eknan::get_padding_bit_count<T>();
    return bits == 0 ? 0 : (std::uint64_t{1} << bits) - 1;
}

TYPED_TEST(PaddingTest, BitCount)
{
    // Only the 80-bit extended format has padding
    constexpr bool extended =
        eknan::detail::float_type_info<TypeParam>::format.digits == 64;
    constexpr std::size_t expected = extended ? sizeof(TypeParam) * 8 - 80 : 0;
    EXPECT_EQ(eknan::get_padding_bit_count<TypeParam>(), expected);
}

TYPED_TEST(PaddingTest, MakeNanZeroesPadding)
{
    for (const bool quiet : {true, false}) {
        SCOPED_TRACE(quietness_name(quiet));
        TypeParam value{};
        ASSERT_TRUE(eknan::set_padding(value, max_padding<TypeParam>()));
        ASSERT_TRUE(make_nan_of(value, quiet, 32u));
        EXPECT_EQ(eknan::get_padding(value), 0u);

        ASSERT_TRUE(eknan::set_padding(value, max_padding<TypeParam>()));
        make_default_nan(value, quiet);
        EXPECT_EQ(eknan::get_padding(value), 0u);
    }
}

TYPED_TEST(PaddingTest, RoundTrip)
{
    const auto max = max_padding<TypeParam>();
    TypeParam value;
    ASSERT_TRUE(eknan::make_snan(value, 32u));
    eknan::set_signbit(value, true);

    for (const auto padding :
         {max, std::uint64_t{0}, 0xa5a5a5a5a5a5a5a5u & max, max >> 1}) {
        EXPECT_TRUE(eknan::set_padding(value, padding));
        EXPECT_EQ(eknan::get_padding(value), padding);

        EXPECT_TRUE(stdlib_interface::isnan(value));
        EXPECT_TRUE(stdlib_interface::signbit(value));
        EXPECT_TRUE(eknan::is_signaling(value));
        EXPECT_EQ(payload_of(value), 32u);
    }
}

TYPED_TEST(PaddingTest, DoesNotChangeValue)
{
    for (const auto& original : non_nan_values<TypeParam>()) {
        TypeParam value;
        copy_bits(value, original);
        EXPECT_TRUE(eknan::set_padding(value, max_padding<TypeParam>()));
        EXPECT_TRUE(eknan::set_padding(value, 0));
        EXPECT_TRUE(same_bits(value, original));
    }
}

TYPED_TEST(PaddingTest, TooLarge)
{
    const auto bits = eknan::get_padding_bit_count<TypeParam>();
    TypeParam value;
    ASSERT_TRUE(eknan::make_qnan(value, 32u));
    TypeParam original;
    copy_bits(original, value);

    EXPECT_FALSE(eknan::set_padding(value, std::uint64_t{1} << bits));
    EXPECT_FALSE(eknan::set_padding(value, ~std::uint64_t{0}));
    EXPECT_TRUE(same_bits(value, original));
}

#if EKNAN_TEST_HAS_X87_LONG_DOUBLE
TEST(X87PaddingTest, Layout)
{
    // Bytes 10 and up, least significant first
    long double value;
    eknan::make_qnan(value);
    ASSERT_TRUE(eknan::set_padding(
        value, 0x123456789abcu & max_padding<long double>()));
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(long double));
    const unsigned char expected[] = {0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12};
    for (std::size_t i = 10; i < sizeof(long double); ++i) {
        EXPECT_EQ(bytes[i], expected[i - 10]) << i;
    }
}
#endif

// Checks is_nan and get_signbit against the standard library, over bit
// patterns with and without an all-ones exponent
template <typename F, typename Bits>
void check_against_std(Bits bits)
{
    static_assert(sizeof(F) == sizeof(Bits));
    F value;
    std::memcpy(&value, &bits, sizeof(F));
    EXPECT_EQ(eknan::is_nan(value), stdlib_interface::isnan(value))
        << std::hex << bits;
    EXPECT_EQ(eknan::get_signbit(value), stdlib_interface::signbit(value))
        << std::hex << bits;
}

template <typename F, typename Bits>
void check_against_std_sampled(Bits exponent_mask)
{
    // splitmix64
    std::uint64_t state = 0x9e3779b97f4a7c15u;
    const auto next = [&]() {
        std::uint64_t z = (state += 0x9e3779b97f4a7c15u);
        z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9u;
        z = (z ^ (z >> 27u)) * 0x94d049bb133111ebu;
        return static_cast<Bits>(z ^ (z >> 31u));
    };
    for (int i = 0; i < 10000; ++i) {
        const auto bits = next();
        check_against_std<F>(bits);
        check_against_std<F>(static_cast<Bits>(bits | exponent_mask));
    }
}

TEST(StdAgreementTest, Float)
{
    check_against_std_sampled<float, std::uint32_t>(0x7f800000u);
}

TEST(StdAgreementTest, Double)
{
    check_against_std_sampled<double, std::uint64_t>(0x7ff0000000000000u);
}

template <typename F>
void check_against_std_exhaustive()
{
    for (std::uint32_t bits = 0; bits <= 0xffffu; ++bits) {
        check_against_std<F>(static_cast<std::uint16_t>(bits));
    }
}

#if EKNAN_HAS_FLOAT16
TEST(StdAgreementTest, Float16_Exhaustive)
{
    check_against_std_exhaustive<_Float16>();
}
#endif

#if EKNAN_HAS_BF16
TEST(StdAgreementTest, BFloat16_Exhaustive)
{
    check_against_std_exhaustive<__bf16>();
}
#endif

#if EKNAN_TEST_HAS_X87_LONG_DOUBLE
namespace {

// The explicit integer bit, the top bit of the significand
constexpr std::size_t x87_integer_bit_byte = 7;
constexpr unsigned char x87_integer_bit = 0x80;

bool has_integer_bit(const long double& value)
{
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(long double));
    return (bytes[x87_integer_bit_byte] & x87_integer_bit) != 0;
}

void clear_integer_bit(long double& value)
{
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(long double));
    bytes[x87_integer_bit_byte] &= static_cast<unsigned char>(~x87_integer_bit);
    std::memcpy(&value, bytes, sizeof(long double));
}

}  // namespace

TEST(IsNanTest, X87PseudoNan)
{
    // A pseudo-NaN (integer bit 0) is a NaN
    long double pseudo_nan;
    ASSERT_TRUE(eknan::make_snan(pseudo_nan, 32u));
    ASSERT_TRUE(has_integer_bit(pseudo_nan));
    clear_integer_bit(pseudo_nan);
    EXPECT_TRUE(eknan::is_nan(pseudo_nan));
    EXPECT_EQ(payload_of(pseudo_nan), 32u);

    // A pseudo-infinity (integer bit 0, fraction 0) isn't
    long double pseudo_inf;
    stdlib_interface::make_infinity(pseudo_inf);
    clear_integer_bit(pseudo_inf);
    EXPECT_FALSE(eknan::is_nan(pseudo_inf));

    // set_payload restores the integer bit
    EXPECT_TRUE(eknan::set_payload(pseudo_nan, 33u));
    EXPECT_TRUE(has_integer_bit(pseudo_nan));
    EXPECT_TRUE(stdlib_interface::isnan(pseudo_nan));
}
#endif

using eknan::uint128_polyfill;

TEST(Uint128PolyfillTest, Construction)
{
    const uint128_polyfill halves{1, 2};
    EXPECT_EQ(halves.high, 1u);
    EXPECT_EQ(halves.low, 2u);
    EXPECT_TRUE(uint128_polyfill{} == uint128_polyfill(0, 0));
    EXPECT_TRUE(uint128_polyfill{42u} == uint128_polyfill(0, 42));
    EXPECT_TRUE(uint128_polyfill{std::uint16_t{7}} == uint128_polyfill(0, 7));
    // Sign-extends, like a conversion to unsigned __int128
    EXPECT_TRUE(uint128_polyfill{-1} == uint128_polyfill(~0ull, ~0ull));
    EXPECT_TRUE(uint128_polyfill{-2ll} == uint128_polyfill(~0ull, ~1ull));
}

TEST(Uint128PolyfillTest, Comparison)
{
    const auto small = uint128_polyfill(0, ~0ull);
    const auto large = uint128_polyfill(1, 0);

    EXPECT_TRUE(small < large);
    EXPECT_FALSE(large < small);
    EXPECT_TRUE(large > small);
    EXPECT_FALSE(small > large);

    EXPECT_TRUE(small <= large);
    EXPECT_FALSE(large <= small);
    EXPECT_TRUE(small <= small);

    EXPECT_TRUE(large >= small);
    EXPECT_FALSE(small >= large);
    EXPECT_TRUE(large >= large);

    EXPECT_TRUE(small != large);
    EXPECT_FALSE(small == large);
    EXPECT_TRUE(uint128_polyfill(0, 2) < uint128_polyfill(0, 3));

    EXPECT_TRUE(static_cast<bool>(uint128_polyfill(1, 0)));
    EXPECT_TRUE(static_cast<bool>(uint128_polyfill(0, 1)));
    EXPECT_FALSE(static_cast<bool>(uint128_polyfill{}));
}

TEST(Uint128PolyfillTest, AdditionAndSubtraction)
{
    EXPECT_TRUE(uint128_polyfill(0, ~0ull) + uint128_polyfill{1u} ==
                uint128_polyfill(1, 0));
    EXPECT_TRUE(uint128_polyfill(1, 0) - uint128_polyfill{1u} ==
                uint128_polyfill(0, ~0ull));
    EXPECT_TRUE(uint128_polyfill{} - uint128_polyfill{1u} ==
                uint128_polyfill(~0ull, ~0ull));

    auto value = uint128_polyfill(2, 3);
    value += uint128_polyfill(4, 5);
    EXPECT_TRUE(value == uint128_polyfill(6, 8));
    value -= uint128_polyfill(6, 9);
    EXPECT_TRUE(value == uint128_polyfill(~0ull, ~0ull));
}

TEST(Uint128PolyfillTest, Bitwise)
{
    const auto a = uint128_polyfill(0xf0, 0x0f);
    const auto b = uint128_polyfill(0x3c, 0x3c);
    EXPECT_TRUE((a & b) == uint128_polyfill(0x30, 0x0c));
    EXPECT_TRUE((a | b) == uint128_polyfill(0xfc, 0x3f));
    EXPECT_TRUE((a ^ b) == uint128_polyfill(0xcc, 0x33));
    EXPECT_TRUE(~a == uint128_polyfill(~0xf0ull, ~0x0full));

    auto value = a;
    value &= b;
    EXPECT_TRUE(value == uint128_polyfill(0x30, 0x0c));
    value |= uint128_polyfill(1, 1);
    EXPECT_TRUE(value == uint128_polyfill(0x31, 0x0d));
    const auto copy = value;
    value ^= copy;
    EXPECT_TRUE(value == uint128_polyfill{});
}

TEST(Uint128PolyfillTest, Shift)
{
    const auto one = uint128_polyfill{1u};
    EXPECT_TRUE((one << 0u) == one);
    EXPECT_TRUE((one << 1u) == uint128_polyfill(0, 2));
    EXPECT_TRUE((one << 63u) == uint128_polyfill(0, 1ull << 63));
    EXPECT_TRUE((one << 64u) == uint128_polyfill(1, 0));
    EXPECT_TRUE((one << 127u) == uint128_polyfill(1ull << 63, 0));
    EXPECT_TRUE((one << 128u) == uint128_polyfill{});
    EXPECT_TRUE((uint128_polyfill(0, 0xff00000000000000u) << 4u) ==
                uint128_polyfill(0xf, 0xf000000000000000u));

    const auto top = uint128_polyfill(1ull << 63, 0);
    EXPECT_TRUE((top >> 0u) == top);
    EXPECT_TRUE((top >> 63u) == uint128_polyfill(1, 0));
    EXPECT_TRUE((top >> 64u) == uint128_polyfill(0, 1ull << 63));
    EXPECT_TRUE((top >> 127u) == one);
    EXPECT_TRUE((top >> 128u) == uint128_polyfill{});
    EXPECT_TRUE((uint128_polyfill(0xf, 0) >> 4u) ==
                uint128_polyfill(0, 0xf000000000000000u));

    auto value = one;
    value <<= 100u;
    value >>= 99u;
    EXPECT_TRUE(value == uint128_polyfill(0, 2));
}

TEST(Uint128PolyfillTest, Conversion)
{
    const auto value = uint128_polyfill(1, 0x1234567890abcdefu);
    EXPECT_EQ(static_cast<unsigned long long>(value), 0x1234567890abcdefull);
    EXPECT_EQ(static_cast<unsigned>(value), 0x90abcdefu);
    EXPECT_EQ(static_cast<std::uint8_t>(value), 0xefu);
    EXPECT_EQ(static_cast<std::uint16_t>(value), 0xcdefu);
    EXPECT_EQ(static_cast<std::uint64_t>(value), 0x1234567890abcdefu);

    const uint128_polyfill all_ones{~0ull, ~0ull};
    EXPECT_EQ(static_cast<std::int64_t>(all_ones), -1);
    EXPECT_EQ(static_cast<int>(all_ones), -1);
    EXPECT_EQ(static_cast<std::int64_t>(uint128_polyfill{-5}), -5);
}
