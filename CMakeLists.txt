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

find_package(unofficial-sodium CONFIG REQUIRED)

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
        unofficial-sodium::sodium
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