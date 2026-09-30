cmake_minimum_required(VERSION 3.16)

file(STRINGS include/eknan/eknan.hpp EKNAN_VERSION_DEFINES
     REGEX "^#define EKNAN_VERSION_(MAJOR|MINOR|PATCH) [0-9]+$")
foreach(part MAJOR MINOR PATCH)
    string(REGEX MATCH "EKNAN_VERSION_${part} ([0-9]+)" _ "${EKNAN_VERSION_DEFINES}")
    set(EKNAN_VERSION_${part} "${CMAKE_MATCH_1}")
    if(EKNAN_VERSION_${part} STREQUAL "")
        message(FATAL_ERROR "Couldn't read EKNAN_VERSION_${part} from eknan.hpp")
    endif()
endforeach()

project(eknan
        VERSION ${EKNAN_VERSION_MAJOR}.${EKNAN_VERSION_MINOR}.${EKNAN_VERSION_PATCH}
        LANGUAGES CXX)

if(CMAKE_VERSION VERSION_LESS "3.21")
    get_directory_property(EKNAN_HAS_PARENT PARENT_DIRECTORY)
    if(NOT EKNAN_HAS_PARENT)
        set(EKNAN_IS_TOP_LEVEL TRUE)
    else()
        set(EKNAN_IS_TOP_LEVEL FALSE)
    endif()
else()
    set(EKNAN_IS_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})
endif()

option(BUILD_TESTING "Build tests" ${EKNAN_IS_TOP_LEVEL})
option(EKNAN_USE_EXTERNAL_GTEST "Use external GTest instead of FetchContent" OFF)
option(EKNAN_WERROR "Treat warnings in the tests as errors" OFF)
option(EKNAN_COMPILER_RT "clang-cl only: add compiler-rt as a linked library, to support _Float16 and __bf16" OFF)
option(EKNAN_INSTALL "Generate install targets" ${EKNAN_IS_TOP_LEVEL})

set(EKNAN_GTEST_VERSION "v1.15.2" CACHE STRING "GTest version to fetch")

# _Float16 and __bf16 are off by default with clang-cl, because their
# conversions need compiler-rt, which clang-cl doesn't link
set(EKNAN_COMPILER_RT_LIBRARY)
if(EKNAN_COMPILER_RT)
    if(NOT (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        message(FATAL_ERROR "EKNAN_COMPILER_RT requires clang-cl")
    endif()
    execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} /clang:--rtlib=compiler-rt /clang:-print-libgcc-file-name
            OUTPUT_VARIABLE EKNAN_COMPILER_RT_LIBRARY
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT EXISTS "${EKNAN_COMPILER_RT_LIBRARY}")
        message(FATAL_ERROR "compiler-rt not found: '${EKNAN_COMPILER_RT_LIBRARY}'")
    endif()
endif()

include(GNUInstallDirs)

add_library(eknan INTERFACE)
add_library(eknan::eknan ALIAS eknan)

if(EKNAN_COMPILER_RT)
    target_link_libraries(eknan INTERFACE
            $<BUILD_INTERFACE:${EKNAN_COMPILER_RT_LIBRARY}>
    )
    target_compile_definitions(eknan INTERFACE
            $<BUILD_INTERFACE:EKNAN_HAS_FLOAT16=1>
    )
    # On ARM64, Clang 17 and 18 crash when converting to __bf16
    if(NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64" OR
       CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19)
        target_compile_definitions(eknan INTERFACE
                $<BUILD_INTERFACE:EKNAN_HAS_BF16=1>
        )
    endif()
endif()

target_include_directories(eknan INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(eknan INTERFACE cxx_std_17)

if(EKNAN_INSTALL)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/eknan
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.hpp")

    install(FILES LICENSE
            DESTINATION ${CMAKE_INSTALL_DOCDIR})

    install(TARGETS eknan
            EXPORT eknanTargets
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # No dependencies, so the exported targets file can serve as the package config directly
    install(EXPORT eknanTargets
            FILE eknanConfig.cmake
            NAMESPACE eknan::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eknan)

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/eknanConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/eknanConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eknan)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
