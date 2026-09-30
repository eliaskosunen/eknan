if(NOT TARGET GTest::gtest)
    if(EKNAN_USE_EXTERNAL_GTEST)
        find_package(GTest REQUIRED)
    else()
        include(FetchContent)

        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG ${EKNAN_GTEST_VERSION}
                SYSTEM
                EXCLUDE_FROM_ALL
            )
            FetchContent_MakeAvailable(googletest)
        else()
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG ${EKNAN_GTEST_VERSION}
            )

            FetchContent_GetProperties(googletest)
            if(NOT googletest_POPULATED)
                FetchContent_Populate(googletest)

                add_library(gtest
                    "${googletest_SOURCE_DIR}/googletest/src/gtest-all.cc"
                )
                target_include_directories(gtest SYSTEM PUBLIC
                    "${googletest_SOURCE_DIR}/googletest/include"
                )
                target_include_directories(gtest PRIVATE
                    "${googletest_SOURCE_DIR}/googletest"
                )

                find_package(Threads REQUIRED)
                target_link_libraries(gtest PUBLIC Threads::Threads)

                add_library(gtest_main
                    "${googletest_SOURCE_DIR}/googletest/src/gtest_main.cc"
                )
                target_link_libraries(gtest_main PUBLIC gtest)

                add_library(GTest::gtest ALIAS gtest)
                add_library(GTest::gtest_main ALIAS gtest_main)
            endif()
        endif()
    endif()
endif()

# The standard library interface, which must not be compiled with -ffast-math
if(MSVC)
    set_source_files_properties(stdlib_interface.cpp PROPERTIES COMPILE_OPTIONS /fp:precise)
else()
    set_source_files_properties(stdlib_interface.cpp PROPERTIES COMPILE_OPTIONS -fno-fast-math)
endif()

include(CheckCXXCompilerFlag)

# googletest builds itself with -WX on clang-cl, and before v1.18.0, its
# char8_t PrintTo triggers Clang 21's -Wcharacter-conversion in C++20 and later
if(TARGET gtest)
    check_cxx_compiler_flag(-Wcharacter-conversion EKNAN_HAS_FLAG_Wcharacter_conversion)
    if(EKNAN_HAS_FLAG_Wcharacter_conversion)
        target_compile_options(gtest PRIVATE -Wno-character-conversion)
    endif()
endif()

