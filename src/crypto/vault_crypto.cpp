#include <steelbox/crypto/vault_crypto.hpp>

#include <sodium.h>

#include <array>
#include <string>
#include <utility>

namespace steelbox {

namespace {

constexpr std::size_t salt_size = crypto_pwhash_SALTBYTES;
constexpr std::size_t nonce_size =
    crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
constexpr std::size_t key_size =
    crypto_aead_xchacha20poly1305_ietf_KEYBYTES;

bool initialize_sodium() {
    return sodium_init() >= 0;
}

bool derive_key(
    const std::string& password,
    const unsigned char* salt,
    unsigned char* key
) {
    return crypto_pwhash(
        key,
        key_size,
        password.data(),
        password.size(),
        salt,
        crypto_pwhash_OPSLIMIT_MODERATE,
        crypto_pwhash_MEMLIMIT_MODERATE,
        crypto_pwhash_ALG_ARGON2ID13
    ) == 0;
}

}

CryptoResult encrypt_payload(
    const std::string& payload,
    const std::string& master_password,
    std::string_view associated_data
) {
    if (!initialize_sodium()) {
        return {false, "", "Nao foi possivel inicializar a libsodium."};
    }

    if (master_password.empty()) {
        return {false, "", "A senha mestra nao pode ser vazia."};
    }

    std::array<unsigned char, salt_size> salt{};
    std::array<unsigned char, nonce_size> nonce{};
    std::array<unsigned char, key_size> key{};
    randombytes_buf(salt.data(), salt.size());
    randombytes_buf(nonce.data(), nonce.size());

    if (!derive_key(master_password, salt.data(), key.data())) {
        sodium_memzero(key.data(), key.size());
        return {false, "", "Nao foi possivel derivar a chave."};
    }

    std::string ciphertext(
        payload.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES,
        '\0'
    );
    unsigned long long ciphertext_size = 0;

    const int encryption_result =
        crypto_aead_xchacha20poly1305_ietf_encrypt(
            reinterpret_cast<unsigned char*>(ciphertext.data()),
            &ciphertext_size,
            reinterpret_cast<const unsigned char*>(payload.data()),
            payload.size(),
            reinterpret_cast<const unsigned char*>(associated_data.data()),
            associated_data.size(),
            nullptr,
            nonce.data(),
            key.data()
        );

    sodium_memzero(key.data(), key.size());
    if (encryption_result != 0) {
        return {false, "", "Nao foi possivel cifrar o cofre."};
    }

    ciphertext.resize(static_cast<std::size_t>(ciphertext_size));
    std::string result;
    result.append(reinterpret_cast<const char*>(salt.data()), salt.size());
    result.append(reinterpret_cast<const char*>(nonce.data()), nonce.size());
    result.append(ciphertext);
    return {true, std::move(result), ""};
}

CryptoResult decrypt_payload(
    const std::string& encrypted_data,
    const std::string& master_password,
    std::string_view associated_data
) {
    if (!initialize_sodium()) {
        return {false, "", "Nao foi possivel inicializar a libsodium."};
    }

    if (master_password.empty()) {
        return {false, "", "A senha mestra nao pode ser vazia."};
    }

    constexpr std::size_t header_size = salt_size + nonce_size;
    if (encrypted_data.size() < header_size +
            crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        return {false, "", "Payload criptografado incompleto."};
    }

    const auto* salt = reinterpret_cast<const unsigned char*>(
        encrypted_data.data()
    );
    const auto* nonce = salt + salt_size;
    const auto* ciphertext = nonce + nonce_size;
    const std::size_t ciphertext_size =
        encrypted_data.size() - header_size;

    std::array<unsigned char, key_size> key{};
    if (!derive_key(master_password, salt, key.data())) {
        sodium_memzero(key.data(), key.size());
        return {false, "", "Nao foi possivel derivar a chave."};
    }

    std::string payload(ciphertext_size, '\0');
    unsigned long long payload_size = 0;
    const int decryption_result =
        crypto_aead_xchacha20poly1305_ietf_decrypt(
            reinterpret_cast<unsigned char*>(payload.data()),
            &payload_size,
            nullptr,
            ciphertext,
            ciphertext_size,
            reinterpret_cast<const unsigned char*>(associated_data.data()),
            associated_data.size(),
            nonce,
            key.data()
        );

    sodium_memzero(key.data(), key.size());
    if (decryption_result != 0) {
        return {false, "", "Senha mestra incorreta ou cofre adulterado."};
    }

    payload.resize(static_cast<std::size_t>(payload_size));
    return {true, std::move(payload), ""};
}

}
