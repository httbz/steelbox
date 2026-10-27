cmake_minimum_required(VERSION 3.25)

project(steelbox
    VERSION 0.1.0
    LANGUAGES CXX
)

enable_testing()

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(unofficial-sodium CONFIG QUIET)
if(TARGET unofficial-sodium::sodium)
    set(STEELBOX_SODIUM_TARGET unofficial-sodium::sodium)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    set(STEELBOX_SODIUM_TARGET PkgConfig::SODIUM)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(STEELBOX_CORE_SOURCES
    src/crypto/vault_crypto.cpp
    src/crypto/password_generator.cpp
    src/models/password_entry.cpp
    src/storage/vault_serializer.cpp
    src/storage/vault_storage.cpp
    src/models/url.cpp
    src/models/vault.cpp
)

add_library(steelbox_core STATIC ${STEELBOX_CORE_SOURCES})

target_include_directories(steelbox_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    steelbox_core
    PUBLIC
        nlohmann_json::nlohmann_json
        ${STEELBOX_SODIUM_TARGET}
)

add_executable(steelbox src/main.cpp)
target_link_libraries(steelbox PRIVATE steelbox_core)

add_executable(steelbox_tests
    tests/core_tests.cpp
)

target_include_directories(steelbox_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    steelbox_tests
    PRIVATE
        steelbox_core
)

add_test(NAME steelbox_core_tests COMMAND steelbox_tests)

option(STEELBOX_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(STEELBOX_BUILD_FUZZER "Build the parser fuzz target with libFuzzer" OFF)

if(STEELBOX_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "STEELBOX_ENABLE_SANITIZERS requires Clang or GCC")
    endif()

    foreach(target steelbox_core steelbox steelbox_tests)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

if(STEELBOX_BUILD_FUZZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "STEELBOX_BUILD_FUZZER requires Clang with libFuzzer")
    endif()

    add_executable(steelbox_parser_fuzz tests/parser_fuzz.cpp)
    target_link_libraries(steelbox_parser_fuzz PRIVATE steelbox_core)
    target_compile_options(steelbox_core PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
    )
    target_compile_options(steelbox_parser_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(steelbox_parser_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
    )

    foreach(target steelbox steelbox_tests)
        target_link_options(${target} PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined
        )
    endforeach()
endif()