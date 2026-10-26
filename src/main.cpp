#include <steelbox/models/vault.hpp>

#include <iostream>

int main() {
    steelbox::Vault vault;
    vault.unlock();

    steelbox::PasswordEntry github_entry{
        .id = "",
        .title = "GitHub",
        .username = "bernardo",
        .password = "securepassword123",
        .url = "https://github.com",
        .notes = "My GitHub account",
        .tags = {"work", "code"}
    };

    const steelbox::VaultResult add_result = vault.add_entry(github_entry);
    if (add_result.succeeded()) {
        std::cout << "Entrada adicionada com sucesso.\n";
        std::cout << "UUID gerado: " << add_result.entry_id << "\n";
    } else {
        std::cout << "Falha ao adicionar entrada: "
                  << add_result.message
                  << "\n";
    }

    steelbox::PasswordEntry duplicate_github_entry{
        .id = "",
        .title = "GitHub duplicado",
        .username = "outro.usuario",
        .password = "outra-senha-de-exemplo",
        .url = "https://github.com",
        .notes = "Esta entrada deve ser rejeitada",
        .tags = {"teste"}
    };

    if (vault.add_entry(duplicate_github_entry).succeeded()) {
        std::cout << "Segunda entrada adicionada com UUID proprio.\n";
    } else {
        std::cout << "Falha ao adicionar segunda entrada.\n";
    }

    if (const auto* entries = vault.entries()) {
        std::cout << "Entradas no cofre: "
                  << entries->size()
                  << "\n";
    }

    if (steelbox::PasswordEntry* entry = vault.find_entry(
            add_result.entry_id)) {
        std::cout << "Entrada encontrada: "
                  << entry->title
                  << "\n";

        std::cout << "Usuário: "
                  << entry->username
                  << "\n";
    }

    steelbox::PasswordEntry updated_github_entry{
        .id = add_result.entry_id,
        .title = "GitHub Updated",
        .username = "bernardo.updated",
        .password = "newsecurepassword456",
        .url = "https://github.com",
        .notes = "Updated GitHub account",
        .tags = {"work", "code", "updated"}
    };

    const steelbox::VaultResult update_result =
        vault.update_entry(updated_github_entry);

    if (update_result.succeeded()) {
        std::cout << "Entrada atualizada com sucesso. \n";
    } else {
        std::cout << "Falha ao atualizar a entrada: "
                  << update_result.message
                  << "\n";
    }

    if (steelbox::PasswordEntry* entry = vault.find_entry(
            add_result.entry_id)) {
        std::cout << "Dados apos atualizacao: \n";
        std::cout << "Título: " << entry->title << "\n";
        std::cout << "Usuário: " << entry->username << "\n";
        std::cout << "URL: " << entry->url << "\n";
    }

    if (vault.remove_entry(add_result.entry_id).succeeded()) {
        std::cout << "Entrada removida. \n";
    }

    if (const auto* entries = vault.entries()) {
        std::cout << "Entradas no cofre após remoção: "
                  << entries->size()
                  << "\n";
    }

    return 0;
}