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

#include <eknan/eknan.hpp>

// Standard library operations to check eknan against. They're defined in
// stdlib_interface.cpp, which is compiled without -ffast-math, because with it
// the compiler may assume that no value is a NaN or an infinity. Values are
// passed by reference: returning a float or double on i386 quiets a signaling
// NaN.
namespace stdlib_interface {

template <typename T>
bool isnan(const T& value);

template <typename T>
bool signbit(const T& value);

template <typename T>
void set_sign(T& value, bool negative);

template <typename T>
void quiet_nan(T& out);

template <typename T>
void signaling_nan(T& out);

template <typename T>
void make_infinity(T& out);

// Applies X to every supported float type.
// __float128 is left out if it's the same type as long double (GCC on
// PowerPC with -mabi=ieeelongdouble), where it would be a duplicate.

#if EKNAN_HAS_FLOAT16
#define EKNAN_TEST_F16(X) X(_Float16)
#else
#define EKNAN_TEST_F16(X)
#endif

#if EKNAN_HAS_FLOAT32
#define EKNAN_TEST_F32(X) X(_Float32)
#else
#define EKNAN_TEST_F32(X)
#endif

#if EKNAN_HAS_FLOAT64
#define EKNAN_TEST_F64(X) X(_Float64)
#else
#define EKNAN_TEST_F64(X)
#endif

#if EKNAN_HAS_FLOAT128
#define EKNAN_TEST_F128(X) X(_Float128)
#else
#define EKNAN_TEST_F128(X)
#endif

#if EKNAN_HAS_BF16
#define EKNAN_TEST_BF16(X) X(__bf16)
#else
#define EKNAN_TEST_BF16(X)
#endif

#if EKNAN_HAS_GNU_FLOAT128 && !defined(__LONG_DOUBLE_IEEE128__)
#define EKNAN_TEST_HAS_GNU_FLOAT128 1
#define EKNAN_TEST_GNU_F128(X)      X(__float128)
#else
#define EKNAN_TEST_HAS_GNU_FLOAT128 0
#define EKNAN_TEST_GNU_F128(X)
#endif

#define EKNAN_TEST_FOR_EACH_FLOAT(X) \
    X(float)                         \
    X(double)                        \
    X(long double)                   \
    EKNAN_TEST_F16(X)                \
    EKNAN_TEST_F32(X)                \
    EKNAN_TEST_F64(X)                \
    EKNAN_TEST_F128(X)               \
    EKNAN_TEST_BF16(X)               \
    EKNAN_TEST_GNU_F128(X)

#define EKNAN_TEST_REFERENCE_INSTANTIATIONS(prefix, T) \
    prefix bool isnan<T>(const T&);                    \
    prefix bool signbit<T>(const T&);                  \
    prefix void set_sign<T>(T&, bool);                 \
    prefix void quiet_nan<T>(T&);                      \
    prefix void signaling_nan<T>(T&);                  \
    prefix void make_infinity<T>(T&);

// Instantiated in stdlib_interface.cpp
#define EKNAN_TEST_DECLARE_REFERENCE(T) \
    EKNAN_TEST_REFERENCE_INSTANTIATIONS(extern template, T)
EKNAN_TEST_FOR_EACH_FLOAT(EKNAN_TEST_DECLARE_REFERENCE)

}  // namespace stdlib_interface
