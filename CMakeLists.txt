cmake_minimum_required(VERSION 3.25)

project(steelbox
    VERSION 0.1.0
    LANGUAGES CXX
)

enable_testing()

include(FetchContent)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "Disable FTXUI install rules" FORCE)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

find_package(unofficial-sodium CONFIG QUIET)
if(TARGET unofficial-sodium::sodium)
    set(STEELBOX_SODIUM_TARGET unofficial-sodium::sodium)
elseif(WIN32)
    message(FATAL_ERROR
        "libsodium nao foi encontrada no Windows. "
        "Configure com o toolchain do vcpkg: "
        "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Bernardo/vcpkg/scripts/buildsystems/vcpkg.cmake "
        "-DVCPKG_TARGET_TRIPLET=x64-windows"
    )
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

add_library(steelbox_cli_support STATIC
    src/cli/init_command.cpp
)
target_include_directories(steelbox_cli_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(steelbox_cli_support PUBLIC steelbox_core)

add_executable(
    steelbox
    src/main.cpp
    src/cli/secure_prompt.cpp
)
target_link_libraries(steelbox PRIVATE steelbox_cli_support)
target_compile_definitions(steelbox PRIVATE STEELBOX_VERSION="${PROJECT_VERSION}")

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
    steelbox_cli_support
)

add_test(NAME steelbox_core_tests COMMAND steelbox_tests)

add_executable(steelbox_tui
    src/tui/main.cpp
)
target_link_libraries(steelbox_tui
    PRIVATE
        steelbox_core
        ftxui::component
        ftxui::dom
        ftxui::screen
)

if(WIN32)
    foreach(target steelbox steelbox_tests steelbox_tui)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:${target}>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:${target}>;$<TARGET_FILE_DIR:${target}>>"
            COMMAND_EXPAND_LISTS
        )
    endforeach()
endif()

include(GNUInstallDirs)

install(TARGETS steelbox steelbox_tui
    RUNTIME DESTINATION .
)

install(FILES
    README.md
    DESTINATION .
)

install(DIRECTORY docs/
    DESTINATION docs
)

if(WIN32)
    install(FILES
        "$<TARGET_FILE_DIR:steelbox>/libsodium.dll"
        DESTINATION .
    )
endif()

set(CPACK_PACKAGE_NAME "steelbox")
set(CPACK_PACKAGE_VENDOR "httbz")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Gerenciador de senhas local e portatil")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
set(CPACK_GENERATOR "ZIP")
include(CPack)

option(STEELBOX_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(STEELBOX_BUILD_FUZZER "Build the parser fuzz target with libFuzzer" OFF)

if(STEELBOX_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "STEELBOX_ENABLE_SANITIZERS requires Clang or GCC")
    endif()

    foreach(target steelbox_core steelbox steelbox_tests steelbox_tui)
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