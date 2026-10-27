#pragma once

#include <cstddef>
#include <string>

namespace steelbox {

struct PasswordGeneratorOptions {
    std::size_t length = 20;
    bool lowercase = true;
    bool uppercase = true;
    bool digits = true;
    bool symbols = true;
};

struct PasswordGenerationResult {
    bool succeeded = false;
    std::string password;
    std::string message;
};

PasswordGenerationResult generate_password(
    const PasswordGeneratorOptions& options = {}
);

}
