#include <steelbox/storage/vault_serializer.hpp>
#include <steelbox/models/url.hpp>

#include <nlohmann/json.hpp>

#include <regex>
#include <string>
#include <utility>
#include <sodium.h>

namespace steelbox {

namespace {

using json = nlohmann::json;
constexpr int current_format_version = 2;

void to_json(json& value, const PasswordEntry& entry) {
    value = json{
        {"id", entry.id},
        {"title", entry.title},
        {"username", entry.username},
        {"password", std::string(entry.password.view())},
        {"url", entry.url},
        {"normalized_domain", entry.normalized_domain},
        {"notes", entry.notes},
        {"tags", entry.tags},
        {"created_at", entry.created_at},
        {"updated_at", entry.updated_at}
    };
}

bool from_json(const json& value, PasswordEntry& entry, std::string& error) {
    try {
        if (!value.is_object()) {
            error = "A entrada deve ser um objeto.";
            return false;
        }
        entry.id = value.at("id").get<std::string>();
        entry.title = value.at("title").get<std::string>();
        entry.username = value.at("username").get<std::string>();
        std::string password = value.at("password").get<std::string>();
        entry.password = password;
        sodium_memzero(password.data(), password.size());
        entry.url = value.at("url").get<std::string>();
        entry.normalized_domain =
            value.at("normalized_domain").get<std::string>();
        entry.notes = value.at("notes").get<std::string>();
        entry.tags = value.at("tags").get<std::vector<std::string>>();
        entry.created_at = value.at("created_at").get<std::string>();
        entry.updated_at = value.at("updated_at").get<std::string>();

        static const std::regex uuid_pattern(
            R"([0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12})"
        );
        static const std::regex timestamp_pattern(
            R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)"
        );
        if (!std::regex_match(entry.id, uuid_pattern)) {
            error = "ID deve ser um UUID v4.";
            return false;
        }
        if (!std::regex_match(entry.created_at, timestamp_pattern) ||
            !std::regex_match(entry.updated_at, timestamp_pattern)) {
            error = "Datas devem estar em UTC ISO 8601.";
            return false;
        }
        if (entry.tags.size() > 256) {
            error = "A entrada possui tags demais.";
            return false;
        }
        for (const auto& tag : entry.tags) {
            if (tag.size() > 256) {
                error = "Uma tag excede o tamanho permitido.";
                return false;
            }
        }

        const UrlResult url_result = normalize_url(entry.url);
        if (!url_result.valid() ||
            entry.normalized_domain != url_result.domain) {
            error = "O dominio normalizado nao corresponde a URL.";
            return false;
        }
        return true;
    } catch (const json::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool migrate_v1_to_v2(json& root, std::string& error) {
    if (!root.at("entries").is_array()) {
        error = "O campo entries deve ser uma lista.";
        return false;
    }

    for (auto& serialized_entry : root.at("entries")) {
        if (!serialized_entry.is_object()) {
            error = "A entrada deve ser um objeto.";
            return false;
        }
        if (!serialized_entry.contains("normalized_domain")) {
            try {
                const auto url = serialized_entry.at("url").get<std::string>();
                const UrlResult normalized = normalize_url(url);
                if (!normalized.valid()) {
                    error = normalized.message;
                    return false;
                }
                serialized_entry["normalized_domain"] = normalized.domain;
            } catch (const json::exception& exception) {
                error = exception.what();
                return false;
            }
        }
    }

    root["format_version"] = current_format_version;
    root["schema"] = "steelbox.vault";
    return true;
}

}

SerializationResult serialize_vault(const Vault& vault) {
    if (vault.state() == VaultState::Locked) {
        return {false, "", "O cofre esta bloqueado."};
    }

    const auto* entries = vault.entries();
    if (entries == nullptr) {
        return {false, "", "Nao foi possivel acessar as entradas."};
    }

    nlohmann::json root;
    root["format_version"] = current_format_version;
    root["schema"] = "steelbox.vault";
    root["entries"] = nlohmann::json::array();

    for (const auto& entry : *entries) {
        nlohmann::json serialized_entry;
        to_json(serialized_entry, entry);
        root["entries"].push_back(serialized_entry);
    }

    return {true, root.dump(2), ""};
}

SerializationResult deserialize_vault(const std::string& data, Vault& vault) {
    try {
        auto root = nlohmann::json::parse(data);
        if (!root.is_object() ||
            !root.contains("format_version") ||
            !root.contains("entries")) {
            return {false, "", "Estrutura JSON do cofre invalida."};
        }
        if (!root["format_version"].is_number_integer()) {
            return {false, "", "A versao do formato deve ser um inteiro."};
        }
        const int format_version = root.at("format_version").get<int>();
        if (format_version == 1) {
            std::string migration_error;
            if (!migrate_v1_to_v2(root, migration_error)) {
                return {false, "", "Migracao para o formato 2 falhou: " +
                    migration_error};
            }
        } else if (format_version != current_format_version) {
            return {false, "", "Versao do formato nao suportada."};
        }

        if (!root.at("schema").is_string() ||
            root.at("schema").get<std::string>() != "steelbox.vault") {
            return {false, "", "Schema do cofre invalido."};
        }
        if (!root.at("entries").is_array()) {
            return {false, "", "O campo entries deve ser uma lista."};
        }
        if (root.at("entries").size() > 100000) {
            return {false, "", "O cofre possui entradas demais."};
        }

        Vault loaded_vault;
        loaded_vault.unlock();

        for (const auto& serialized_entry : root.at("entries")) {
            PasswordEntry entry;
            std::string error;
            if (!from_json(serialized_entry, entry, error)) {
                return {false, "", "Entrada invalida: " + error};
            }

            const VaultResult result = loaded_vault.restore_entry(entry);
            if (!result.succeeded()) {
                return {false, "", result.message};
            }
        }

        vault = std::move(loaded_vault);
        return {true, "", ""};
    } catch (const nlohmann::json::exception& exception) {
        return {false, "", "JSON invalido: " + std::string(exception.what())};
    }
}

}
