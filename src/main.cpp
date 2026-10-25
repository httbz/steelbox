#include <steelbox/models/vault.hpp>

#include <iostream>

int main() {
    steelbox::Vault vault;

    steelbox::PasswordEntry github_entry{
        .id = "1",
        .title = "GitHub",
        .username = "bernardo",
        .password = "securepassword123",
        .url = "https://github.com",
        .notes = "My GitHub account",
        .tags = {"work", "code"}
    };

    vault.add_entry(github_entry);

    std::cout << "Entradas no cofre: "
              << vault.entries().size()
              << "\n";

    if (steelbox::PasswordEntry* entry = vault.find_entry("1")) {
        std::cout << "Entrada encontrada: "
                  << entry->title
                  << "\n";

        std::cout << "Usuário: "
                  << entry->username
                  << "\n";
    }

    if (vault.remove_entry("1")) {
        std::cout << "Entrada removida. \n";
    }

    std::cout << "Entradas no cofre após remoção: "
              << vault.entries().size()
              << "\n";

    return 0;
}