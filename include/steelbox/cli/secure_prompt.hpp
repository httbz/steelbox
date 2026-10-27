#pragma once

#include <string>

namespace steelbox::cli {

bool read_hidden_input(
    const std::string& prompt,
    std::string& value,
    std::string& error
);

}