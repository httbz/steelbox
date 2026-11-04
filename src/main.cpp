#include <steelbox/cli/init_command.hpp>
#include <steelbox/cli/secure_prompt.hpp>
#include <steelbox/crypto/password_generator.hpp>
#include <steelbox/models/vault.hpp>
#include <steelbox/storage/vault_serializer.hpp>
#include <steelbox/storage/vault_storage.hpp>

#include <sodium.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#else
#include <unistd.h>
#endif

#ifndef STEELBOX_VERSION
#define STEELBOX_VERSION "0.1.0"
#endif

namespace {

struct SecretWiper {
    std::string& value;

    ~SecretWiper() {
        if (!value.empty()) {
            sodium_memzero(value.data(), value.size());
        }
    }
};

void clear_secret(std::string& value) {
    if (!value.empty()) {
        sodium_memzero(value.data(), value.size());
    }
    value.clear();
}

std::filesystem::path executable_path() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(1024);
    while (buffer.size() <= 32768) {
        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );
        if (length == 0) {
            throw std::runtime_error("Nao foi possivel localizar o executavel.");
        }
        if (length < buffer.size()) {
            return std::filesystem::path(
                std::wstring(buffer.data(), static_cast<std::size_t>(length))
            );
        }
        buffer.resize(buffer.size() * 2);
    }
    throw std::runtime_error("O caminho do executavel e longo demais.");
#elif defined(__APPLE__)
    std::uint32_t buffer_size = 0;
    _NSGetExecutablePath(nullptr, &buffer_size);
    std::vector<char> buffer(buffer_size);
    if (_NSGetExecutablePath(buffer.data(), &buffer_size) != 0) {
        throw std::runtime_error("Nao foi possivel localizar o executavel.");
    }
    return std::filesystem::canonical(buffer.data());
#else
    std::vector<char> buffer(1024);
    while (buffer.size() <= 1024 * 1024) {
        const ssize_t length = readlink(
            "/proc/self/exe",
            buffer.data(),
            buffer.size()
        );
        if (length < 0) {
            throw std::runtime_error("Nao foi possivel localizar o executavel.");
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            return std::filesystem::path(
                std::string(buffer.data(), static_cast<std::size_t>(length))
            );
        }
        buffer.resize(buffer.size() * 2);
    }
    throw std::runtime_error("O caminho do executavel e longo demais.");
#endif
}

std::filesystem::path default_vault_path() {
    return executable_path().parent_path() / "steelbox.vault";
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string value = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(value.data()),
        value.size()
    );
}

void print_help(std::ostream& output) {
    output << "Steelbox " << STEELBOX_VERSION << "\n"
           << "Uso: steelbox <comando> [opcoes]\n\n"
           << "Comandos disponiveis:\n"
           << "  init [--vault <caminho>]  Criar um cofre criptografado\n"
           << "  add [--vault <caminho>]   Adicionar uma entrada\n"
           << "  get <id> [--reveal]       Exibir uma entrada\n"
           << "  ls [--query <termo>]      Listar entradas\n"
           << "  edit <id>                 Editar uma entrada\n"
           << "  rm <id> [--yes]           Remover uma entrada\n"
           << "  gen [--length <n>]        Gerar uma senha\n"
           << "  change-master             Alterar a senha mestra\n"
           << "  export <id|--all>         Exportar entradas criptografadas\n"
           << "  import <arquivo>          Importar entradas criptografadas\n"
           << "  help [comando]            Mostrar ajuda\n"
           << "  version                   Mostrar a versao\n";
}

bool read_line(const std::string& prompt, std::string& value) {
    std::cout << prompt << std::flush;
    return static_cast<bool>(std::getline(std::cin, value));
}

