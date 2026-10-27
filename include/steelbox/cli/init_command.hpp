#pragma once

#include <steelbox/storage/vault_storage.hpp>

#include <filesystem>
#include <string>

namespace steelbox::cli {

StorageResult initialize_vault(
    const std::filesystem::path& vault_path,
    const std::string& master_password
);

}