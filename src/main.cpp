#include <steelbox/cli/init_command.hpp>
#include <steelbox/cli/secure_prompt.hpp>

#include <sodium.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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
           << "  help [comando]            Mostrar ajuda\n"
           << "  version                   Mostrar a versao\n";
}

void print_init_help(std::ostream& output) {
    output << "Uso: steelbox init [--vault <caminho>]\n\n"
           << "Cria um cofre vazio e criptografado. O caminho padrao e\n"
           << "steelbox.vault na mesma pasta do executavel. Um cofre existente\n"
           << "nunca e sobrescrito. A senha mestra e solicitada sem exibicao.\n";
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
        if (arguments.size() == 2 && arguments[1] == "init") {
            print_init_help(std::cout);
            return 0;
        }
        std::cerr << "Erro: ajuda indisponivel para esse comando.\n";
        print_help(std::cerr);
        return 2;
    }
    if (arguments[0] == "init") {
        return initialize_command(arguments);
    }

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