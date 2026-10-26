#include <steelbox/models/password_entry.hpp>
#include <steelbox/models/url.hpp>

namespace steelbox {

ValidationResult validate_entry(const PasswordEntry& entry) {
    if (entry.url.empty()) {
        return {
            EntryError::MissingUrl,
            "A URL e obrigatoria."
        };
    }

    const UrlResult url_result = normalize_url(entry.url);
    if (!url_result.valid()) {
        return {
            EntryError::InvalidUrl,
            url_result.message
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