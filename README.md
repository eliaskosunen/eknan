# eknan

[![Linux builds](https://github.com/eliaskosunen/eknan/actions/workflows/linux.yml/badge.svg)](https://github.com/eliaskosunen/eknan/actions/workflows/linux.yml)
[![macOS builds](https://github.com/eliaskosunen/eknan/actions/workflows/macos.yml/badge.svg)](https://github.com/eliaskosunen/eknan/actions/workflows/macos.yml)
[![Windows builds](https://github.com/eliaskosunen/eknan/actions/workflows/windows.yml/badge.svg)](https://github.com/eliaskosunen/eknan/actions/workflows/windows.yml)
[![Other architectures](https://github.com/eliaskosunen/eknan/actions/workflows/arch.yml/badge.svg)](https://github.com/eliaskosunen/eknan/actions/workflows/arch.yml)
[![License](https://img.shields.io/github/license/eliaskosunen/eknan.svg)](https://github.com/eliaskosunen/eknan/blob/master/LICENSE)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17%2F20%2F23%2F26-blue.svg)](https://img.shields.io/badge/C%2B%2B-17%2F20%2F23%2F26-blue.svg)

```cpp
#include <eknan/eknan.hpp>
#include <cstdio>

int main() {
    double d;
    // A signaling NaN, with the payload 0x1234
    if (!eknan::make_snan(d, 0x1234)) {
        return 1;
    }

    if (!eknan::set_quiet(d)) {
        return 1;
    }
    // Prints "quiet: 1, payload: 0x1234"
    std::printf("quiet: %d, payload: %#llx\n", eknan::is_quiet(d),
                static_cast<unsigned long long>(eknan::get_payload(d)));
}
```

## What is this?

`eknan` is a small, header-only C++17 library for portably constructing and
inspecting NaNs, both quiet and signaling, and their payloads.

The C++ standard library doesn't offer much here: `std::nan("...")` has an
implementation-defined payload format, and only makes quiet NaNs.
In fact, some implementations ignore the payload argument altogether.
`std::strtod` provides similar yet unreliable functionality.
There's no support in the standard library for inspecting the payload.

C23 has `getpayload`, `setpayload`, and `setpayloadsig`, but they're not
part of C++, and not portably available. These are currently proposed for
inclusion in C++ with [P3935](https://wg21.link/p3935). They don't, however,
provide us with all the functionality this library does.

`eknan` was split out of [scnlib](https://github.com/eliaskosunen/scnlib),
where it's used for parsing strings like `nan(0x1234)`.
It's an optional dependency of scnlib.

## Explainer

An IEEE 754 binary floating-point number consists of a sign bit, an exponent,
and a fraction (the significand, without its leading bit).
When all bits of the exponent are set, the value is either an infinity (if the
fraction is zero), or a NaN (if it's not).
In a NaN, the fraction is split into two parts:

* The most significant bit is the *quiet bit*, which tells whether the NaN is
  quiet or signaling. Since IEEE 754-2008, a set quiet bit means a quiet NaN.
  Some older architectures (pre-2008 MIPS, PA-RISC) use the opposite
  convention.
* The remaining bits are the *payload*, which the hardware generally
  doesn't interpret.

For example, a `double` (IEEE 754 binary64):

```
 63  62       52  51  50                                               0
+---+-----------+---+--------------------------------------------------+
| s |  1 ... 1  | q |                  payload (51 bits)               |
+---+-----------+---+--------------------------------------------------+
```

Arithmetic on a quiet NaN produces a quiet NaN, without raising a
floating-point exception.
Arithmetic on a signaling NaN raises an invalid operation exception, and
produces a quiet NaN.
IEEE 754 recommends that the result carries the payload of an input NaN, so
a payload can be used to record where a NaN came from.
Most hardware does this, but it isn't guaranteed: for example, RISC-V always
produces the same canonical NaN.

Since a fraction of all zeros would be an infinity, not every payload is valid
in every NaN: with the IEEE 754-2008 encoding, a signaling NaN can't have a
payload of 0, and with the legacy encoding, a quiet NaN can't.

Other formats follow the same pattern, with some quirks:
the x87 80-bit `long double` has an explicit leading significand bit before the
quiet bit, and double-double (a pair of `double`s) is a NaN if its high
`double` is.

## Features

* Construct quiet and signaling NaNs, with an optional payload, given either
  as an integer or as a string
* Get and set NaN payloads, quiet bits, and sign bits
* Supports `float`, `double`, `long double` (in all of its forms: 64-bit,
  x87 80-bit extended, IEEE binary128, and double-double), and these extended
  types, where the compiler provides them:
  * `_Float16`, `_Float32`, `_Float64`, `_Float128` (GCC 13+; `_Float16` also
    in Clang), which are the C++23 `std::float16_t`, `std::float32_t`,
    `std::float64_t`, and `std::float128_t`, but are also available in C++17
  * `__bf16` (GCC 13+, and Clang 17+ on x86 and arm64), which is the C++23
    `std::bfloat16_t`
  * `__float128` (GCC and Clang)
  * A type is supported only if its format agrees with the expected binary
    layout (radix, digits, and maximum exponent), and it has quiet NaNs.
    The format is read from the compiler's predefined macros (like
    `__FLT16_MANT_DIG__`), or from `std::numeric_limits` if there are none.
    `std::numeric_limits` isn't required, because it isn't specialized for
    every extended type in every standard library version.
  * If the detection of an extended type is wrong for your compiler, define
    `EKNAN_HAS_FLOAT16`, `EKNAN_HAS_FLOAT32`, `EKNAN_HAS_FLOAT64`,
    `EKNAN_HAS_FLOAT128`, `EKNAN_HAS_BF16`, or `EKNAN_HAS_GNU_FLOAT128`
    to 0 before including `eknan.hpp`.
* Highly portable
  * Handles both the IEEE 754-2008 NaN encoding, and the legacy encoding
    with an inverted quiet bit (pre-2008 MIPS, PA-RISC)
  * Handles both byte orders, and platforms without `__int128`
  * Tested in CI on x86, x86-64, arm (v5 and v7), aarch64, riscv64,
    loongarch64, ppc, ppc64(le), s390x, sparc64, m68k, sh4, hppa, and MIPS
    (both NaN encodings)
* Unaffected by `-ffast-math` (or `/fp:fast`), because it inspects the bits of
  a value instead of relying on floating-point operations.
  The test suite is also run with it enabled.
* Signaling NaNs aren't accidentally quieted on i386, where loading a
  `float` or `double` into an x87 register does that: values are passed by
  reference, and copied with `std::memcpy`

## API overview

All functions are in namespace `eknan`, take the value by reference, and are
`noexcept`. Except for `get_payload_bit_count` and `get_padding_bit_count`,
none of the functions are `constexpr`.
Functions that can fail return a `[[nodiscard]] bool`, and leave the value
unchanged on failure: for example, when the payload doesn't fit, or when the
result would be an infinity instead of a NaN.

| Function                                          | Description                                                        |
|:--------------------------------------------------|:-------------------------------------------------------------------|
| `is_nan(v)`                                       | Is `v` a NaN                                                       |
| `is_quiet(v)`, `is_signaling(v)`                  | Is `v` a quiet/signaling NaN                                       |
| `make_qnan(out)`, `make_snan(out)`                | Default quiet/signaling NaN                                        |
| `make_qnan(out, payload)`, `make_snan(out, payload)` | Quiet/signaling NaN with a payload, as an integer or a string   |
| `get_payload(v)`, `set_payload(v, payload)`       | Payload of a NaN                                                   |
| `get_payload_bit_count<F>()`                      | Number of payload bits in `F`                                      |
| `set_quiet(v)`, `set_signaling(v)`                | Change the quietness of a NaN, keeping its payload                 |
| `get_signbit(v)`, `set_signbit(v, bit)`           | Sign bit, of any value                                             |
| `get_padding(v)`, `set_padding(v, bits)`, `get_padding_bit_count<F>()` | Bits outside the value (x87 `long double` only) |

Payload strings are decimal, hexadecimal (`0x`), or octal (`0`),
with optional `_` digit separators.
The payload type of `F` is `payload_type<F>`, an unsigned integer type.
For 128-bit payloads, it's `unsigned __int128`, or `eknan::uint128_polyfill` on
platforms without it. `uint128_polyfill` supports comparison, addition,
subtraction, bitwise and shift operators, and conversions to and from integer
types, but not multiplication or division.

See [`eknan.hpp`](include/eknan/eknan.hpp) for the documentation of
the full interface.

## Installing

`eknan` is header-only, so there's nothing to build: copy
`include/eknan/eknan.hpp` into your project, or use CMake, through
`add_subdirectory`, `FetchContent`, or `cmake --install` + `find_package`.
The CMake target is `eknan::eknan`.

```cmake
add_executable(my_program ...)
target_link_libraries(my_program eknan::eknan)
```

### Building the tests

The tests use googletest, which is downloaded with `FetchContent`, unless
`-DEKNAN_USE_EXTERNAL_GTEST=ON` is given.
They're built by default when `eknan` is the top-level CMake project.

```sh
cmake -S . -B build
cmake --build build
cd build && ctest
```

## Compiler support

A C++17-compatible compiler is required. The following compilers are tested in
CI:

* GCC 7 and newer
* Clang 8 and newer
* Apple Clang on macOS 15 and newer
* Visual Studio 2022 (MSVC and clang-cl), x86, x64, and arm64
* MinGW and MSYS2 (GCC and Clang)

CMake 3.16 or newer is required, if using CMake.

## License

eknan is licensed under the Apache License, version 2.0.  
Copyright (c) 2026 Elias Kosunen  
See LICENSE for further details.
