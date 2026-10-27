#pragma once

#include <steelbox/models/vault.hpp>

#include <string>

namespace steelbox {

struct SerializationResult {
    bool succeeded = false;
    std::string data;
    std::string message;
};

SerializationResult serialize_vault(const Vault& vault);
SerializationResult deserialize_vault(const std::string& data, Vault& vault);

}
