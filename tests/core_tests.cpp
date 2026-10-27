#include <steelbox/crypto/password_generator.hpp>
#include <steelbox/crypto/vault_crypto.hpp>
#include <steelbox/models/password_entry.hpp>
#include <steelbox/models/url.hpp>
#include <steelbox/models/vault.hpp>
#include <steelbox/storage/vault_serializer.hpp>
#include <steelbox/storage/vault_storage.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

steelbox::PasswordEntry valid_entry() {
    return {.id = "caller-id-must-be-ignored",
            .title = "GitHub",
            .username = "test-user",
            .password = "test-password",
            .url = "https://github.com",
            .notes = "Test entry",
            .tags = {"test"},
            .created_at = "",
            .updated_at = ""};
}

void test_entry_validation() {
    steelbox::PasswordEntry entry = valid_entry();

    expect(steelbox::validate_entry(entry).valid(),
           "A entrada completa deveria ser valida.");

    entry.url.clear();
    const auto missing_url = steelbox::validate_entry(entry);
    expect(missing_url.error == steelbox::EntryError::MissingUrl,
           "URL vazia deveria gerar MissingUrl.");

    entry = valid_entry();
    entry.username.clear();
    const auto missing_username = steelbox::validate_entry(entry);
    expect(missing_username.error == steelbox::EntryError::MissingUsername,
           "Usuario vazio deveria gerar MissingUsername.");

    entry = valid_entry();
    entry.password.clear();
    const auto missing_password = steelbox::validate_entry(entry);
    expect(missing_password.error == steelbox::EntryError::MissingPassword,
           "Senha vazia deveria gerar MissingPassword.");

    entry = valid_entry();
    entry.title.clear();
    expect(steelbox::validate_entry(entry).valid(),
           "Titulo vazio deveria ser permitido.");

    entry = valid_entry();
    entry.url = "github.com";
    const auto invalid_url = steelbox::validate_entry(entry);
    expect(invalid_url.error == steelbox::EntryError::InvalidUrl,
           "URL sem esquema deveria ser invalida.");
}

void test_url_normalization() {
    const auto github = steelbox::normalize_url(
        "https://WWW.GitHub.com/login?source=menu#form");
    expect(github.valid() && github.domain == "github.com",
           "Dominio HTTPS deveria ser normalizado.");

    const auto subdomain = steelbox::normalize_url("https://login.github.com");
    expect(subdomain.valid() && subdomain.domain == "login.github.com",
           "Subdominio nao deveria ser removido.");

    const auto local = steelbox::normalize_url("http://localhost:3000/login");
    expect(local.valid() && local.domain == "localhost",
           "Host local deveria ser aceito.");

    const auto ip = steelbox::normalize_url("http://192.168.0.10:8080");
    expect(ip.valid() && ip.domain == "192.168.0.10",
           "Endereco IP deveria ser aceito.");

    expect(steelbox::normalize_url("ftp://github.com").error ==
               steelbox::UrlError::UnsupportedScheme,
           "FTP deveria ser rejeitado.");
    expect(steelbox::normalize_url("javascript:alert(1)").error ==
               steelbox::UrlError::UnsupportedScheme,
           "Javascript URL deveria ser rejeitada.");
    expect(steelbox::normalize_url("https://").error ==
               steelbox::UrlError::MissingHost,
           "URL sem host deveria ser rejeitada.");
}

void test_locked_vault_rejects_operations() {
    steelbox::Vault vault;
    const steelbox::PasswordEntry entry = valid_entry();

    expect(vault.state() == steelbox::VaultState::Locked,
           "O cofre deveria iniciar bloqueado.");
    expect(vault.entries() == nullptr,
           "Cofre bloqueado nao deveria expor suas entradas.");
    expect(vault.find_entry("anything") == nullptr,
           "Busca em cofre bloqueado deveria falhar.");

    const auto result = vault.add_entry(entry);
    expect(result.error == steelbox::VaultError::Locked,
           "Adicionar em cofre bloqueado deveria retornar Locked.");
}

void test_authenticated_unlock_and_secure_lock() {
    steelbox::Vault vault;
    expect(vault.unlock("session-password").succeeded(),
           "A primeira autenticacao deveria iniciar a sessao.");
    expect(vault.state() == steelbox::VaultState::Unlocked,
           "A sessao deveria estar desbloqueada.");
    vault.lock();
    expect(vault.state() == steelbox::VaultState::Locked,
           "O cofre deveria voltar a ficar bloqueado.");
    expect(!vault.unlock("wrong-password").succeeded(),
           "Uma senha diferente deveria ser rejeitada.");
    expect(vault.unlock("session-password").succeeded(),
           "A senha original deveria desbloquear o cofre.");
}

