#pragma once

#include <steelbox/models/password_entry.hpp>

#include <string>
#include <vector>

namespace steelbox {

    class Vault {
        public:
            void add_entry(const PasswordEntry& entry);
            bool remove_entry(const std::string& id);
            PasswordEntry* find_entry(const std::string& id);

            const std::vector<PasswordEntry>& entries() const;

        private:
            std::vector<PasswordEntry> entries_;
    };
}