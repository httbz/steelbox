#include <steelbox/cli/init_command.hpp>

#include <steelbox/models/vault.hpp>

#include <system_error>

namespace steelbox::cli {

StorageResult initialize_vault(
    const std::filesystem::path& vault_path,
    const std::string& master_password
) {
    if (master_password.empty()) {
        return {false, "A senha mestra nao pode ser vazia."};
    }

    std::error_code error;
    const bool already_exists = std::filesystem::exists(vault_path, error);
    if (error) {
        return {false, "Nao foi possivel verificar o caminho do cofre."};
    }
    if (already_exists) {
        return {false, "Ja existe um cofre nesse caminho; nenhum dado foi alterado."};
    }

    Vault vault;
    const VaultResult unlock_result = vault.unlock(master_password);
    if (!unlock_result.succeeded()) {
        return {false, unlock_result.message};
    }

    const StorageResult save_result =
        save_vault(vault, vault_path, master_password);
    vault.lock();
    return save_result;
}

}