void test_search_and_password_generation() {
    steelbox::Vault vault;
    vault.unlock();
    auto entry = valid_entry();
    entry.tags = {"Work", "Code"};
    expect(vault.add_entry(entry).succeeded(),
           "A entrada de busca deveria ser adicionada.");

    expect(vault.search_entries("github").size() == 1,
           "A busca deveria encontrar o dominio.");
    expect(vault.search_entries("work").size() == 1,
           "A busca deveria ignorar maiusculas em tags.");
    expect(vault.search_entries("missing").empty(),
           "A busca deveria retornar vazio para termos ausentes.");

    const auto generated = steelbox::generate_password({.length = 32,
                                                        .lowercase = true,
                                                        .uppercase = true,
                                                        .digits = true,
                                                        .symbols = true});
    expect(generated.succeeded && generated.password.size() == 32,
           "O gerador deveria produzir o tamanho solicitado.");
    const auto only_digits = steelbox::generate_password({.length = 16,
                                                          .lowercase = false,
                                                          .uppercase = false,
                                                          .digits = true,
                                                          .symbols = false});
    expect(only_digits.succeeded && only_digits.password.find_first_not_of(
                                        "0123456789") == std::string::npos,
           "O gerador deveria respeitar os conjuntos selecionados.");
}

void test_hardened_deserialization() {
    steelbox::Vault vault;
    vault.unlock();
    expect(
        !steelbox::deserialize_vault(
             R"({"format_version":1,"entries":[{"id":"123e4567-e89b-42d3-a456-426614174000","title":"","username":"u","password":"p","url":"https://example.com","notes":"","tags":[],"created_at":"2025-02-29T00:00:00Z","updated_at":"2025-02-28T00:00:00Z"}]})",
             vault)
             .succeeded,
        "Data que nao existe no calendario deveria ser rejeitada.");
    expect(
        steelbox::deserialize_vault(
            R"({"format_version":1,"entries":[{"id":"123e4567-e89b-42d3-a456-426614174000","title":"","username":"u","password":"p","url":"https://example.com","notes":"","tags":[],"created_at":"2024-02-29T23:59:59Z","updated_at":"2024-02-29T23:59:59Z"}]})",
            vault)
            .succeeded,
        "Dia bissexto valido deveria ser aceito.");
    expect(
        !steelbox::deserialize_vault(
             R"({"format_version":1,"entries":[{"id":"123e4567-e89b-42d3-a456-426614174000","title":"","username":"u","password":"p","url":"https://example.com","notes":"","tags":[],"created_at":"2026-01-01T24:00:00Z","updated_at":"2026-01-01T00:00:00Z"}]})",
             vault)
             .succeeded,
        "Hora fora do intervalo deveria ser rejeitada.");
    expect(
        steelbox::deserialize_vault(
            R"({"format_version":1,"entries":[{"id":"bad","title":"","username":"u","password":"p","url":"https://example.com","normalized_domain":"example.com","notes":"","tags":[],"created_at":"2026-01-01T00:00:00Z","updated_at":"2026-01-01T00:00:00Z"}]})",
            vault)
                .succeeded == false,
        "UUID invalido deveria ser rejeitado.");
    expect(steelbox::deserialize_vault(
               R"({"format_version":1,"entries":[],"extra":true})", vault)
               .succeeded,
           "Campos futuros deveriam ser ignorados para compatibilidade.");
}

void test_add_generates_uuid_and_timestamps() {
    steelbox::Vault vault;
    vault.unlock();

    const auto result = vault.add_entry(valid_entry());
    expect(result.succeeded(), "Entrada valida deveria ser adicionada.");
    expect(
        std::regex_match(
            result.entry_id,
            std::regex(
                R"([0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12})")),
        "O ID gerado deveria ser um UUID v4.");

    const auto* entries = vault.entries();
    expect(entries != nullptr && entries->size() == 1,
           "O cofre deveria conter uma entrada.");
    expect(entries->front().id == result.entry_id,
           "O UUID retornado deveria ser o UUID armazenado.");
    expect(entries->front().normalized_domain == "github.com",
           "O dominio normalizado deveria ser armazenado.");
    expect(entries->front().created_at.size() == 20,
           "created_at deveria usar ISO 8601 UTC.");
    expect(entries->front().updated_at == entries->front().created_at,
           "created_at e updated_at deveriam iniciar iguais.");
    expect(entries->front().created_at.back() == 'Z',
           "O timestamp deveria estar em UTC.");
}