bool read_master(std::string& password, bool confirmation) {
    std::string error;
    if (!steelbox::cli::read_hidden_input("Senha mestra: ", password, error)) {
        std::cerr << "Erro: " << error << "\n";
        return false;
    }
    if (!confirmation) {
        return true;
    }
    std::string second;
    const SecretWiper second_wiper{second};
    if (!steelbox::cli::read_hidden_input(
            "Confirme a senha mestra: ", second, error)) {
        std::cerr << "Erro: " << error << "\n";
        return false;
    }
    const bool matches = password.size() == second.size() &&
        sodium_memcmp(password.data(), second.data(), password.size()) == 0;
    clear_secret(second);
    if (!matches) {
        clear_secret(password);
        std::cerr << "Erro: as senhas nao conferem.\n";
        return false;
    }
    return !password.empty();
}

std::filesystem::path parse_vault_path(
    const std::vector<std::string>& arguments,
    std::size_t& index
) {
    std::filesystem::path path = default_vault_path();
    while (index < arguments.size()) {
        if (arguments[index] != "--vault" || index + 1 >= arguments.size()) {
            throw std::runtime_error("Opcao invalida para o cofre.");
        }
        path = std::filesystem::u8path(arguments[++index]);
        if (path.is_relative()) {
            path = std::filesystem::absolute(path);
        }
        ++index;
    }
    return path;
}

bool load_for_command(
    const std::filesystem::path& path,
    std::string& master,
    steelbox::Vault& vault
) {
    std::error_code filesystem_error;
    if (!std::filesystem::exists(path, filesystem_error)) {
        std::cerr << "Erro: o arquivo do cofre nao existe: "
                  << path_to_utf8(path) << ".\n";
        return false;
    }
    if (filesystem_error ||
        !std::filesystem::is_regular_file(path, filesystem_error) ||
        filesystem_error) {
        std::cerr << "Erro: o caminho do cofre nao e um arquivo regular.\n";
        return false;
    }
    const SecretWiper wiper{master};
    if (!read_master(master, false)) {
        return false;
    }
    const auto result = steelbox::load_vault(vault, path, master);
    if (!result.succeeded) {
        std::cerr << "Erro: " << result.message << "\n";
        return false;
    }
    return true;
}

void print_entry(const steelbox::PasswordEntry& entry, bool reveal) {
    std::cout << "ID: " << entry.id << "\n"
              << "Titulo: " << entry.title << "\n"
              << "Usuario: " << entry.username << "\n"
              << "URL: " << entry.url << "\n"
              << "Dominio: " << entry.normalized_domain << "\n"
              << "Tags: ";
    for (std::size_t i = 0; i < entry.tags.size(); ++i) {
        if (i != 0) std::cout << ", ";
        std::cout << entry.tags[i];
    }
    std::cout << "\nNotas: " << entry.notes << "\n";
    if (reveal) {
        std::cout << "Senha: " << entry.password.view() << "\n";
    } else {
        std::cout << "Senha: ********\n";
    }
}

int add_command(const std::vector<std::string>& arguments) {
    std::size_t index = 1;
    const auto path = parse_vault_path(arguments, index);
    if (index != arguments.size()) throw std::runtime_error("Opcoes invalidas para add.");
    std::string master;
    steelbox::Vault vault;
    if (!load_for_command(path, master, vault)) return 1;
    steelbox::PasswordEntry entry;
    if (!read_line("Titulo (opcional): ", entry.title) ||
        !read_line("Usuario: ", entry.username) ||
        !read_line("URL: ", entry.url)) return 1;
    std::string password;
    const SecretWiper password_wiper{password};
    if (!read_line("Gerar senha automaticamente? [s/N]: ", password)) return 1;
    if (password == "s" || password == "S") {
        const auto generated = steelbox::generate_password();
        if (!generated.succeeded) {
            std::cerr << "Erro: " << generated.message << "\n";
            return 1;
        }
        entry.password = generated.password;
        std::cout << "Senha gerada: " << generated.password << "\n";
    } else {
        clear_secret(password);
        std::string prompt_error;
        if (!steelbox::cli::read_hidden_input("Senha: ", password, prompt_error)) {
            std::cerr << "Erro: " << prompt_error << "\n";
            return 1;
        }
        entry.password = password;
    }
    if (!read_line("Notas (opcional): ", entry.notes)) return 1;
    std::string tags;
    if (!read_line("Tags separadas por virgula (opcional): ", tags)) return 1;
    std::size_t start = 0;
    while (start < tags.size()) {
        const auto comma = tags.find(',', start);
        const auto end = comma == std::string::npos ? tags.size() : comma;
        std::string tag = tags.substr(start, end - start);
        if (!tag.empty()) entry.tags.push_back(tag);
        start = comma == std::string::npos ? tags.size() : comma + 1;
    }
    const auto added = vault.add_entry(std::move(entry));
    if (!added.succeeded()) {
        std::cerr << "Erro: " << added.message << "\n";
        return 1;
    }
    const auto saved = steelbox::save_vault(vault, path, master);
    if (!saved.succeeded) {
        std::cerr << "Erro: " << saved.message << "\n";
        return 1;
    }
    std::cout << "Entrada criada. ID: " << added.entry_id << "\n";
    return 0;
}

