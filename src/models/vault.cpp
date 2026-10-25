#include <steelbox/models/vault.hpp>

#include <algorithm>

namespace steelbox {

void Vault::add_entry(const PasswordEntry& entry) {
    entries_.push_back(entry);
}

bool Vault::update_entry(const PasswordEntry& updated_entry) {
    PasswordEntry* existing_entry = find_entry(updated_entry.id);

    if (existing_entry == nullptr) {
        return false;
    }

    *existing_entry = updated_entry;
    return true;
}

bool Vault::remove_entry(const std::string& id) {
    const auto entry_iterator = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&id](const PasswordEntry& entry) {
            return entry.id == id;
        }
    );

    if (entry_iterator == entries_.end()) {
        return false;
    }

    entries_.erase(entry_iterator);
    return true;
};

PasswordEntry* Vault::find_entry(const std::string& id) {
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

const std::vector<PasswordEntry>& Vault::entries() const{
    return entries_;
}
}