#pragma once

#include <steelbox/crypto/secure_buffer.hpp>

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace steelbox {

class SecureString {
public:
    SecureString() = default;

    SecureString(std::string_view value) {
        assign(value);
    }

    SecureString(const SecureString& other) {
        assign(other.view());
    }

    SecureString& operator=(const SecureString& other) {
        if (this != &other) {
            SecureString copy(other);
            swap(copy);
        }
        return *this;
    }

    SecureString(SecureString&& other) noexcept
        : buffer_(std::move(other.buffer_)), size_(other.size_) {
        other.size_ = 0;
    }

    SecureString& operator=(SecureString&& other) noexcept {
        if (this != &other) {
            clear();
            buffer_ = std::move(other.buffer_);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }

    SecureString& operator=(std::string_view value) {
        assign(value);
        return *this;
    }

    ~SecureString() = default;

    bool empty() const {
        return size_ == 0;
    }

    std::size_t size() const {
        return size_;
    }

    std::string_view view() const {
        return {
            reinterpret_cast<const char*>(buffer_.data()),
            size_
        };
    }

    void clear() {
        buffer_.clear();
        size_ = 0;
    }

    friend bool operator==(
        const SecureString& value,
        std::string_view other
    ) {
        return value.view() == other;
    }

private:
    void assign(std::string_view value) {
        SecureBuffer replacement(value.size());
        if (value.size() != 0 && !replacement.valid()) {
            throw std::runtime_error(
                "Nao foi possivel proteger uma string sensivel."
            );
        }
        if (!value.empty()) {
            std::memcpy(replacement.data(), value.data(), value.size());
        }
        buffer_ = std::move(replacement);
        size_ = value.size();
    }

    void swap(SecureString& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(size_, other.size_);
    }

    SecureBuffer buffer_;
    std::size_t size_ = 0;
};

}
