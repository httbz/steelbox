#pragma once

#include <string>
#include <vector>

namespace steelbox {

enum class EntryError {
    None,
    MissingUrl,
    InvalidUrl,
    MissingUsername,
    MissingPassword
};

struct ValidationResult {
    EntryError error = EntryError::None;
    std::string message;

    bool valid() const {
        return error == EntryError::None;
    }
};

struct PasswordEntry {
    std::string id;
    std::string title;
    std::string username;
    std::string password;
    std::string url;
    std::string normalized_domain;
    std::string notes;
    std::vector<std::string> tags;
    std::string created_at;
    std::string updated_at;
};

ValidationResult validate_entry(const PasswordEntry& entry);

}