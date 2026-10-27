#pragma once

#include <string>
#include <string_view>

namespace steelbox {

struct CryptoResult {
    bool succeeded = false;
    std::string data;
    std::string message;
};

CryptoResult encrypt_payload(
    const std::string& payload,
    const std::string& master_password,
    std::string_view associated_data = {}
);

CryptoResult decrypt_payload(
    const std::string& encrypted_data,
    const std::string& master_password,
    std::string_view associated_data = {}
);

}
