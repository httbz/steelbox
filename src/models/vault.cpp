#include <steelbox/models/vault.hpp>
#include <steelbox/models/url.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

namespace steelbox {

namespace {

std::string generate_uuid() {
    std::random_device random_device;
    std::array<unsigned char, 16> bytes{};

    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(random_device());
    }

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    std::ostringstream result;
    result << std::hex << std::setfill('0');

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result << std::setw(2) << static_cast<int>(bytes[index]);

        if (index == 3 || index == 5 || index == 7 || index == 9) {
            result << '-';
        }
    }

    return result.str();
}

std::string current_utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time{};

#if defined(_WIN32)
    gmtime_s(&utc_time, &now_time);
#else
    gmtime_r(&now_time, &utc_time);
#endif

    std::ostringstream result;
    result << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return result.str();
}

}

VaultResult Vault::add_entry(PasswordEntry entry) {
    if (state_ == VaultState::Locked) {
        return {
            VaultError::Locked,
            "O cofre esta bloqueado."
        };
    }

    const ValidationResult validation = validate_entry(entry);
    if (!validation.valid()) {
        return {
            VaultError::InvalidEntry,
            validation.message
        };
    }

    entry.id = generate_uuid();
    entry.normalized_domain = normalize_url(entry.url).domain;
    entry.created_at = current_utc_timestamp();
    entry.updated_at = entry.created_at;
    const std::string entry_id = entry.id;
    entries_.push_back(std::move(entry));
    return {
        VaultError::None,
        "",
        entry_id
    };
}

VaultResult Vault::update_entry(const PasswordEntry& updated_entry) {
    if (state_ == VaultState::Locked) {
        return {
            VaultError::Locked,
            "O cofre esta bloqueado."
        };
    }

    const ValidationResult validation = validate_entry(updated_entry);
    if (!validation.valid()) {
        return {
            VaultError::InvalidEntry,
            validation.message
        };
    }

    PasswordEntry* existing_entry = find_entry(updated_entry.id);

    if (existing_entry == nullptr) {
        return {
            VaultError::EntryNotFound,
            "A entrada nao foi encontrada."
        };
    }

    const std::string created_at = existing_entry->created_at;
    PasswordEntry normalized_entry = updated_entry;
    normalized_entry.normalized_domain =
        normalize_url(normalized_entry.url).domain;
    *existing_entry = normalized_entry;
    existing_entry->created_at = created_at;
    existing_entry->updated_at = current_utc_timestamp();
    return {};
}

VaultResult Vault::remove_entry(const std::string& id) {
    if (state_ == VaultState::Locked) {
        return {
            VaultError::Locked,
            "O cofre esta bloqueado."
        };
    }

    const auto entry_iterator = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&id](const PasswordEntry& entry) {
            return entry.id == id;
        }
    );

    if (entry_iterator == entries_.end()) {
        return {
            VaultError::EntryNotFound,
            "A entrada nao foi encontrada."
        };
    }

    entries_.erase(entry_iterator);
    return {};
}

PasswordEntry* Vault::find_entry(const std::string& id) {
    if (state_ == VaultState::Locked) {
        return nullptr;
    }

    const auto entry_iterator = std::find_if (
        entries_.begin(),
        entries_.end(),
        [&id](const PasswordEntry& entry) {
            return entry.id == id;
        }
    );

    if (entry_iterator == entries_.end()) {
        return nullptr;
    }

    return &*entry_iterator;
}

const PasswordEntry* Vault::find_entry(const std::string& id) const {
    if (state_ == VaultState::Locked) {
        return nullptr;
    }

    const auto entry_iterator = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&id](const PasswordEntry& entry) {
            return entry.id == id;
        }
    );

    if (entry_iterator == entries_.end()) {
        return nullptr;
    }

    return &*entry_iterator;
}

void Vault::unlock() {
    state_ = VaultState::Unlocked;
}

void Vault::lock() {
    state_ = VaultState::Locked;
}

VaultState Vault::state() const {
    return state_;
}

const std::vector<PasswordEntry>* Vault::entries() const {
    if (state_ == VaultState::Locked) {
        return nullptr;
    }

    return &entries_;
}

}