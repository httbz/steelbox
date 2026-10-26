#pragma once

#include <string>

namespace steelbox {

enum class UrlError {
    None,
    Empty,
    UnsupportedScheme,
    MissingHost,
    InvalidHost
};

struct UrlResult {
    UrlError error = UrlError::None;
    std::string domain;
    std::string message;

    bool valid() const {
        return error == UrlError::None;
    }
};

UrlResult normalize_url(const std::string& url);

}
