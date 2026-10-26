#include <steelbox/models/url.hpp>

#include <algorithm>
#include <cctype>
#include <string_view>

namespace steelbox {

namespace {

bool has_whitespace_or_control(std::string_view value) {
    return std::any_of(
        value.begin(),
        value.end(),
        [](unsigned char character) {
            return std::isspace(character) != 0 ||
                   std::iscntrl(character) != 0;
        }
    );
}

std::string lowercase(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );

    return value;
}

}

UrlResult normalize_url(const std::string& url) {
    if (url.empty()) {
        return {
            UrlError::Empty,
            "",
            "A URL e obrigatoria."
        };
    }

    if (has_whitespace_or_control(url)) {
        return {
            UrlError::InvalidHost,
            "",
            "A URL nao pode conter espacos ou caracteres de controle."
        };
    }

    std::size_t authority_start = 0;
    if (url.starts_with("https://")) {
        authority_start = 8;
    } else if (url.starts_with("http://")) {
        authority_start = 7;
    } else {
        return {
            UrlError::UnsupportedScheme,
            "",
            "A URL deve usar http:// ou https://."
        };
    }

    const std::size_t authority_end = url.find_first_of("/?#", authority_start);
    const std::string authority = url.substr(
        authority_start,
        authority_end == std::string::npos
            ? std::string::npos
            : authority_end - authority_start
    );

    if (authority.empty() || authority.find('@') != std::string::npos) {
        return {
            UrlError::MissingHost,
            "",
            "A URL deve conter um host valido."
        };
    }

    std::string host = authority;
    if (host.front() == '[') {
        const std::size_t closing_bracket = host.find(']');
        if (closing_bracket == std::string::npos) {
            return {
                UrlError::InvalidHost,
                "",
                "O host IPv6 e invalido."
            };
        }

        host = host.substr(1, closing_bracket - 1);
        if (closing_bracket + 1 < authority.size() &&
            authority[closing_bracket + 1] != ':') {
            return {
                UrlError::InvalidHost,
                "",
                "O host da URL e invalido."
            };
        }
    } else {
        const std::size_t port_separator = host.find(':');
        if (port_separator != std::string::npos) {
            host = host.substr(0, port_separator);
        }
    }

    if (host.empty() || host.front() == '.' || host.back() == '.') {
        return {
            UrlError::InvalidHost,
            "",
            "O host da URL e invalido."
        };
    }

    host = lowercase(host);
    if (host.starts_with("www.")) {
        host.erase(0, 4);
    }

    return {
        UrlError::None,
        host,
        ""
    };
}

}
