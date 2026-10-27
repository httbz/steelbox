#pragma once

#include <sodium.h>

#include <cstddef>
#include <cstring>

namespace steelbox {

class SecureBuffer {
public:
    SecureBuffer() = default;

    explicit SecureBuffer(std::size_t size)
        : data_(allocate(size)),
          size_(size) {
        if (data_ == nullptr || sodium_mlock(data_, size_) != 0) {
            if (data_ != nullptr) {
                sodium_free(data_);
            }
            data_ = nullptr;
            return;
        }
        std::memset(data_, 0, size_);
    }

    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    SecureBuffer(SecureBuffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    SecureBuffer& operator=(SecureBuffer&& other) noexcept {
        if (this != &other) {
            clear();
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    ~SecureBuffer() {
        clear();
    }

    bool valid() const {
        return data_ != nullptr || size_ == 0;
    }

    unsigned char* data() {
        return data_;
    }

    const unsigned char* data() const {
        return data_;
    }

    std::size_t size() const {
        return size_;
    }

    void clear() {
        if (data_ != nullptr) {
            sodium_memzero(data_, size_);
            sodium_munlock(data_, size_);
            sodium_free(data_);
            data_ = nullptr;
        }
        size_ = 0;
    }

private:
    static unsigned char* allocate(std::size_t size) {
        if (size == 0 || sodium_init() < 0) {
            return nullptr;
        }
        return static_cast<unsigned char*>(sodium_malloc(size));
    }

    unsigned char* data_ = nullptr;
    std::size_t size_ = 0;
};

}
