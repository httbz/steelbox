#include <steelbox/storage/vault_serializer.hpp>

#include <nlohmann/json.hpp>

#include <utility>

namespace steelbox {

namespace {

using json = nlohmann::json;

void to_json(json& value, const PasswordEntry& entry) {
    value = json{
        {"id", entry.id},
        {"title", entry.title},
        {"username", entry.username},
        {"password", entry.password},
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
        entry.id = value.at("id").get<std::string>();
        entry.title = value.at("title").get<std::string>();
        entry.username = value.at("username").get<std::string>();
        entry.password = value.at("password").get<std::string>();
        entry.url = value.at("url").get<std::string>();
        entry.normalized_domain =
            value.at("normalized_domain").get<std::string>();
        entry.notes = value.at("notes").get<std::string>();
        entry.tags = value.at("tags").get<std::vector<std::string>>();
        entry.created_at = value.at("created_at").get<std::string>();
        entry.updated_at = value.at("updated_at").get<std::string>();
        return true;
    } catch (const json::exception& exception) {
        error = exception.what();
        return false;
    }
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
    root["format_version"] = 1;
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
        const auto root = nlohmann::json::parse(data);
        if (root.at("format_version").get<int>() != 1) {
            return {false, "", "Versao do formato nao suportada."};
        }

        if (!root.at("entries").is_array()) {
            return {false, "", "O campo entries deve ser uma lista."};
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
