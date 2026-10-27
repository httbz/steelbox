#pragma once

#include <steelbox/models/vault.hpp>

#include <filesystem>
#include <string>

namespace steelbox {

struct StorageResult {
    bool succeeded = false;
    std::string message;
};

StorageResult save_vault(
    const Vault& vault,
    const std::filesystem::path& vault_path
);

StorageResult load_vault(
    Vault& vault,
    const std::filesystem::path& vault_path
);

}