int load_and_find(
    const std::vector<std::string>& arguments,
    bool reveal
) {
    if (arguments.size() < 2) throw std::runtime_error("Informe o ID da entrada.");
    std::filesystem::path path = default_vault_path();
    for (std::size_t index = 2; index < arguments.size(); ++index) {
        if (arguments[index] == "--reveal") continue;
        if (arguments[index] == "--vault" && index + 1 < arguments.size()) {
            path = std::filesystem::u8path(arguments[++index]);
            continue;
        }
        throw std::runtime_error("Opcoes invalidas para get.");
    }
    std::string master;
    steelbox::Vault vault;
    if (!load_for_command(path, master, vault)) return 1;
    const auto* entry = vault.find_entry(arguments[1]);
    if (entry == nullptr) {
        std::cerr << "Erro: entrada nao encontrada.\n";
        return 1;
    }
    print_entry(*entry, reveal);
    return 0;
}

int list_command(const std::vector<std::string>& arguments) {
    std::string query;
    std::string tag;
    std::size_t index = 1;
    std::filesystem::path path = default_vault_path();
    while (index < arguments.size()) {
        if (arguments[index] == "--query" && index + 1 < arguments.size()) query = arguments[++index];
        else if (arguments[index] == "--tag" && index + 1 < arguments.size()) tag = arguments[++index];
        else if (arguments[index] == "--vault" && index + 1 < arguments.size()) {
            path = std::filesystem::u8path(arguments[++index]);
            if (path.is_relative()) path = std::filesystem::absolute(path);
        }
        else throw std::runtime_error("Opcoes invalidas para ls.");
        ++index;
    }
    std::string master;
    steelbox::Vault vault;
    if (!load_for_command(path, master, vault)) return 1;
    auto entries = query.empty() ? std::vector<const steelbox::PasswordEntry*>{} : vault.search_entries(query);
    if (query.empty() && vault.entries() != nullptr) {
        for (const auto& entry : *vault.entries()) entries.push_back(&entry);
    }
    for (const auto* entry : entries) {
        if (!tag.empty() &&
            std::find(entry->tags.begin(), entry->tags.end(), tag) ==
                entry->tags.end()) {
            continue;
        }
        std::cout << entry->id << " | " << entry->title << " | "
                  << entry->username << " | " << entry->normalized_domain
                  << "\n";
    }
    return 0;
}

