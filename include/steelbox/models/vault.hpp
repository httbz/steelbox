#pragma once

#include <steelbox/models/password_entry.hpp>

#include <string>
#include <vector>

namespace steelbox {

enum class VaultState {
    Locked,
    Unlocked
};

enum class VaultError {
    None,
    Locked,
    DuplicateId,
    EntryNotFound,
    InvalidEntry
};

struct VaultResult {
    VaultError error = VaultError::None;
    std::string message;
    std::string entry_id;

    bool succeeded() const {
        return error == VaultError::None;
    }
};

class Vault {
public:
    VaultResult add_entry(PasswordEntry entry);
    VaultResult update_entry(const PasswordEntry& updated_entry);
    VaultResult remove_entry(const std::string& id);
    PasswordEntry* find_entry(const std::string& id);

    void unlock();
    void lock();
    VaultState state() const;
    const std::vector<PasswordEntry>* entries() const;

private:
    std::vector<PasswordEntry> entries_;
    VaultState state_ = VaultState::Locked;
};

}