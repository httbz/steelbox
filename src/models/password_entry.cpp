#include <steelbox/models/password_entry.hpp>

namespace steelbox {

ValidationResult validate_entry(const PasswordEntry& entry) {
    if (entry.url.empty()) {
        return {
            EntryError::MissingUrl,
            "A URL e obrigatoria."
        };
    }

    if (entry.username.empty()) {
        return {
            EntryError::MissingUsername,
            "O usuario e obrigatorio."
        };
    }

    if (entry.password.empty()) {
        return {
            EntryError::MissingPassword,
            "A senha e obrigatoria."
        };
    }

    return {};
}

}