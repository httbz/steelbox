#include <steelbox/cli/secure_prompt.hpp>

#include <sodium.h>

#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace steelbox::cli {

namespace {

constexpr std::size_t maximum_secret_size = 4096;

void wipe(std::string& value) {
    if (!value.empty()) {
        sodium_memzero(value.data(), value.size());
    }
    value.clear();
}

#if defined(_WIN32)

struct WideBufferWiper {
    std::vector<wchar_t>& value;

    ~WideBufferWiper() {
        if (!value.empty()) {
            SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
        }
    }
};

#else

class TerminalEchoGuard {
public:
    explicit TerminalEchoGuard(int descriptor) : descriptor_(descriptor) {
        if (tcgetattr(descriptor_, &original_) != 0) {
            return;
        }

        termios hidden = original_;
        hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
        active_ = tcsetattr(descriptor_, TCSAFLUSH, &hidden) == 0;
    }

    ~TerminalEchoGuard() {
        restore();
    }

    bool active() const {
        return active_;
    }

    bool restore() {
        if (!active_) {
            return true;
        }
        active_ = false;
        return tcsetattr(descriptor_, TCSAFLUSH, &original_) == 0;
    }

private:
    int descriptor_;
    termios original_{};
    bool active_ = false;
};

#endif

}

bool read_hidden_input(
    const std::string& prompt,
    std::string& value,
    std::string& error
) {
    wipe(value);

#if defined(_WIN32)
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD original_mode = 0;
    std::vector<wchar_t> wide_value(maximum_secret_size);
    const WideBufferWiper wide_value_wiper{wide_value};
    if (input == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(input, &original_mode)) {
        error = "Digite a senha em um terminal interativo.";
        return false;
    }
    if (!SetConsoleMode(input, original_mode & ~ENABLE_ECHO_INPUT)) {
        error = "Nao foi possivel ocultar a entrada da senha.";
        return false;
    }

    std::cout << prompt << std::flush;
    DWORD characters_read = 0;
    const BOOL read_succeeded = ReadConsoleW(
        input,
        wide_value.data(),
        static_cast<DWORD>(wide_value.size()),
        &characters_read,
        nullptr
    );
    const DWORD read_error = GetLastError();
    const BOOL restored = SetConsoleMode(input, original_mode);
    std::cout << '\n' << std::flush;

    if (!restored) {
        error = "Nao foi possivel restaurar o modo do terminal.";
        return false;
    }
    if (!read_succeeded) {
        error = "Nao foi possivel ler a senha do terminal (erro " +
            std::to_string(read_error) + ").";
        return false;
    }
    if (characters_read >= wide_value.size()) {
        FlushConsoleInputBuffer(input);
        error = "A senha excede o limite de 4096 caracteres.";
        return false;
    }

    while (characters_read > 0 &&
           (wide_value[characters_read - 1] == L'\r' ||
            wide_value[characters_read - 1] == L'\n')) {
        --characters_read;
    }
    if (characters_read == 0) {
        return true;
    }

    const int utf8_size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide_value.data(),
        static_cast<int>(characters_read),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (utf8_size <= 0) {
        error = "A senha contem caracteres invalidos para UTF-8.";
        return false;
    }
    value.resize(static_cast<std::size_t>(utf8_size));
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            wide_value.data(),
            static_cast<int>(characters_read),
            value.data(),
            utf8_size,
            nullptr,
            nullptr
        ) != utf8_size) {
        wipe(value);
        error = "Nao foi possivel converter a senha para UTF-8.";
        return false;
    }
    return true;
#else
    if (!isatty(STDIN_FILENO)) {
        error = "Digite a senha em um terminal interativo.";
        return false;
    }

    TerminalEchoGuard terminal_guard(STDIN_FILENO);
    if (!terminal_guard.active()) {
        error = "Nao foi possivel ocultar a entrada da senha.";
        return false;
    }

    std::cout << prompt << std::flush;
    bool too_long = false;
    while (true) {
        const int input_character = std::cin.get();
        if (input_character == std::char_traits<char>::eof()) {
            terminal_guard.restore();
            std::cout << '\n' << std::flush;
            wipe(value);
            error = "A leitura da senha foi cancelada.";
            return false;
        }
        if (input_character == '\n') {
            break;
        }
        if (input_character == '\r') {
            continue;
        }
        if (input_character == '\b' || input_character == 0x7f) {
            if (!too_long && !value.empty()) {
                sodium_memzero(&value.back(), 1);
                value.pop_back();
            }
            continue;
        }
        if (value.size() < maximum_secret_size) {
            value.push_back(static_cast<char>(input_character));
        } else {
            too_long = true;
        }
    }

    const bool restored = terminal_guard.restore();
    std::cout << '\n' << std::flush;
    if (!restored) {
        wipe(value);
        error = "Nao foi possivel restaurar o modo do terminal.";
        return false;
    }
    if (too_long) {
        wipe(value);
        error = "A senha excede o limite de 4096 bytes.";
        return false;
    }
    return true;
#endif
}

}