# Almost every warning, except those that only restrict style (like
# -Wtemplates), or are unusable with googletest (like -Wglobal-constructors).
# Flags the compiler doesn't know are dropped.
set(EKNAN_GCC_WARNING_CANDIDATES
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Warith-conversion -Wdouble-promotion
    -Wfloat-equal -Wold-style-cast -Wuseless-cast
    -Wzero-as-null-pointer-constant
    -Wshadow -Wcast-qual -Wcast-align=strict
    -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2 -Wformat-signedness
    -Wswitch-default -Wswitch-enum
    -Wlogical-op -Wduplicated-cond -Wduplicated-branches
    -Wnull-dereference -Wshift-overflow=2
    -Wredundant-decls -Wmissing-declarations -Wundef
    -Wunused -Wunused-macros -Wunused-const-variable=2
    -Wimplicit-fallthrough=5 -Wextra-semi
    -Wnon-virtual-dtor -Woverloaded-virtual=2 -Wsuggest-override
    -Wctor-dtor-privacy -Wnoexcept -Wstrict-null-sentinel -Wsign-promo
    -Wdeprecated-copy-dtor -Wcatch-value=3 -Wplacement-new=2
    -Wconditionally-supported -Wcomma-subscript -Wvolatile
    -Wmismatched-tags -Wredundant-tags -Wctad-maybe-unsupported
    -Winvalid-imported-macros -Wnrvo
    -Wdisabled-optimization -Wtrampolines -Walloca -Wvla -Wdate-time
    -Wmissing-include-dirs -Wpointer-arith -Wwrite-strings -Winit-self
    -Wstringop-overflow=4 -Warray-bounds=2 -Wattribute-alias=2
    -Wvector-operation-performance -Wbidi-chars=any
)
set(EKNAN_CLANG_WARNING_CANDIDATES
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Wshorten-64-to-32
    -Wimplicit-int-conversion -Wimplicit-float-conversion
    -Wdouble-promotion -Wfloat-equal
    -Wold-style-cast -Wzero-as-null-pointer-constant
    -Wshadow-all -Wcast-qual -Wcast-align -Wcast-function-type-strict
    -Wformat=2 -Wformat-pedantic -Wformat-type-confusion
    -Wswitch-default -Wswitch-enum -Wcovered-switch-default
    -Wassign-enum -Wbitfield-enum-conversion -Wsigned-enum-bitfield
    -Wanon-enum-enum-conversion
    -Wnull-dereference -Wshift-sign-overflow
    -Wmissing-prototypes -Wmissing-variable-declarations -Wundef
    -Wunused -Wunused-macros -Wunused-template -Wunused-member-function
    -Wunused-exception-parameter -Wused-but-marked-unused
    -Wimplicit-fallthrough -Wextra-semi -Wextra-semi-stmt -Wnewline-eof
    -Wcomma -Wredundant-parens -Wempty-init-stmt
    -Wnon-virtual-dtor -Woverloaded-virtual -Wsuggest-override
    -Wsuggest-destructor-override -Winconsistent-missing-destructor-override
    -Wdeprecated -Wdeprecated-dynamic-exception-spec
    -Wunreachable-code-aggressive -Wloop-analysis
    -Wconditional-uninitialized -Wuninitialized-const-reference
    -Widiomatic-parentheses -Wmissing-noreturn -Wheader-hygiene
    -Wreserved-identifier -Wreserved-macro-identifier
    -Wdocumentation -Wdocumentation-pedantic -Wmove -Wself-assign
    -Wstring-conversion -Wtautological-compare
    -Wtautological-constant-in-range-compare -Wtautological-overlap-compare
    -Wtautological-type-limit-compare -Wtautological-unsigned-zero-compare
    -Wtautological-value-range-compare -Wthread-safety
    -Wundefined-func-template -Wundefined-reinterpret-cast
    -Wvla -Wzero-length-array -Wgnu -Wlanguage-extension-token
    -Watomic-implicit-seq-cst -Wclass-varargs -Wdtor-name
    -Wexpansion-to-defined -Wmain -Wnon-gcc -Wdate-time
    -Wunreachable-code-loop-increment -Wweak-vtables
)

set(EKNAN_WARNING_FLAGS)
if(MSVC)
    set(EKNAN_WARNING_FLAGS /W4)
else()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(candidates ${EKNAN_GCC_WARNING_CANDIDATES})
    else()
        set(candidates ${EKNAN_CLANG_WARNING_CANDIDATES})
    endif()
    foreach(flag IN LISTS candidates)
        string(MAKE_C_IDENTIFIER "EKNAN_HAS_FLAG${flag}" flag_variable)
        check_cxx_compiler_flag(${flag} ${flag_variable})
        if(${flag_variable})
            list(APPEND EKNAN_WARNING_FLAGS ${flag})
        endif()
    endforeach()
endif()

if(EKNAN_WERROR)
    if(MSVC)
        list(APPEND EKNAN_WARNING_FLAGS /WX)
    else()
        list(APPEND EKNAN_WARNING_FLAGS -Werror)
    endif()
endif()

function(eknan_add_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE
        eknan::eknan
        GTest::gtest
        GTest::gtest_main
        ${EKNAN_TEST_COMPILER_RT_LIBRARY}
    )
    target_compile_options(${test_name} PRIVATE ${EKNAN_WARNING_FLAGS})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

eknan_add_test(eknan_test test.cpp stdlib_interface.cpp)

# Runs the same tests with uint128_polyfill as the 128-bit payload type
eknan_add_test(eknan_test_polyfill test.cpp stdlib_interface.cpp)
target_compile_definitions(eknan_test_polyfill PRIVATE EKNAN_HAS_INT128=0)

# eknan's behavior must not change with -ffast-math
eknan_add_test(eknan_test_fast_math test.cpp stdlib_interface.cpp)
if(MSVC)
    target_compile_options(eknan_test_fast_math PRIVATE /fp:fast)
else()
    target_compile_options(eknan_test_fast_math PRIVATE -ffast-math)
endif()