void test_update_preserves_creation_date() {
    steelbox::Vault vault;
    vault.unlock();

    const auto add_result = vault.add_entry(valid_entry());
    expect(add_result.succeeded(), "A entrada deveria ser adicionada.");
    const auto* original = vault.find_entry(add_result.entry_id);
    expect(original != nullptr, "A entrada original deveria ser encontrada.");
    const std::string original_created_at = original->created_at;

    steelbox::PasswordEntry updated = valid_entry();
    updated.id = add_result.entry_id;
    updated.title = "Updated title";
    updated.username = "updated-user";

    const auto update_result = vault.update_entry(updated);
    expect(update_result.succeeded(), "A entrada deveria ser atualizada.");

    const auto* found = vault.find_entry(add_result.entry_id);
    expect(found != nullptr, "A entrada atualizada deveria ser encontrada.");
    expect(found->title == "Updated title",
           "O titulo atualizado deveria ser armazenado.");
    expect(found->normalized_domain == "github.com",
           "O dominio normalizado deveria ser preservado na edicao.");
    expect(found->created_at == original_created_at,
           "created_at nao deveria ser alterado na edicao.");
    expect(!found->updated_at.empty(),
           "updated_at deveria continuar preenchido.");
}

void test_const_vault_access() {
    steelbox::Vault vault;
    vault.unlock();
    const auto add_result = vault.add_entry(valid_entry());
    expect(add_result.succeeded(), "A entrada deveria ser adicionada.");

    const steelbox::Vault& read_only_vault = vault;
    const steelbox::PasswordEntry* entry =
        read_only_vault.find_entry(add_result.entry_id);
    expect(entry != nullptr, "A busca const deveria encontrar a entrada.");
    expect(read_only_vault.entries() != nullptr,
           "A leitura const deveria expor as entradas desbloqueadas.");
}

void test_missing_entry_errors() {
    steelbox::Vault vault;
    vault.unlock();

    steelbox::PasswordEntry entry = valid_entry();
    entry.id = "does-not-exist";

    const auto update_result = vault.update_entry(entry);
    expect(update_result.error == steelbox::VaultError::EntryNotFound,
           "Editar entrada inexistente deveria retornar EntryNotFound.");

    const auto remove_result = vault.remove_entry("does-not-exist");
    expect(remove_result.error == steelbox::VaultError::EntryNotFound,
           "Remover entrada inexistente deveria retornar EntryNotFound.");
}

void test_remove_and_lock() {
    steelbox::Vault vault;
    vault.unlock();

    const auto add_result = vault.add_entry(valid_entry());
    expect(add_result.succeeded(), "A entrada deveria ser adicionada.");
    expect(vault.remove_entry(add_result.entry_id).succeeded(),
           "A entrada deveria ser removida.");
    expect(vault.find_entry(add_result.entry_id) == nullptr,
           "A entrada removida nao deveria ser encontrada.");

    vault.lock();
    expect(vault.state() == steelbox::VaultState::Locked,
           "O cofre deveria estar bloqueado.");
    expect(vault.entries() == nullptr,
           "Cofre bloqueado nao deveria expor entradas.");
}

void test_serialization_round_trip() {
    steelbox::Vault vault;
    vault.unlock();
    const auto add_result = vault.add_entry(valid_entry());
    expect(add_result.succeeded(), "A entrada deveria ser adicionada.");

    const auto serialized = steelbox::serialize_vault(vault);
    expect(serialized.succeeded, "A serializacao deveria funcionar.");
    expect(serialized.data.find("\"format_version\": 2") != std::string::npos,
           "A serializacao deveria conter a versao do formato.");

    steelbox::Vault restored;
    const auto deserialized =
        steelbox::deserialize_vault(serialized.data, restored);
    expect(deserialized.succeeded, "A desserializacao deveria funcionar.");
    expect(restored.state() == steelbox::VaultState::Unlocked,
           "O cofre desserializado deveria estar desbloqueado.");

    const auto* entries = restored.entries();
    expect(entries != nullptr && entries->size() == 1,
           "O round-trip deveria preservar as entradas.");
    expect(entries->front().id == add_result.entry_id,
           "O round-trip deveria preservar o UUID.");
    expect(entries->front().password == "test-password",
           "O round-trip deveria preservar a senha.");
}