int edit_command(const std::vector<std::string>& arguments) {
        if (arguments.size() < 2) throw std::runtime_error("Informe o ID da entrada.");
        std::size_t index = 2;
        const auto path = parse_vault_path(arguments, index);
        if (index != arguments.size()) throw std::runtime_error("Opcoes invalidas para edit.");
        std::string master;
        steelbox::Vault vault;
        if (!load_for_command(path, master, vault)) return 1;
        auto* entry = vault.find_entry(arguments[1]);
        if (entry == nullptr) {
            std::cerr << "Erro: entrada nao encontrada.\n";
            return 1;
        }
        steelbox::PasswordEntry updated = *entry;
        std::string value;
        if (!read_line("Titulo [" + updated.title + "]: ", value)) return 1;
        if (!value.empty()) updated.title = value;
        if (!read_line("Usuario [" + updated.username + "]: ", value)) return 1;
        if (!value.empty()) updated.username = value;
        if (!read_line("URL [" + updated.url + "]: ", value)) return 1;
        if (!value.empty()) updated.url = value;
        std::string new_password;
        const SecretWiper password_wiper{new_password};
        std::string prompt_error;
        if (!steelbox::cli::read_hidden_input(
                "Nova senha (Enter para manter): ", new_password, prompt_error)) {
            std::cerr << "Erro: " << prompt_error << "\n";
            return 1;
        }
        if (!new_password.empty()) updated.password = new_password;
        if (!read_line("Notas [" + updated.notes + "]: ", value)) return 1;
        if (!value.empty()) updated.notes = value;
        const auto result = vault.update_entry(updated);
        if (!result.succeeded()) {
            std::cerr << "Erro: " << result.message << "\n";
            return 1;
        }
        const auto saved = steelbox::save_vault(vault, path, master);
        if (!saved.succeeded) {
            std::cerr << "Erro: " << saved.message << "\n";
            return 1;
        }
        std::cout << "Entrada atualizada.\n";
        return 0;
    }

int remove_command(const std::vector<std::string>& arguments) {
        if (arguments.size() < 2) throw std::runtime_error("Informe o ID da entrada.");
        std::size_t index = 2;
        bool confirmed = false;
        std::filesystem::path path = default_vault_path();
        while (index < arguments.size()) {
            if (arguments[index] == "--yes") {
                confirmed = true;
            } else if (arguments[index] == "--vault" && index + 1 < arguments.size()) {
                path = std::filesystem::u8path(arguments[++index]);
            } else {
                throw std::runtime_error("Opcoes invalidas para rm.");
            }
            ++index;
        }
        if (!confirmed) {
            std::string answer;
            if (!read_line("Digite 'sim' para confirmar a exclusao: ", answer) ||
                answer != "sim") {
                std::cout << "Operacao cancelada.\n";
                return 0;
            }
        }
        std::string master;
        steelbox::Vault vault;
        if (!load_for_command(path, master, vault)) return 1;
        const auto result = vault.remove_entry(arguments[1]);
        if (!result.succeeded()) {
            std::cerr << "Erro: " << result.message << "\n";
            return 1;
        }
        const auto saved = steelbox::save_vault(vault, path, master);
        if (!saved.succeeded) {
            std::cerr << "Erro: " << saved.message << "\n";
            return 1;
        }
        std::cout << "Entrada removida.\n";
        return 0;
    }

int change_master_command(const std::vector<std::string>& arguments) {
        std::size_t index = 1;
        const auto path = parse_vault_path(arguments, index);
        std::string old_password;
        steelbox::Vault vault;
        if (!load_for_command(path, old_password, vault)) return 1;
        std::string new_password;
        const SecretWiper old_wiper{old_password};
        const SecretWiper new_wiper{new_password};
        if (!read_master(new_password, true)) return 1;
        const auto saved = steelbox::save_vault(vault, path, new_password);
        if (!saved.succeeded) {
            std::cerr << "Erro: " << saved.message << "\n";
            return 1;
        }
        std::cout << "Senha mestra alterada.\n";
        return 0;
    }

