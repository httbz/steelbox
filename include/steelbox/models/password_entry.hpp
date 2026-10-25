#pragma once

#include <string>
#include <vector>

namespace steelbox {

    struct PasswordEntry {
        std::string id;
        std::string title;
        std::string username;
        std::string password;
        std::string url;
        std::string notes;
        std::vector<std::string> tags;
    };
}