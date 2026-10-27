#include <steelbox/models/vault.hpp>
#include <steelbox/storage/vault_serializer.hpp>

#include <sodium.h>

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size
) {
    constexpr std::size_t maximum_input_size = 1024 * 1024;
    static const int sodium_initialization = sodium_init();
    if (size > maximum_input_size || sodium_initialization < 0) {
        return 0;
    }

    const std::string serialized(
        reinterpret_cast<const char*>(data),
        size
    );
    steelbox::Vault vault;
    static_cast<void>(steelbox::deserialize_vault(serialized, vault));
    return 0;
}