int export_command(const std::vector<std::string>& arguments) {
        if (arguments.size() < 4) throw std::runtime_error(
            "Uso: export <id|--all> --output <arquivo>");
        const bool all = arguments[1] == "--all";
        std::string output;
        std::filesystem::path source_path = default_vault_path();
        std::size_t index = 2;
        while (index < arguments.size()) {
            if (arguments[index] == "--output" && index + 1 < arguments.size()) {
                output = arguments[++index];
            } else if (arguments[index] == "--vault" && index + 1 < arguments.size()) {
                source_path = std::filesystem::u8path(arguments[++index]);
            } else {
                throw std::runtime_error("Opcoes invalidas para export.");
            }
            ++index;
        }
        if (output.empty()) throw std::runtime_error("Informe --output.");
        const auto output_path = std::filesystem::u8path(output);
        if (std::filesystem::exists(output_path)) {
            std::cerr << "Erro: o arquivo de exportacao ja existe.\n";
            return 1;
        }
        std::string master;
        steelbox::Vault source;
        if (!load_for_command(source_path, master, source)) return 1;
        steelbox::Vault selected;
        selected.unlock();
        if (all) {
            for (const auto& entry : *source.entries()) selected.restore_entry(entry);
        } else {
            const auto* entry = source.find_entry(arguments[1]);
            if (entry == nullptr) {
                std::cerr << "Erro: entrada nao encontrada.\n";
                return 1;
            }
            selected.restore_entry(*entry);
        }
        std::string export_password;
        const SecretWiper export_wiper{export_password};
        if (!read_master(export_password, true)) return 1;
        const auto result = steelbox::save_vault(selected, output_path, export_password);
        if (!result.succeeded) {
            std::cerr << "Erro: " << result.message << "\n";
            return 1;
        }
        std::cout << "Exportacao criada em " << output << ".\n";
        return 0;
    }

int import_command(const std::vector<std::string>& arguments) {
        if (arguments.size() != 2) throw std::runtime_error("Uso: import <arquivo>.");
        std::string export_password;
        const SecretWiper export_wiper{export_password};
        if (!read_master(export_password, false)) return 1;
        steelbox::Vault imported;
        const auto imported_result = steelbox::load_vault(
            imported, std::filesystem::u8path(arguments[1]), export_password);
        if (!imported_result.succeeded) {
            std::cerr << "Erro: " << imported_result.message << "\n";
            return 1;
        }
        std::string master;
        steelbox::Vault target;
        if (!load_for_command(default_vault_path(), master, target)) return 1;
        for (const auto& entry : *imported.entries()) {
            const auto result = target.restore_entry(entry);
            if (!result.succeeded()) {
                std::cerr << "Erro: importacao interrompida: " << result.message << "\n";
                return 1;
            }
        }
        const auto saved = steelbox::save_vault(target, default_vault_path(), master);
        if (!saved.succeeded) {
            std::cerr << "Erro: " << saved.message << "\n";
            return 1;
        }
        std::cout << "Importacao concluida.\n";
        return 0;
    }
int generate_command(const std::vector<std::string>& arguments) {
    steelbox::PasswordGeneratorOptions options;
    for (std::size_t i = 1; i < arguments.size(); ++i) {
        if (arguments[i] != "--length" || i + 1 >= arguments.size()) {
            throw std::runtime_error("Use gen --length <numero>.");
        }
        try {
            std::size_t parsed_characters = 0;
            const auto length = std::stoul(
                arguments[++i],
                &parsed_characters
            );
            if (parsed_characters != arguments[i].size()) {
                throw std::invalid_argument("comprimento");
            }
            options.length = static_cast<std::size_t>(length);
        } catch (const std::exception&) {
            throw std::runtime_error(
                "O comprimento deve ser um numero inteiro."
            );
        }
    }
    const auto result = steelbox::generate_password(options);
    if (!result.succeeded) {
        std::cerr << "Erro: " << result.message << "\n";
        return 1;
    }
    std::cout << result.password << "\n";
    return 0;
}

void print_init_help(std::ostream& output) {
    output << "Uso: steelbox init [--vault <caminho>]\n\n"
           << "Cria um cofre vazio e criptografado. O caminho padrao e\n"
           << "steelbox.vault na mesma pasta do executavel. Um cofre existente\n"
           << "nunca e sobrescrito. A senha mestra e solicitada sem exibicao.\n";
}

