#include <steelbox/models/vault.hpp>
#include <steelbox/storage/vault_storage.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace ftxui;

enum class UiScreen {
    Login,
    Entries,
    Detail
};

struct App {
    std::filesystem::path vault_path;
    std::string master_password;
    std::string login_password;
    std::string query;
    std::string error;
    steelbox::Vault vault;
    const steelbox::PasswordEntry* selected = nullptr;
    std::vector<std::string> entry_labels;
    int selected_index = 0;
    int active_component = 0;
    UiScreen screen = UiScreen::Login;
};

std::filesystem::path parse_vault_path(
    int argc,
    char* argv[],
    std::string& error
) {
    std::filesystem::path path;
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) != "--vault" || index + 1 >= argc) {
            error = "Uso: steelbox_tui [--vault <caminho>]";
            return {};
        }
        path = std::filesystem::u8path(argv[++index]);
    }

    if (path.empty()) {
        path = std::filesystem::current_path() / "steelbox.vault";
    }
    if (path.is_relative()) {
        path = std::filesystem::absolute(path);
    }
    return path;
}

std::vector<const steelbox::PasswordEntry*> filtered_entries(
    const App& app
) {
    if (app.query.empty()) {
        std::vector<const steelbox::PasswordEntry*> result;
        if (const auto* entries = app.vault.entries(); entries != nullptr) {
            for (const auto& entry : *entries) {
                result.push_back(&entry);
            }
        }
        return result;
    }
    return app.vault.search_entries(app.query);
}

void refresh_entry_labels(App& app) {
    app.entry_labels.clear();
    for (const auto* entry : filtered_entries(app)) {
        app.entry_labels.push_back(
            (entry->title.empty() ? "(sem titulo)" : entry->title) +
            " | " + entry->username + " | " +
            entry->normalized_domain
        );
    }
    if (app.entry_labels.empty()) {
        app.selected_index = 0;
    } else if (app.selected_index >=
               static_cast<int>(app.entry_labels.size())) {
        app.selected_index =
            static_cast<int>(app.entry_labels.size()) - 1;
    }
}

Element entry_line(const steelbox::PasswordEntry& entry) {
    return hbox({
        text(entry.title.empty() ? "(sem titulo)" : entry.title),
        separator(),
        text(entry.username),
        separator(),
        text(entry.normalized_domain)
    });
}

Component login_screen(
    App& app,
    ScreenInteractive& screen
) {
    auto password = Input(
        &app.login_password,
        "Senha mestra",
        InputOption{.password = true}
    );
    auto unlock = Button("Desbloquear", [&] {
        const auto result = steelbox::load_vault(
            app.vault,
            app.vault_path,
            app.login_password
        );
        if (!result.succeeded) {
            app.error = result.message;
            return;
        }
        app.master_password = std::move(app.login_password);
        app.login_password.clear();
        app.error.clear();
        refresh_entry_labels(app);
        app.screen = UiScreen::Entries;
        app.active_component = 1;
    });
    auto quit = Button("Sair", [&] {
        app.vault.lock();
        screen.Exit();
    });

    auto form = Container::Vertical({password, unlock, quit});
    return Renderer(form, [&, password, unlock, quit] {
        Elements content = {
            text("STEELBOX - Cofre local"),
            separator(),
            text("Arquivo: " + app.vault_path.string()),
            text("Digite a senha mestra para desbloquear."),
            password->Render(),
            unlock->Render(),
            quit->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center;
    });
}

Component entries_screen(
    App& app,
    ScreenInteractive& screen
) {
    auto search = Input(&app.query, "Buscar por titulo, usuario ou dominio");
    auto menu = Menu(
        &app.entry_labels,
        &app.selected_index
    );
    auto view = Button("Visualizar", [&] {
        const auto entries = filtered_entries(app);
        if (app.selected_index >= 0 &&
            static_cast<std::size_t>(app.selected_index) < entries.size()) {
            app.selected = entries[
                static_cast<std::size_t>(app.selected_index)
            ];
            app.screen = UiScreen::Detail;
            app.active_component = 2;
        }
    });
    auto lock = Button("Bloquear", [&] {
        app.vault.lock();
        app.master_password.clear();
        app.screen = UiScreen::Login;
        app.active_component = 0;
    });
    auto quit = Button("Sair", [&] {
        app.vault.lock();
        app.master_password.clear();
        screen.Exit();
    });

    auto controls = Container::Vertical({search, menu, view, lock, quit});
    return Renderer(controls, [&, search, menu, view, lock, quit] {
        const auto entries = filtered_entries(app);
        return vbox({
            text("Entradas") | bold,
            text("Busca:"),
            search->Render(),
            separator(),
            menu->Render() | flex,
            separator(),
            text("Entradas encontradas: " + std::to_string(entries.size())),
            view->Render(),
            lock->Render(),
            quit->Render()
        }) | border;
    });
}

Component detail_screen(App& app) {
    auto back = Button("Voltar", [&] {
        app.selected = nullptr;
        app.screen = UiScreen::Entries;
        app.active_component = 1;
    });
    auto controls = Container::Vertical({back});
    return Renderer(controls, [&, back] {
        if (app.selected == nullptr) {
            return text("Nenhuma entrada selecionada.") | border;
        }
        const auto& entry = *app.selected;
        return vbox({
            text("Visualizacao da entrada") | bold,
            separator(),
            text("ID: " + entry.id),
            text("Titulo: " + entry.title),
            text("Usuario: " + entry.username),
            text("URL: " + entry.url),
            text("Dominio: " + entry.normalized_domain),
            text("Tags: " + std::to_string(entry.tags.size())),
            text("Notas: " + entry.notes),
            text("Senha: ********"),
            separator(),
            back->Render()
        }) | border;
    });
}

}

int main(int argc, char* argv[]) {
    App app;
    std::string argument_error;
    app.vault_path = parse_vault_path(argc, argv, argument_error);
    if (!argument_error.empty()) {
        std::cerr << argument_error << "\n";
        return 2;
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto login = login_screen(app, screen);
    auto entries = entries_screen(app, screen);
    auto detail = detail_screen(app);
    auto root = ftxui::Container::Tab(
        {login, entries, detail},
        &app.active_component
    );
    root = ftxui::CatchEvent(root, [&](const ftxui::Event& event) {
        if (app.screen == UiScreen::Entries) {
            refresh_entry_labels(app);
        }
        if (event == ftxui::Event::Escape &&
            app.screen == UiScreen::Detail) {
            app.selected = nullptr;
            app.screen = UiScreen::Entries;
            return true;
        }
        if (event == ftxui::Event::Escape &&
            app.screen == UiScreen::Entries) {
            app.vault.lock();
            screen.Exit();
            return true;
        }
        return false;
    });
    screen.Loop(root);
    app.vault.lock();
    app.master_password.clear();
    app.login_password.clear();
    return 0;
}