void test_secure_string_copy_move_and_cleanup() {
    static_assert(std::is_copy_constructible_v<steelbox::PasswordEntry>);
    static_assert(std::is_move_constructible_v<steelbox::PasswordEntry>);

    steelbox::PasswordEntry original = valid_entry();
    steelbox::PasswordEntry copy = original;
    expect(copy.password == "test-password",
           "A copia deveria preservar a senha protegida.");

    steelbox::PasswordEntry moved = std::move(copy);
    expect(moved.password == "test-password",
           "A movimentacao deveria preservar a senha protegida.");
    copy.password = "replacement";
    expect(moved.password == "test-password",
           "A copia movida nao deveria compartilhar o buffer.");

    moved.password.clear();
    expect(moved.password.empty(),
           "A limpeza explicita deveria remover a senha.");
}

void test_format_migration() {
    steelbox::Vault restored;
    const auto result = steelbox::deserialize_vault(
        R"({"format_version":1,"entries":[{"id":"123e4567-e89b-42d3-a456-426614174000","title":"","username":"u","password":"p","url":"https://example.com","notes":"","tags":[],"created_at":"2026-01-01T00:00:00Z","updated_at":"2026-01-01T00:00:00Z"}]})",
        restored);
    expect(result.succeeded, "Formato 1 deveria ser migrado explicitamente.");
    expect(restored.entries()->front().normalized_domain == "example.com",
           "A migracao deveria derivar o dominio normalizado.");
}

void test_storage_backup_and_atomic_replace() {
    const auto directory =
        std::filesystem::temp_directory_path() / "steelbox-core-tests";
    std::filesystem::create_directories(directory);
    const auto vault_path = directory / "test.vault";
    const auto backup_path =
        std::filesystem::path(vault_path.string() + ".bak");

    steelbox::Vault first;
    first.unlock();
    expect(first.add_entry(valid_entry()).succeeded(),
           "O primeiro cofre deveria aceitar a entrada.");
    expect(steelbox::save_vault(first, vault_path, "master-password").succeeded,
           "O primeiro salvamento deveria funcionar.");

    steelbox::Vault second;
    second.unlock();
    auto second_entry = valid_entry();
    second_entry.title = "Second version";
    expect(second.add_entry(second_entry).succeeded(),
           "O segundo cofre deveria aceitar a entrada.");
    expect(
        steelbox::save_vault(second, vault_path, "master-password").succeeded,
        "O segundo salvamento deveria funcionar.");
    expect(std::filesystem::exists(backup_path),
           "O segundo salvamento deveria criar backup.");

    steelbox::Vault loaded;
    expect(
        steelbox::load_vault(loaded, vault_path, "master-password").succeeded,
        "O cofre salvo deveria ser carregado.");
    const auto* entries = loaded.entries();
    expect(entries != nullptr && entries->front().title == "Second version",
           "O arquivo principal deveria conter a versao mais recente.");

    std::filesystem::remove_all(directory);
}

void test_storage_failure_paths() {
    const auto directory = std::filesystem::temp_directory_path() /
                           "steelbox-storage-failure-tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    steelbox::Vault first;
    first.unlock();
    expect(first.add_entry(valid_entry()).succeeded(),
           "O cofre inicial deveria aceitar a entrada.");

    const auto missing_parent_path = directory / "missing" / "vault.vault";
    expect(!steelbox::save_vault(first, missing_parent_path, "master-password")
                .succeeded,
           "Salvar em um diretorio inexistente deveria falhar.");
    expect(!std::filesystem::exists(missing_parent_path),
           "Falha ao criar o destino nao deveria criar o cofre.");
    expect(!steelbox::load_vault(first, missing_parent_path, "master-password")
                .succeeded,
           "Carregar um arquivo inexistente deveria falhar.");

    const auto vault_path = directory / "existing.vault";
    expect(steelbox::save_vault(first, vault_path, "master-password").succeeded,
           "O salvamento inicial deveria funcionar.");

    const auto backup_path =
        std::filesystem::path(vault_path.string() + ".bak");
    std::filesystem::create_directory(backup_path);
    steelbox::Vault second;
    second.unlock();
    auto second_entry = valid_entry();
    second_entry.title = "Replacement version";
    expect(second.add_entry(second_entry).succeeded(),
           "O segundo cofre deveria aceitar a entrada.");
    expect(
        !steelbox::save_vault(second, vault_path, "master-password").succeeded,
        "Falha ao criar o backup deveria rejeitar o salvamento.");
       expect(!std::filesystem::exists(vault_path.string() + ".tmp"),
                 "Falha no backup deveria remover o arquivo temporario.");

    steelbox::Vault loaded;
    expect(
        steelbox::load_vault(loaded, vault_path, "master-password").succeeded,
        "O cofre original deveria continuar carregavel apos a falha.");
    expect(loaded.entries()->front().title == "GitHub",
           "A falha no backup nao deveria substituir o arquivo original.");

    std::filesystem::remove_all(directory);
}