void print_command_help(
    const std::string& command,
    std::ostream& output
) {
    if (command == "init") {
        print_init_help(output);
    } else if (command == "add") {
        output << "Uso: steelbox add [--vault <caminho>]\n";
    } else if (command == "get") {
        output << "Uso: steelbox get <id> [--reveal] [--vault <caminho>]\n";
    } else if (command == "ls") {
        output << "Uso: steelbox ls [--query <termo>] [--tag <tag>] "
                   "[--vault <caminho>]\n";
    } else if (command == "edit") {
        output << "Uso: steelbox edit <id> [--vault <caminho>]\n";
    } else if (command == "rm") {
        output << "Uso: steelbox rm <id> [--yes] [--vault <caminho>]\n";
    } else if (command == "gen") {
        output << "Uso: steelbox gen [--length <numero>]\n";
    } else if (command == "change-master") {
        output << "Uso: steelbox change-master [--vault <caminho>]\n";
    } else if (command == "export") {
        output << "Uso: steelbox export <id|--all> --output <arquivo> "
                   "[--vault <caminho>]\n";
    } else if (command == "import") {
        output << "Uso: steelbox import <arquivo>\n";
    } else if (command == "version") {
        output << "Uso: steelbox version\n";
    } else {
        output << "Erro: ajuda indisponivel para esse comando.\n";
    }
}

bool destination_is_available(
    const std::filesystem::path& path,
    std::string& error
) {
    std::error_code filesystem_error;
    if (std::filesystem::exists(path, filesystem_error)) {
        error = "Ja existe um cofre em " + path_to_utf8(path) +
            "; nenhum dado foi alterado.";
        return false;
    }
    if (filesystem_error) {
        error = "Nao foi possivel verificar o caminho " + path_to_utf8(path) +
            ".";
        return false;
    }

    std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        parent = std::filesystem::current_path(filesystem_error);
        if (filesystem_error) {
            error = "Nao foi possivel localizar o diretorio atual.";
            return false;
        }
    }
    if (!std::filesystem::is_directory(parent, filesystem_error) ||
        filesystem_error) {
        error = "O diretorio de destino nao existe ou nao e acessivel: " +
            path_to_utf8(parent) + ".";
        return false;
    }
    return true;
}

int initialize_command(const std::vector<std::string>& arguments) {
    std::filesystem::path vault_path;
    try {
        vault_path = default_vault_path();
        if (arguments.size() == 2 && arguments[1] == "--help") {
            print_init_help(std::cout);
            return 0;
        }
        if (arguments.size() != 1) {
            if (arguments.size() != 3 || arguments[1] != "--vault" ||
                arguments[2].empty()) {
                std::cerr << "Erro: opcao ou argumento invalido para init.\n";
                print_init_help(std::cerr);
                return 2;
            }
            vault_path = std::filesystem::u8path(arguments[2]);
            if (vault_path.is_relative()) {
                vault_path = std::filesystem::absolute(vault_path);
            }
        }
    } catch (const std::filesystem::filesystem_error& error) {
        std::cerr << "Erro ao resolver o caminho do cofre: "
                  << error.what() << "\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Erro: " << error.what() << "\n";
        return 1;
    }

    std::string error;
    if (!destination_is_available(vault_path, error)) {
        std::cerr << "Erro: " << error << "\n";
        return 1;
    }

    std::cout << "Arquivo do cofre: " << path_to_utf8(vault_path) << "\n"
              << "A senha mestra nao pode ser recuperada; use uma frase longa e unica.\n";

    std::string master_password;
    std::string confirmation;
    const SecretWiper master_password_wiper{master_password};
    const SecretWiper confirmation_wiper{confirmation};
    bool passwords_match = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string prompt_error;
        if (!steelbox::cli::read_hidden_input(
                "Senha mestra: ", master_password, prompt_error
            ) ||
            !steelbox::cli::read_hidden_input(
                "Confirme a senha mestra: ", confirmation, prompt_error
            )) {
            std::cerr << "Erro: " << prompt_error << "\n";
            return 1;
        }
        if (master_password.empty()) {
            std::cerr << "A senha mestra nao pode ser vazia.\n";
            clear_secret(master_password);
            clear_secret(confirmation);
            continue;
        }
        if (master_password.size() == confirmation.size() &&
            sodium_memcmp(
                master_password.data(),
                confirmation.data(),
                master_password.size()
            ) == 0) {
            passwords_match = true;
            break;
        }

        std::cerr << "As senhas nao conferem. Tente novamente.\n";
        clear_secret(master_password);
        clear_secret(confirmation);
    }
    if (!passwords_match) {
        std::cerr << "Erro: nao foi possivel confirmar a senha mestra.\n";
        return 1;
    }

    std::cout << "Criando e cifrando o cofre...\n" << std::flush;
    const steelbox::StorageResult result = steelbox::cli::initialize_vault(
        vault_path,
        master_password
    );
    if (!result.succeeded) {
        std::cerr << "Erro: " << result.message << "\n";
        return 1;
    }

    std::cout << "Cofre criado e protegido em " << path_to_utf8(vault_path)
              << ".\n";
    return 0;
}

