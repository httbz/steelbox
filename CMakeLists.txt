cmake_minimum_required(VERSION 3.25)

project(steelbox
    VERSION 0.1.0
    LANGUAGES CXX
)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(steelbox
    src/main.cpp
    src/models/password_entry.cpp
    src/models/url.cpp
    src/models/vault.cpp
)

target_include_directories(steelbox
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(steelbox_tests
    tests/core_tests.cpp
    src/models/password_entry.cpp
    src/models/url.cpp
    src/models/vault.cpp
)

target_include_directories(steelbox_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME steelbox_core_tests COMMAND steelbox_tests)