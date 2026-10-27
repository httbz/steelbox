#include <steelbox/models/password_entry.hpp>
#include <steelbox/models/url.hpp>
#include <steelbox/models/vault.hpp>
#include <steelbox/storage/vault_serializer.hpp>
#include <steelbox/storage/vault_storage.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

steelbox::PasswordEntry valid_entry() {
    return {
        .id = "caller-id-must-be-ignored",
        .title = "GitHub",
        .username = "test-user",
        .password = "test-password",
        .url = "https://github.com",
        .notes = "Test entry",
        .tags = {"test"},
        .created_at = "",
        .updated_at = ""
    };
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
        "https://WWW.GitHub.com/login?source=menu#form"
    );
    expect(github.valid() && github.domain == "github.com",
           "Dominio HTTPS deveria ser normalizado.");

    const auto subdomain = steelbox::normalize_url(
        "https://login.github.com"
    );
    expect(subdomain.valid() && subdomain.domain == "login.github.com",
           "Subdominio nao deveria ser removido.");

    const auto local = steelbox::normalize_url(
        "http://localhost:3000/login"
    );
    expect(local.valid() && local.domain == "localhost",
           "Host local deveria ser aceito.");

    const auto ip = steelbox::normalize_url(
        "http://192.168.0.10:8080"
    );
    expect(ip.valid() && ip.domain == "192.168.0.10",
           "Endereco IP deveria ser aceito.");

    expect(
        steelbox::normalize_url("ftp://github.com").error ==
            steelbox::UrlError::UnsupportedScheme,
        "FTP deveria ser rejeitado."
    );
    expect(
        steelbox::normalize_url("javascript:alert(1)").error ==
            steelbox::UrlError::UnsupportedScheme,
        "Javascript URL deveria ser rejeitada."
    );
    expect(
        steelbox::normalize_url("https://").error ==
            steelbox::UrlError::MissingHost,
        "URL sem host deveria ser rejeitada."
    );
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

void test_add_generates_uuid_and_timestamps() {
    steelbox::Vault vault;
    vault.unlock();

    const auto result = vault.add_entry(valid_entry());
    expect(result.succeeded(), "Entrada valida deveria ser adicionada.");
    expect(std::regex_match(
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
    expect(serialized.data.find("\"format_version\": 1") !=
               std::string::npos,
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

void test_storage_backup_and_atomic_replace() {
    const auto directory =
        std::filesystem::temp_directory_path() / "steelbox-core-tests";
    std::filesystem::create_directories(directory);
    const auto vault_path = directory / "test.vault";
    const auto backup_path = std::filesystem::path(vault_path.string() + ".bak");

    steelbox::Vault first;
    first.unlock();
    expect(first.add_entry(valid_entry()).succeeded(),
           "O primeiro cofre deveria aceitar a entrada.");
    expect(steelbox::save_vault(first, vault_path).succeeded,
           "O primeiro salvamento deveria funcionar.");

    steelbox::Vault second;
    second.unlock();
    auto second_entry = valid_entry();
    second_entry.title = "Second version";
    expect(second.add_entry(second_entry).succeeded(),
           "O segundo cofre deveria aceitar a entrada.");
    expect(steelbox::save_vault(second, vault_path).succeeded,
           "O segundo salvamento deveria funcionar.");
    expect(std::filesystem::exists(backup_path),
           "O segundo salvamento deveria criar backup.");

    steelbox::Vault loaded;
    expect(steelbox::load_vault(loaded, vault_path).succeeded,
           "O cofre salvo deveria ser carregado.");
    const auto* entries = loaded.entries();
    expect(entries != nullptr && entries->front().title == "Second version",
           "O arquivo principal deveria conter a versao mais recente.");

    std::filesystem::remove_all(directory);
}

void run_test(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << "\n";
}

}

int main() {
    try {
        run_test("entry validation", test_entry_validation);
        run_test("URL normalization", test_url_normalization);
        run_test("locked vault", test_locked_vault_rejects_operations);
        run_test("UUID and timestamps", test_add_generates_uuid_and_timestamps);
        run_test("entry update", test_update_preserves_creation_date);
        run_test("missing entry errors", test_missing_entry_errors);
        run_test("remove and lock", test_remove_and_lock);
        run_test("const vault access", test_const_vault_access);
        run_test("serialization round trip", test_serialization_round_trip);
        run_test(
            "storage backup and atomic replace",
            test_storage_backup_and_atomic_replace
        );
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