int run_cli(const std::vector<std::string>& arguments) {
    if (arguments.empty() || arguments[0] == "--help" ||
        arguments[0] == "-h") {
        print_help(std::cout);
        return 0;
    }
    if (arguments[0] == "version" || arguments[0] == "--version") {
        std::cout << "Steelbox " << STEELBOX_VERSION << "\n";
        return 0;
    }
    if (arguments[0] == "help") {
        if (arguments.size() == 1) {
            print_help(std::cout);
            return 0;
        }
        if (arguments.size() == 2) {
            print_command_help(arguments[1], std::cout);
            if (arguments[1] == "init" || arguments[1] == "add" ||
                arguments[1] == "get" || arguments[1] == "ls" ||
                arguments[1] == "edit" || arguments[1] == "rm" ||
                arguments[1] == "gen" || arguments[1] == "change-master" ||
                arguments[1] == "export" || arguments[1] == "import" ||
                arguments[1] == "version") {
                return 0;
            }
            std::cerr << "Use 'steelbox help' para ver os comandos.\n";
            return 2;
        }
        std::cerr << "Erro: ajuda indisponivel para esse comando.\n";
        print_help(std::cerr);
        return 2;
    }
    if (arguments[0] == "init") {
        return initialize_command(arguments);
    }
    if (arguments[0] == "add") return add_command(arguments);
    if (arguments[0] == "get") {
        return load_and_find(arguments,
            std::find(arguments.begin(), arguments.end(), "--reveal") !=
                arguments.end());
    }
    if (arguments[0] == "ls") return list_command(arguments);
    if (arguments[0] == "edit") return edit_command(arguments);
    if (arguments[0] == "rm") return remove_command(arguments);
    if (arguments[0] == "gen") return generate_command(arguments);
    if (arguments[0] == "change-master") {
        return change_master_command(arguments);
    }
    if (arguments[0] == "export") return export_command(arguments);
    if (arguments[0] == "import") return import_command(arguments);

    std::cerr << "Erro: comando desconhecido. Use 'steelbox help'.\n";
    print_help(std::cerr);
    return 2;
}

#if defined(_WIN32)

std::string wide_to_utf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (size <= 0) {
        throw std::runtime_error("Argumento contem caracteres invalidos.");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            size,
            nullptr,
            nullptr
        ) != size) {
        throw std::runtime_error("Nao foi possivel converter um argumento.");
    }
    return result;
}

#endif

}

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    try {
        std::vector<std::string> arguments;
        arguments.reserve(static_cast<std::size_t>(argc - 1));
        for (int index = 1; index < argc; ++index) {
            arguments.push_back(wide_to_utf8(argv[index]));
        }
        return run_cli(arguments);
    } catch (const std::exception& error) {
        std::cerr << "Erro: " << error.what() << "\n";
        return 1;
    }
}
#else
int main(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc - 1));
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    try {
        return run_cli(arguments);
    } catch (const std::exception& error) {
        std::cerr << "Erro: " << error.what() << "\n";
        return 1;
    }
}
#endif