void test_encryption_round_trip_and_authentication() {
    const std::string payload = "payload de teste";
    const auto encrypted =
        steelbox::encrypt_payload(payload, "master-password");
    expect(encrypted.succeeded, "A cifragem deveria funcionar.");
    expect(encrypted.data != payload,
           "O payload cifrado nao deveria ser texto puro.");

    const auto decrypted =
        steelbox::decrypt_payload(encrypted.data, "master-password");
    expect(decrypted.succeeded && decrypted.data == payload,
           "A decifragem deveria recuperar o payload.");

    const auto wrong_password =
        steelbox::decrypt_payload(encrypted.data, "wrong-password");
    expect(!wrong_password.succeeded, "Senha mestra incorreta deveria falhar.");

    std::string tampered = encrypted.data;
    tampered.back() ^= 0x01;
    const auto tampered_result =
        steelbox::decrypt_payload(tampered, "master-password");
    expect(!tampered_result.succeeded,
           "Payload adulterado deveria falhar na autenticacao.");
}
void test_encrypted_storage() {
    const auto directory =
        std::filesystem::temp_directory_path() / "steelbox-crypto-tests";
    std::filesystem::create_directories(directory);
    const auto vault_path = directory / "encrypted.vault";

    steelbox::Vault vault;
    vault.unlock();
    expect(vault.add_entry(valid_entry()).succeeded(),
           "O cofre deveria aceitar a entrada.");
    expect(steelbox::save_vault(vault, vault_path, "master-password").succeeded,
           "O salvamento criptografado deveria funcionar.");

    std::ifstream input(vault_path, std::ios::binary);
    const std::string file_data{std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>()};
    input.close();
    expect(file_data.find("test-password") == std::string::npos,
           "O arquivo nao deveria conter a senha em texto puro.");

    steelbox::Vault loaded;
    expect(
        steelbox::load_vault(loaded, vault_path, "master-password").succeeded,
        "O carregamento criptografado deveria funcionar.");

    steelbox::Vault wrong_password;
    expect(!steelbox::load_vault(wrong_password, vault_path, "wrong-password")
                .succeeded,
           "Carregamento com senha errada deveria falhar.");

    {
        std::fstream file(vault_path,
                          std::ios::binary | std::ios::in | std::ios::out);
        expect(file.good(),
               "O arquivo deveria estar disponivel para corrupcao.");
        file.seekp(0);
        const char invalid_magic = 'X';
        file.write(&invalid_magic, 1);
    }
    steelbox::Vault corrupted;
    expect(!steelbox::load_vault(corrupted, vault_path, "master-password")
                .succeeded,
           "Magic corrompido deveria ser rejeitado.");

    std::filesystem::remove_all(directory);
}

void run_test(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << "\n";
}

} // namespace

int main() {
    try {
        run_test("entry validation", test_entry_validation);
        run_test("URL normalization", test_url_normalization);
        run_test("locked vault", test_locked_vault_rejects_operations);
        run_test("authenticated unlock",
                 test_authenticated_unlock_and_secure_lock);
        run_test("UUID and timestamps", test_add_generates_uuid_and_timestamps);
        run_test("entry update", test_update_preserves_creation_date);
        run_test("missing entry errors", test_missing_entry_errors);
        run_test("remove and lock", test_remove_and_lock);
        run_test("const vault access", test_const_vault_access);
        run_test("serialization round trip", test_serialization_round_trip);
        run_test("secure string lifecycle",
                 test_secure_string_copy_move_and_cleanup);
        run_test("format migration", test_format_migration);
        run_test("storage backup and atomic replace",
                 test_storage_backup_and_atomic_replace);
        run_test("storage failure paths", test_storage_failure_paths);
        run_test("encryption round trip and authentication",
                 test_encryption_round_trip_and_authentication);
        run_test("encrypted storage", test_encrypted_storage);
        run_test("hardened deserialization", test_hardened_deserialization);
        run_test("search and password generation",
                 test_search_and_password_generation);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
