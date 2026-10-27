#include <steelbox/crypto/password_generator.hpp>

#include <sodium.h>

#include <algorithm>
#include <string>

namespace steelbox {

PasswordGenerationResult generate_password(
    const PasswordGeneratorOptions& options
) {
    if (sodium_init() < 0) {
        return {false, "", "Nao foi possivel inicializar a libsodium."};
    }
    if (options.length < 4 || options.length > 1024) {
        return {false, "", "O comprimento deve estar entre 4 e 1024."};
    }

    std::string alphabet;
    if (options.lowercase) alphabet += "abcdefghijklmnopqrstuvwxyz";
    if (options.uppercase) alphabet += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (options.digits) alphabet += "0123456789";
    if (options.symbols) alphabet += "!@#$%^&*()-_=+[]{};:,.?/|~";
    if (alphabet.empty()) {
        return {false, "", "Pelo menos um conjunto de caracteres e necessario."};
    }

    std::string result;
    result.reserve(options.length);
    for (std::size_t index = 0; index < options.length; ++index) {
        result.push_back(
            alphabet[randombytes_uniform(
                static_cast<uint32_t>(alphabet.size())
            )]
        );
    }

    return {true, result, ""};
}

}
