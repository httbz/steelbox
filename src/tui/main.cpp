#include <steelbox/crypto/password_generator.hpp>
#include <steelbox/models/vault.hpp>
#include <steelbox/storage/vault_storage.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

namespace {

using namespace ftxui;

enum class UiScreen {
    Login,
    Entries,
    Detail,
    EntryForm,
    ConfirmDelete,
    GeneratePassword,
    ChangeMaster
};

struct App {
    std::filesystem::path vault_path;
    std::string master_password;
    std::string login_password;
    std::string query;
    std::string pending_query;
    std::string error;
    std::string notice;
    std::string form_title;
    std::string form_username;
    std::string form_url;
    std::string form_password;
    std::string form_notes;
    std::string form_tags;
    std::string generated_password;
    std::string new_master_password;
    std::string confirm_master_password;
    std::string delete_confirmation;
    std::string generator_length = "20";
    steelbox::Vault vault;
    const steelbox::PasswordEntry* selected = nullptr;
    std::vector<std::string> entry_labels;
    int selected_index = 0;
    int active_component = 0;
    bool editing = false;
    bool form_password_visible = false;
    bool detail_password_visible = false;
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
    if (path.empty()) path = std::filesystem::current_path() / "steelbox.vault";
    if (path.is_relative()) path = std::filesystem::absolute(path);
    return path;
}

std::vector<const steelbox::PasswordEntry*> filtered_entries(const App& app) {
    if (app.query.empty()) {
        std::vector<const steelbox::PasswordEntry*> result;
        if (const auto* entries = app.vault.entries(); entries != nullptr) {
            for (const auto& entry : *entries) result.push_back(&entry);
        }
        return result;
    }
    return app.vault.search_entries(app.query);
}

bool select_current_entry(App& app) {
    const auto entries = filtered_entries(app);
    if (app.selected_index < 0 ||
        static_cast<std::size_t>(app.selected_index) >= entries.size()) {
        app.selected = nullptr;
        return false;
    }
    app.selected = entries[static_cast<std::size_t>(app.selected_index)];
    app.detail_password_visible = false;
    return true;
}

void refresh_entry_labels(App& app) {
    app.entry_labels.clear();
    for (const auto* entry : filtered_entries(app)) {
        app.entry_labels.push_back(
            (entry->title.empty() ? "(sem titulo)" : entry->title) +
            " | " + entry->username + " | " + entry->normalized_domain
        );
    }
    if (app.entry_labels.empty()) {
        app.selected_index = 0;
    } else if (app.selected_index >=
               static_cast<int>(app.entry_labels.size())) {
        app.selected_index = static_cast<int>(app.entry_labels.size()) - 1;
    }
}

void clear_form(App& app) {
    app.form_title.clear();
    app.form_username.clear();
    app.form_url.clear();
    app.form_password.clear();
    app.form_notes.clear();
    app.form_tags.clear();
}

void begin_new_entry(App& app) {
    clear_form(app);
    app.form_password_visible = false;
    app.editing = false;
    app.error.clear();
    app.notice.clear();
    app.screen = UiScreen::EntryForm;
    app.active_component = 3;
}

void begin_edit_entry(App& app) {
    if (app.selected == nullptr) return;
    app.form_title = app.selected->title;
    app.form_username = app.selected->username;
    app.form_url = app.selected->url;
    app.form_password = app.selected->password.view();
    app.form_notes = app.selected->notes;
    app.form_tags.clear();
    app.form_password_visible = false;
    for (std::size_t index = 0; index < app.selected->tags.size(); ++index) {
        if (index != 0) app.form_tags += ", ";
        app.form_tags += app.selected->tags[index];
    }
    app.editing = true;
    app.error.clear();
    app.notice.clear();
    app.screen = UiScreen::EntryForm;
    app.active_component = 3;
}

void begin_password_generation(App& app) {
    app.generated_password.clear();
    app.error.clear();
    app.screen = UiScreen::GeneratePassword;
    app.active_component = 5;
}

std::vector<std::string> parse_tags(const std::string& value) {
    std::vector<std::string> tags;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto comma = value.find(',', start);
        const auto end = comma == std::string::npos ? value.size() : comma;
        const auto first = value.find_first_not_of(" \t", start);
        const auto last = value.find_last_not_of(" \t", end == 0 ? 0 : end - 1);
        if (first != std::string::npos && first < end && last >= first) {
            tags.push_back(value.substr(first, last - first + 1));
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return tags;
}

void save_form(App& app) {
    steelbox::PasswordEntry entry;
    if (app.editing && app.selected != nullptr) entry = *app.selected;
    entry.title = app.form_title;
    entry.username = app.form_username;
    entry.url = app.form_url;
    entry.password = app.form_password;
    entry.notes = app.form_notes;
    entry.tags = parse_tags(app.form_tags);

    const auto result = app.editing
        ? app.vault.update_entry(entry)
        : app.vault.add_entry(std::move(entry));
    if (!result.succeeded()) {
        app.error = result.message;
        return;
    }
    const auto saved = steelbox::save_vault(
        app.vault, app.vault_path, app.master_password
    );
    if (!saved.succeeded) {
        app.error = saved.message;
        return;
    }
    app.selected = nullptr;
    app.notice = app.editing ? "Entrada atualizada." : "Entrada criada.";
    app.error.clear();
    refresh_entry_labels(app);
    app.screen = UiScreen::Entries;
    app.active_component = 1;
}

Component login_screen(App& app, ScreenInteractive& screen) {
    auto password = Input(
        &app.login_password, "Senha mestra", InputOption{.password = true}
    );
    auto unlock = Button("Desbloquear", [&] {
        const auto result = steelbox::load_vault(
            app.vault, app.vault_path, app.login_password
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
            password->Render(), unlock->Render(), quit->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center;
    });
}

Component entries_screen(App& app, ScreenInteractive& screen) {
    auto search = Input(
        &app.pending_query, "Buscar por titulo, usuario ou dominio"
    );
    auto execute_search = Button("\xF0\x9F\x94\x8D Buscar", [&] {
        app.query = app.pending_query;
        app.selected_index = 0;
        app.selected = nullptr;
        refresh_entry_labels(app);
        app.notice = app.query.empty() ? "Busca limpa." : "Busca executada.";
    });
    auto menu = Menu(&app.entry_labels, &app.selected_index);
    auto view = Button("Visualizar", [&] {
        if (select_current_entry(app)) {
            app.screen = UiScreen::Detail;
            app.active_component = 2;
        }
    });
    auto add = Button("Nova entrada", [&] { begin_new_entry(app); });
    auto edit = Button("Editar", [&] {
        if (select_current_entry(app)) begin_edit_entry(app);
    });
    auto remove = Button("Excluir", [&] {
        if (select_current_entry(app)) {
            app.delete_confirmation.clear();
            app.error.clear();
            app.screen = UiScreen::ConfirmDelete;
            app.active_component = 4;
        }
    });
    auto lock = Button("Bloquear", [&] {
        app.vault.lock();
        app.master_password.clear();
        app.selected = nullptr;
        app.screen = UiScreen::Login;
        app.active_component = 0;
    });
    auto change_master = Button("Alterar senha mestra", [&] {
        app.new_master_password.clear();
        app.confirm_master_password.clear();
        app.error.clear();
        app.screen = UiScreen::ChangeMaster;
        app.active_component = 6;
    });
    auto quit = Button("Sair", [&] {
        app.vault.lock();
        app.master_password.clear();
        screen.Exit();
    });
    auto controls = Container::Vertical({
        search, execute_search, menu, view, add, edit, remove, lock,
        change_master, quit
    });
    return Renderer(controls, [&, search, execute_search, menu, view, add,
                               edit, remove, lock, change_master, quit] {
        const auto entries = filtered_entries(app);
        return vbox({
            text("Entradas") | bold, text("Busca:"),
            hbox({search->Render(), execute_search->Render()}),
            separator(), menu->Render() | flex, separator(),
            text("Entradas encontradas: " + std::to_string(entries.size())),
            view->Render(), add->Render(), edit->Render(), remove->Render(),
            lock->Render(), change_master->Render(), quit->Render(),
            app.notice.empty() ? text("") : text(app.notice) | color(Color::Green)
        }) | border;
    });
}

Component detail_screen(App& app) {
    auto back = Button("Voltar", [&] {
        app.selected = nullptr;
        app.screen = UiScreen::Entries;
        app.active_component = 1;
    });
    auto edit = Button("Editar", [&] {
        if (select_current_entry(app)) begin_edit_entry(app);
    });
    auto remove = Button("Excluir", [&] {
        if (select_current_entry(app)) {
            app.delete_confirmation.clear();
            app.screen = UiScreen::ConfirmDelete;
            app.active_component = 4;
        }
    });
    auto reveal = Button("Visualizar senha", [&] {
        app.detail_password_visible = !app.detail_password_visible;
    });
    auto controls = Container::Vertical({reveal, edit, remove, back});
    return Renderer(controls, [&, reveal, edit, remove, back] {
        if (app.selected == nullptr) return text("Nenhuma entrada selecionada.") | border;
        const auto& entry = *app.selected;
        return vbox({
            text("Visualizacao da entrada") | bold, separator(),
            text("ID: " + entry.id), text("Titulo: " + entry.title),
            text("Usuario: " + entry.username), text("URL: " + entry.url),
            text("Dominio: " + entry.normalized_domain),
            text("Tags: " + std::to_string(entry.tags.size())),
            text("Notas: " + entry.notes),
            text(app.detail_password_visible
                ? std::string("Senha: ") + std::string(entry.password.view())
                : "Senha: ********"),
            separator(), reveal->Render(), edit->Render(), remove->Render(),
            back->Render()
        }) | border;
    });
}

Component entry_form_screen(App& app) {
    auto title = Input(&app.form_title, "Titulo");
    auto username = Input(&app.form_username, "Usuario");
    auto url = Input(&app.form_url, "https://exemplo.com");
    InputOption password_options;
    password_options.placeholder = "Senha";
    password_options.password = &app.form_password_visible;
    auto password = Input(&app.form_password, password_options);
    auto notes = Input(&app.form_notes, "Notas");
    auto tags = Input(&app.form_tags, "tag1, tag2");
    auto generate = Button("Gerar senha", [&] {
        begin_password_generation(app);
    });
    auto reveal = Button("Visualizar senha", [&] {
        app.form_password_visible = !app.form_password_visible;
    });
    auto save = Button("Salvar", [&] { save_form(app); });
    auto cancel = Button("Cancelar", [&] {
        app.error.clear();
        app.screen = app.selected == nullptr ? UiScreen::Entries : UiScreen::Detail;
        app.active_component = app.screen == UiScreen::Entries ? 1 : 2;
    });
    auto controls = Container::Vertical({
        title, username, url, password, reveal, generate, notes, tags, save,
        cancel
    });
    return Renderer(controls, [&, title, username, url, password, reveal,
                               generate, notes, tags, save, cancel] {
        Elements content = {
            text(app.editing ? "Editar entrada" : "Nova entrada") | bold,
            separator(), text("Titulo:"), title->Render(),
            text("Usuario:"), username->Render(), text("URL:"), url->Render(),
            text("Senha:"), hbox({password->Render(), reveal->Render()}),
            generate->Render(),
            text("Notas:"), notes->Render(), text("Tags:"), tags->Render(),
            save->Render(), cancel->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border;
    });
}

Component delete_screen(App& app) {
    auto confirm = Button("Excluir definitivamente", [&] {
        if (app.delete_confirmation != "EXCLUIR") {
            app.error = "Digite EXCLUIR para confirmar.";
            return;
        }
        if (app.selected == nullptr) return;
        const auto result = app.vault.remove_entry(app.selected->id);
        if (!result.succeeded()) {
            app.error = result.message;
            return;
        }
        const auto saved = steelbox::save_vault(
            app.vault, app.vault_path, app.master_password
        );
        if (!saved.succeeded) {
            app.error = saved.message;
            return;
        }
        app.selected = nullptr;
        app.notice = "Entrada excluida.";
        refresh_entry_labels(app);
        app.screen = UiScreen::Entries;
        app.active_component = 1;
    });
    auto confirmation = Input(&app.delete_confirmation, "EXCLUIR");
    auto cancel = Button("Cancelar", [&] {
        app.error.clear();
        app.screen = app.selected == nullptr ? UiScreen::Entries : UiScreen::Detail;
        app.active_component = app.screen == UiScreen::Entries ? 1 : 2;
    });
    auto controls = Container::Vertical({confirmation, confirm, cancel});
    return Renderer(controls, [&, confirmation, confirm, cancel] {
        Elements content = {
            text("Confirmar exclusao") | bold, separator(),
            text("Esta operacao nao pode ser desfeita."),
            text("Digite EXCLUIR para continuar."),
            confirmation->Render(), confirm->Render(), cancel->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center;
    });
}

Component generator_screen(App& app) {
    auto length = Input(&app.generator_length, "20");
    auto generate = Button("Gerar", [&] {
        try {
            const auto value = std::stoul(app.generator_length);
            steelbox::PasswordGeneratorOptions options;
            options.length = value;
            const auto result = steelbox::generate_password(options);
            if (!result.succeeded) app.error = result.message;
            else {
                app.generated_password = result.password;
                app.error.clear();
            }
        } catch (const std::exception&) {
            app.error = "Informe um comprimento numerico entre 4 e 1024.";
        }
    });
    auto use = Button("Usar na entrada", [&] {
        if (app.generated_password.empty()) {
            app.error = "Gere uma senha antes de usa-la.";
            return;
        }
        app.form_password = app.generated_password;
        app.screen = UiScreen::EntryForm;
        app.active_component = 3;
    });
    auto cancel = Button("Cancelar", [&] {
        app.screen = UiScreen::EntryForm;
        app.active_component = 3;
    });
    auto controls = Container::Vertical({length, generate, use, cancel});
    return Renderer(controls, [&, length, generate, use, cancel] {
        Elements content = {
            text("Gerar senha") | bold, separator(),
            text("Comprimento (4-1024):"), length->Render(),
            generate->Render(),
            text(app.generated_password.empty()
                ? "Nenhuma senha gerada."
                : "Senha gerada: " + app.generated_password),
            use->Render(), cancel->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center;
    });
}

Component change_master_screen(App& app) {
    auto password = Input(
        &app.new_master_password, "Nova senha", InputOption{.password = true}
    );
    auto confirmation = Input(
        &app.confirm_master_password, "Confirmacao", InputOption{.password = true}
    );
    auto save = Button("Alterar senha", [&] {
        if (app.new_master_password.empty() ||
            app.new_master_password != app.confirm_master_password) {
            app.error = "As senhas devem ser preenchidas e iguais.";
            return;
        }
        const auto result = steelbox::save_vault(
            app.vault, app.vault_path, app.new_master_password
        );
        if (!result.succeeded) {
            app.error = result.message;
            return;
        }
        app.master_password = std::move(app.new_master_password);
        app.confirm_master_password.clear();
        app.notice = "Senha mestra alterada.";
        app.error.clear();
        app.screen = UiScreen::Entries;
        app.active_component = 1;
    });
    auto cancel = Button("Cancelar", [&] {
        app.error.clear();
        app.screen = UiScreen::Entries;
        app.active_component = 1;
    });
    auto controls = Container::Vertical({password, confirmation, save, cancel});
    return Renderer(controls, [&, password, confirmation, save, cancel] {
        Elements content = {
            text("Alterar senha mestra") | bold, separator(),
            text("Nova senha:"), password->Render(),
            text("Confirme a nova senha:"), confirmation->Render(),
            save->Render(), cancel->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center;
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
    auto form = entry_form_screen(app);
    auto deletion = delete_screen(app);
    auto generator = generator_screen(app);
    auto change_master = change_master_screen(app);
    auto root = ftxui::Container::Tab(
        {login, entries, detail, form, deletion, generator, change_master},
        &app.active_component
    );
    root = ftxui::CatchEvent(root, [&](const ftxui::Event& event) {
        if (app.screen == UiScreen::Entries) refresh_entry_labels(app);
        if (event == ftxui::Event::Escape) {
            if (app.screen == UiScreen::Detail) {
                app.selected = nullptr;
                app.screen = UiScreen::Entries;
                app.active_component = 1;
                return true;
            }
            if (app.screen == UiScreen::Entries) {
                app.vault.lock();
                screen.Exit();
                return true;
            }
        }
        if (event == ftxui::Event::Character("n") &&
            app.screen == UiScreen::Entries) {
            begin_new_entry(app);
            return true;
        }
        if (event == ftxui::Event::Character("e") &&
            app.screen == UiScreen::Entries) {
            if (select_current_entry(app)) begin_edit_entry(app);
            return true;
        }
        if (event == ftxui::Event::Character("d") &&
            app.screen == UiScreen::Entries) {
            if (select_current_entry(app)) {
                app.delete_confirmation.clear();
                app.error.clear();
                app.screen = UiScreen::ConfirmDelete;
                app.active_component = 4;
            }
            return true;
        }
        if (event == ftxui::Event::Character("v") &&
            app.screen == UiScreen::Entries) {
            if (select_current_entry(app)) {
                app.screen = UiScreen::Detail;
                app.active_component = 2;
            }
            return true;
        }
        if (event == ftxui::Event::Character("s") &&
            app.screen == UiScreen::Entries) {
            app.query = app.pending_query;
            app.selected_index = 0;
            app.selected = nullptr;
            refresh_entry_labels(app);
            app.notice = app.query.empty() ? "Busca limpa." : "Busca executada.";
            return true;
        }
        if (event == ftxui::Event::Character("j") &&
            app.screen == UiScreen::Entries &&
            !app.entry_labels.empty()) {
            app.selected_index = std::min(
                app.selected_index + 1,
                static_cast<int>(app.entry_labels.size()) - 1
            );
            return true;
        }
        if (event == ftxui::Event::Character("k") &&
            app.screen == UiScreen::Entries &&
            !app.entry_labels.empty()) {
            app.selected_index = std::max(app.selected_index - 1, 0);
            return true;
        }
        if ((event == ftxui::Event::Special("\x0e") ||
             event == ftxui::Event::ArrowDownCtrl) &&
            app.screen == UiScreen::Entries &&
            !app.entry_labels.empty()) {
            app.selected_index = std::min(
                app.selected_index + 1,
                static_cast<int>(app.entry_labels.size()) - 1
            );
            return true;
        }
        if ((event == ftxui::Event::Special("\x10") ||
             event == ftxui::Event::ArrowUpCtrl) &&
            app.screen == UiScreen::Entries &&
            !app.entry_labels.empty()) {
            app.selected_index = std::max(app.selected_index - 1, 0);
            return true;
        }
        if (event == ftxui::Event::Character("q") &&
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
    app.form_password.clear();
    app.new_master_password.clear();
    app.confirm_master_password.clear();
    return 0;
}
