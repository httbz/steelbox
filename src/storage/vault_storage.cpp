#include <steelbox/storage/vault_storage.hpp>
#include <steelbox/storage/vault_serializer.hpp>
#include <steelbox/crypto/vault_crypto.hpp>

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace steelbox {

namespace {

constexpr std::array<char, 4> magic = {'S', 'T', 'B', 'X'};
constexpr std::uint32_t format_version = 2;

template <typename Integer>
void write_integer(std::ostream& output, Integer value) {
    output.write(
        reinterpret_cast<const char*>(&value),
        static_cast<std::streamsize>(sizeof(value))
    );
}

template <typename Integer>
bool read_integer(std::istream& input, Integer& value) {
    input.read(
        reinterpret_cast<char*>(&value),
        static_cast<std::streamsize>(sizeof(value))
    );
    return input.good();
}

StorageResult write_file(
    const std::filesystem::path& path,
    const std::string& payload
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return {false, "Nao foi possivel criar o arquivo do cofre."};
    }

    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    write_integer(output, format_version);
    write_integer(
        output,
        static_cast<std::uint64_t>(payload.size())
    );
    output.write(
        payload.data(),
        static_cast<std::streamsize>(payload.size())
    );

    if (!output.good()) {
        return {false, "Nao foi possivel gravar o arquivo do cofre."};
    }

    return {true, ""};
}

StorageResult replace_file(
    const std::filesystem::path& temporary_path,
    const std::filesystem::path& vault_path
) {
#if defined(_WIN32)
    if (!MoveFileExW(
            temporary_path.c_str(),
            vault_path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return {false, "Nao foi possivel substituir o cofre existente."};
    }
#else
    std::error_code error;
    std::filesystem::rename(temporary_path, vault_path, error);
    if (error) {
        return {false, "Nao foi possivel substituir o cofre existente."};
    }
#endif

    return {true, ""};
}

}

StorageResult save_vault(
    const Vault& vault,
    const std::filesystem::path& vault_path,
    const std::string& master_password
) {
    const SerializationResult serialized = serialize_vault(vault);
    if (!serialized.succeeded) {
        return {false, serialized.message};
    }

    const CryptoResult encrypted =
        encrypt_payload(serialized.data, master_password);
    if (!encrypted.succeeded) {
        return {false, encrypted.message};
    }

    const std::filesystem::path temporary_path = vault_path.string() + ".tmp";
    const std::filesystem::path backup_path = vault_path.string() + ".bak";

    const StorageResult temporary_result =
        write_file(temporary_path, encrypted.data);
    if (!temporary_result.succeeded) {
        return temporary_result;
    }

    std::error_code error;
    if (std::filesystem::exists(vault_path, error)) {
        std::filesystem::copy_file(
            vault_path,
            backup_path,
            std::filesystem::copy_options::overwrite_existing,
            error
        );
        if (error) {
            std::filesystem::remove(temporary_path);
            return {false, "Nao foi possivel criar o backup do cofre."};
        }
    }

    const StorageResult replace_result =
        replace_file(temporary_path, vault_path);
    if (!replace_result.succeeded) {
        std::filesystem::remove(temporary_path);
        return replace_result;
    }

    return {true, ""};
}

StorageResult load_vault(
    Vault& vault,
    const std::filesystem::path& vault_path,
    const std::string& master_password
) {
    std::ifstream input(vault_path, std::ios::binary);
    if (!input) {
        return {false, "Nao foi possivel abrir o arquivo do cofre."};
    }

    std::array<char, 4> file_magic{};
    input.read(file_magic.data(), static_cast<std::streamsize>(file_magic.size()));
    if (!input || file_magic != magic) {
        return {false, "Arquivo de cofre invalido."};
    }

    std::uint32_t version = 0;
    std::uint64_t payload_size = 0;
    if (!read_integer(input, version) ||
        !read_integer(input, payload_size) ||
        version != format_version ||
        payload_size > 64 * 1024 * 1024) {
        return {false, "Cabecalho do cofre invalido."};
    }

    std::string encrypted_payload(
        static_cast<std::size_t>(payload_size),
        '\0'
    );
    input.read(
        encrypted_payload.data(),
        static_cast<std::streamsize>(encrypted_payload.size())
    );
    if (!input) {
        return {false, "Payload do cofre incompleto."};
    }

    const CryptoResult decrypted =
        decrypt_payload(encrypted_payload, master_password);
    if (!decrypted.succeeded) {
        return {false, decrypted.message};
    }

    const SerializationResult deserialized =
        deserialize_vault(decrypted.data, vault);
    if (!deserialized.succeeded) {
        return {false, deserialized.message};
    }

    return {true, ""};
}

}
