#include <steelbox/crypto/password_generator.hpp>
#include <steelbox/models/vault.hpp>
#include <steelbox/storage/vault_storage.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace
{

    using namespace ftxui;

    enum class UiScreen
    {
        Login,
        Entries,
        Detail,
        EntryForm,
        ConfirmDelete,
        GeneratePassword,
        ChangeMaster,
        Help
    };

    struct App
    {
        std::filesystem::path vault_path;
        std::string master_password;
        std::string login_password;
        std::string query;
        std::string pending_query;
        std::string command_line;
        std::string command_hint = "Spc + E: Edit | Spc + D: Delete | Spc + N: New | Ctrl + Q: Quit | / : Search";
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
        bool generator_lowercase = true;
        bool generator_uppercase = true;
        bool generator_digits = true;
        bool generator_symbols = true;
        steelbox::Vault vault;
        const steelbox::PasswordEntry *selected = nullptr;
        std::vector<std::string> entry_labels;
        int selected_index = 0;
        int active_component = 0;
        bool editing = false;
        bool form_password_visible = false;
        bool detail_password_visible = false;
        bool command_prefix_active = false;
        bool search_mode = false;
        Component command_input;
        UiScreen return_screen = UiScreen::Entries;
        UiScreen screen = UiScreen::Login;
    };

    std::filesystem::path parse_vault_path(
        int argc,
        char *argv[],
        std::string &error)
    {
        std::filesystem::path path;
        for (int index = 1; index < argc; ++index)
        {
            if (std::string(argv[index]) != "--vault" || index + 1 >= argc)
            {
                error = "Uso: steelbox_tui [--vault <caminho>]";
                return {};
            }
            path = std::filesystem::u8path(argv[++index]);
        }
        if (path.empty())
            path = std::filesystem::current_path() / "steelbox.vault";
        if (path.is_relative())
            path = std::filesystem::absolute(path);
        return path;
    }

    std::vector<const steelbox::PasswordEntry *> filtered_entries(const App &app)
    {
        if (app.query.empty())
        {
            std::vector<const steelbox::PasswordEntry *> result;
            if (const auto *entries = app.vault.entries(); entries != nullptr)
            {
                for (const auto &entry : *entries)
                    result.push_back(&entry);
            }
            return result;
        }
        return app.vault.search_entries(app.query);
    }

    bool select_current_entry(App &app)
    {
        const auto entries = filtered_entries(app);
        if (app.selected_index < 0 ||
            static_cast<std::size_t>(app.selected_index) >= entries.size())
        {
            app.selected = nullptr;
            return false;
        }
        app.selected = entries[static_cast<std::size_t>(app.selected_index)];
        app.detail_password_visible = false;
        return true;
    }

    void move_selection(App &app, int delta)
    {
        if (app.entry_labels.empty())
            return;
        app.selected_index = std::clamp(
            app.selected_index + delta,
            0,
            static_cast<int>(app.entry_labels.size()) - 1);
    }

    void refresh_entry_labels(App &app)
    {
        app.entry_labels.clear();
        for (const auto *entry : filtered_entries(app))
        {
            app.entry_labels.push_back(
                (entry->title.empty() ? "(sem titulo)" : entry->title) +
                " | " + entry->username + " | " + entry->normalized_domain);
        }
        if (app.entry_labels.empty())
        {
            app.selected_index = 0;
        }
        else if (app.selected_index >=
                 static_cast<int>(app.entry_labels.size()))
        {
            app.selected_index = static_cast<int>(app.entry_labels.size()) - 1;
        }
    }

    void clear_form(App &app)
    {
        app.form_title.clear();
        app.form_username.clear();
        app.form_url.clear();
        app.form_password.clear();
        app.form_notes.clear();
        app.form_tags.clear();
    }

    void begin_new_entry(App &app)
    {
        clear_form(app);
        app.form_password_visible = false;
        app.editing = false;
        app.error.clear();
        app.notice.clear();
        app.return_screen = UiScreen::Entries;
        app.screen = UiScreen::EntryForm;
        app.active_component = 3;
    }

    void begin_edit_entry(App &app)
    {
        if (app.selected == nullptr)
            return;
        app.return_screen = app.screen;
        app.form_title = app.selected->title;
        app.form_username = app.selected->username;
        app.form_url = app.selected->url;
        app.form_password = app.selected->password.view();
        app.form_notes = app.selected->notes;
        app.form_tags.clear();
        app.form_password_visible = false;
        for (std::size_t index = 0; index < app.selected->tags.size(); ++index)
        {
            if (index != 0)
                app.form_tags += ", ";
            app.form_tags += app.selected->tags[index];
        }
        app.editing = true;
        app.error.clear();
        app.notice.clear();
        app.screen = UiScreen::EntryForm;
        app.active_component = 3;
    }

    void begin_password_generation(App &app)
    {
        app.generated_password.clear();
        app.error.clear();
        app.screen = UiScreen::GeneratePassword;
        app.active_component = 5;
    }

    std::string lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string trim(std::string value)
    {
        const auto first = value.find_first_not_of(" \t");
        if (first == std::string::npos)
            return {};
        const auto last = value.find_last_not_of(" \t");
        return value.substr(first, last - first + 1);
    }

    void open_delete_confirmation(App &app)
    {
        if (!select_current_entry(app))
        {
            app.error = "Selecione uma entrada primeiro.";
            return;
        }
        app.delete_confirmation.clear();
        app.error.clear();
        app.return_screen = app.screen;
        app.screen = UiScreen::ConfirmDelete;
        app.active_component = 4;
    }

    void show_selected_entry(App &app)
    {
        if (!select_current_entry(app))
        {
            app.error = "Selecione uma entrada primeiro.";
            return;
        }
        app.screen = UiScreen::Detail;
        app.active_component = 2;
    }

    void open_change_master(App &app)
    {
        app.new_master_password.clear();
        app.confirm_master_password.clear();
        app.error.clear();
        app.screen = UiScreen::ChangeMaster;
        app.active_component = 6;
    }

    void open_help(App &app)
    {
        app.return_screen = app.screen;
        app.error.clear();
        app.screen = UiScreen::Help;
        app.active_component = 7;
    }

    void lock_vault(App &app)
    {
        app.vault.lock();
        app.master_password.clear();
        app.selected = nullptr;
        app.screen = UiScreen::Login;
        app.active_component = 0;
    }

    void go_back(App &app, ScreenInteractive &screen)
    {
        if (app.screen == UiScreen::Detail ||
            app.screen == UiScreen::EntryForm ||
            app.screen == UiScreen::ConfirmDelete ||
            app.screen == UiScreen::GeneratePassword ||
            app.screen == UiScreen::ChangeMaster ||
            app.screen == UiScreen::Help)
        {
            if (app.screen == UiScreen::GeneratePassword)
            {
                app.screen = UiScreen::EntryForm;
                app.active_component = 3;
                return;
            }
            app.screen = app.return_screen;
            app.active_component =
                app.screen == UiScreen::Entries ? 1 : app.screen == UiScreen::Detail ? 2
                                                                                     : 1;
            app.error.clear();
            return;
        }
        if (app.screen == UiScreen::Entries)
        {
            app.vault.lock();
            app.master_password.clear();
            screen.Exit();
        }
    }

    void change_vault(App &app, const std::string &value)
    {
        if (value.empty())
        {
            app.error = "Informe o caminho do novo cofre.";
            return;
        }
        auto path = std::filesystem::u8path(value);
        if (path.is_relative())
            path = std::filesystem::absolute(path);
        app.vault.lock();
        app.master_password.clear();
        app.login_password.clear();
        app.vault_path = std::move(path);
        app.query.clear();
        app.pending_query.clear();
        app.selected = nullptr;
        app.screen = UiScreen::Login;
        app.active_component = 0;
    }

    void execute_command(App &app, ScreenInteractive &screen)
    {
        const std::string command = trim(app.command_line);
        app.command_line.clear();
        app.error.clear();
        app.command_hint = "Spc + E: Edit | Spc + D: Delete | Spc + N: New | Ctrl + Q: Quit | / : Search";
        if (app.search_mode)
        {
            app.search_mode = false;
            app.query = command;
            app.pending_query = command;
            app.selected_index = 0;
            app.selected = nullptr;
            refresh_entry_labels(app);
            app.notice = command.empty() ? std::string() : "Busca executada.";
            return;
        }
        if (command.empty())
            return;

        const auto separator = command.find_first_of(" \t");
        const std::string name = lowercase(command.substr(0, separator));
        const std::string argument = separator == std::string::npos
                                         ? std::string()
                                         : trim(command.substr(separator + 1));

        if (name == "edit" || name == "e")
        {
            if (select_current_entry(app))
            {
                begin_edit_entry(app);
                app.notice = "Editando a entrada selecionada.";
            }
            else
            {
                app.error = "Selecione uma entrada primeiro.";
            }
        }
        else if (name == "delete" || name == "remove" || name == "rm" || name == "d")
        {
            open_delete_confirmation(app);
        }
        else if (name == "new" || name == "add" || name == "n")
        {
            begin_new_entry(app);
            app.notice = "Nova entrada.";
        }
        else if (name == "view" || name == "open" || name == "v")
        {
            show_selected_entry(app);
            if (app.screen == UiScreen::Detail)
                app.notice = "Visualizando a entrada.";
        }
        else if (name == "search" || name == "find" || name == "s" || name == "/")
        {
            app.query = argument;
            app.pending_query = argument;
            app.selected_index = 0;
            app.selected = nullptr;
            refresh_entry_labels(app);
            app.notice = argument.empty() ? std::string() : "Busca executada.";
        }
        else if (name == "clear")
        {
            app.query.clear();
            app.pending_query.clear();
            app.selected_index = 0;
            app.selected = nullptr;
            refresh_entry_labels(app);
            app.notice.clear();
        }
        else if (name == "change-master" || name == "master")
        {
            open_change_master(app);
            app.notice = "Alteracao da senha mestra.";
        }
        else if (name == "vault" || name == "change-vault" || name == "open-vault")
        {
            change_vault(app, argument);
        }
        else if (name == "lock")
        {
            lock_vault(app);
            app.notice = "Cofre bloqueado.";
        }
        else if (name == "quit" || name == "exit")
        {
            app.vault.lock();
            app.master_password.clear();
            screen.Exit();
        }
        else if (name == "help" || name == "?")
        {
            open_help(app);
        }
        else
        {
            app.error = "Comando desconhecido. Use 'help' para ver os comandos.";
        }
        if (app.error.empty() && app.notice.empty() &&
            app.screen == UiScreen::Entries)
        {
            app.notice = "Comando executado: " + name;
        }
    }

    std::vector<std::string> parse_tags(const std::string &value)
    {
        std::vector<std::string> tags;
        std::size_t start = 0;
        while (start < value.size())
        {
            const auto comma = value.find(',', start);
            const auto end = comma == std::string::npos ? value.size() : comma;
            const auto first = value.find_first_not_of(" \t", start);
            const auto last = value.find_last_not_of(" \t", end == 0 ? 0 : end - 1);
            if (first != std::string::npos && first < end && last >= first)
            {
                tags.push_back(value.substr(first, last - first + 1));
            }
            if (comma == std::string::npos)
                break;
            start = comma + 1;
        }
        return tags;
    }

    void save_form(App &app)
    {
        steelbox::PasswordEntry entry;
        if (app.editing && app.selected != nullptr)
            entry = *app.selected;
        entry.title = app.form_title;
        entry.username = app.form_username;
        entry.url = app.form_url;
        entry.password = app.form_password;
        entry.notes = app.form_notes;
        entry.tags = parse_tags(app.form_tags);

        const auto result = app.editing
                                ? app.vault.update_entry(entry)
                                : app.vault.add_entry(std::move(entry));
        if (!result.succeeded())
        {
            app.error = result.message;
            return;
        }
        const auto saved = steelbox::save_vault(
            app.vault, app.vault_path, app.master_password);
        if (!saved.succeeded)
        {
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

    Component login_screen(App &app, ScreenInteractive &screen)
    {
        auto password = Input(
            &app.login_password, "Senha mestra", InputOption{.password = true});
        auto unlock = Button("Desbloquear", [&]
                             {
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
        app.active_component = 1; });
        auto quit = Button("Sair", [&]
                           {
        app.vault.lock();
        screen.Exit(); });
        auto form = Container::Vertical({password, unlock, quit});
        return Renderer(form, [&, password, unlock, quit]
                        {
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
        return vbox(content) | border | center; });
    }

    Component entries_screen(App &app, ScreenInteractive &screen)
    {
        auto menu = Menu(&app.entry_labels, &app.selected_index);
        InputOption command_options;
        command_options.placeholder = "Comando (help para ajuda)";
        command_options.on_enter = [&]
        { execute_command(app, screen); };
        auto command = Input(&app.command_line, command_options);
        command = CatchEvent(command, [&](const Event &event)
                             {
        if (event == Event::Return) {
            execute_command(app, screen);
            return true;
        }
        if (!app.command_line.empty()) return false;
        if (event == Event::Escape) {
            go_back(app, screen);
            return true;
        }
        if (event == Event::Character("j")) {
            move_selection(app, 1);
            return true;
        }
        if (event == Event::Character("k")) {
            move_selection(app, -1);
            return true;
        }
        if (event == Event::Character("/") ||
            event == Event::Special("\x06")) {
            app.search_mode = true;
            app.command_line.clear();
            app.notice = "Digite o termo da busca e pressione Enter.";
            app.command_input->TakeFocus();
            return true;
        }
        return false; });
        app.command_input = command;
        auto controls = Container::Vertical({menu, command});
        controls->SetActiveChild(command);
        return Renderer(controls, [&, menu, command]
                        {
        const auto entries = filtered_entries(app);
        Elements content = {
            text("Entradas") | bold,
            text(app.query.empty()
                ? "Busca: todas as entradas"
                : "Busca: " + app.query),
            separator(), menu->Render() | flex, separator()};
        if (!app.notice.empty())
            content.push_back(text(app.notice) | color(Color::Green));
        if (!app.error.empty())
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        content.push_back(
            text("Entradas encontradas: " + std::to_string(entries.size())) |
            color(Color::GrayDark) | dim);
        content.push_back(separator());
        content.push_back(text(app.command_hint) | color(Color::GrayLight));
        content.push_back(hbox({text("> "), command->Render()}));
        return vbox(content) | border; });
    }

    Component detail_screen(App &app)
    {
        auto back = Button("Voltar", [&]
                           {
        app.selected = nullptr;
        app.screen = UiScreen::Entries;
        app.active_component = 1; });
        auto edit = Button("Editar", [&]
                           {
        if (select_current_entry(app)) begin_edit_entry(app); });
        auto remove = Button("Excluir", [&]
                             {
        if (select_current_entry(app)) {
            app.delete_confirmation.clear();
            app.screen = UiScreen::ConfirmDelete;
            app.active_component = 4;
        } });
        auto reveal = Button("Visualizar senha", [&]
                             { app.detail_password_visible = !app.detail_password_visible; });
        auto controls = Container::Vertical({reveal, edit, remove, back});
        return Renderer(controls, [&, reveal, edit, remove, back]
                        {
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
        }) | border; });
    }

    Component entry_form_screen(App &app)
    {
        auto title = Input(&app.form_title, "Titulo");
        auto username = Input(&app.form_username, "Usuario");
        auto url = Input(&app.form_url, "https://exemplo.com");
        InputOption password_options;
        password_options.placeholder = "Senha";
        password_options.password = &app.form_password_visible;
        auto password = Input(&app.form_password, password_options);
        auto notes = Input(&app.form_notes, "Notas");
        auto tags = Input(&app.form_tags, "tag1, tag2");
        auto generate = Button("Gerar senha", [&]
                               { begin_password_generation(app); });
        auto reveal = Button("Visualizar senha", [&]
                             { app.form_password_visible = !app.form_password_visible; });
        auto save = Button("Salvar", [&]
                           { save_form(app); });
        auto cancel = Button("Cancelar", [&]
                             {
        app.error.clear();
        app.screen = app.return_screen;
        app.active_component = app.screen == UiScreen::Entries ? 1 : 2; });
        auto controls = Container::Vertical({title, username, url, password, reveal, generate, notes, tags, save,
                                             cancel});
        return Renderer(controls, [&, title, username, url, password, reveal,
                                   generate, notes, tags, save, cancel]
                        {
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
        return vbox(content) | border; });
    }

    Component delete_screen(App &app)
    {
        auto confirm = Button("Excluir definitivamente", [&]
                              {
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
        app.active_component = 1; });
        auto confirmation = Input(&app.delete_confirmation, "EXCLUIR");
        auto cancel = Button("Cancelar", [&]
                             {
        app.error.clear();
        app.screen = app.return_screen;
        app.active_component = app.screen == UiScreen::Entries ? 1 : 2; });
        auto controls = Container::Vertical({confirmation, confirm, cancel});
        return Renderer(controls, [&, confirmation, confirm, cancel]
                        {
        Elements content = {
            text("Confirmar exclusao") | bold, separator(),
            text("Esta operacao nao pode ser desfeita."),
            text("Digite EXCLUIR para continuar."),
            confirmation->Render(), confirm->Render(), cancel->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center; });
    }

    Component generator_screen(App &app)
    {
        auto length = Input(&app.generator_length, "20");
        auto lowercase = Checkbox("Letras minusculas (a-z)", &app.generator_lowercase);
        auto uppercase = Checkbox("Letras maiusculas (A-Z)", &app.generator_uppercase);
        auto digits = Checkbox("Numeros (0-9)", &app.generator_digits);
        auto symbols = Checkbox("Simbolos (!@#$...)", &app.generator_symbols);
        auto generate = Button("Gerar", [&]
                               {
        try {
            const auto value = std::stoul(app.generator_length);
            steelbox::PasswordGeneratorOptions options;
            options.length = value;
            options.lowercase = app.generator_lowercase;
            options.uppercase = app.generator_uppercase;
            options.digits = app.generator_digits;
            options.symbols = app.generator_symbols;
            const auto result = steelbox::generate_password(options);
            if (!result.succeeded) app.error = result.message;
            else {
                app.generated_password = result.password;
                app.error.clear();
            }
        } catch (const std::exception&) {
            app.error = "Informe um comprimento numerico entre 4 e 1024.";
        } });
        auto use = Button("Usar na entrada", [&]
                          {
        if (app.generated_password.empty()) {
            app.error = "Gere uma senha antes de usa-la.";
            return;
        }
        app.form_password = app.generated_password;
        app.screen = UiScreen::EntryForm;
        app.active_component = 3; });
        auto cancel = Button("Cancelar", [&]
                             {
        app.screen = UiScreen::EntryForm;
        app.active_component = 3; });
        auto controls = Container::Vertical({length, lowercase, uppercase, digits, symbols, generate, use, cancel});
        return Renderer(controls, [&, length, lowercase, uppercase, digits,
                                   symbols, generate, use, cancel]
                        {
        Elements content = {
            text("Gerar senha") | bold, separator(),
            text("Comprimento (4-1024):"), length->Render(),
            text("Tipos de caracteres:"),
            lowercase->Render(), uppercase->Render(),
            digits->Render(), symbols->Render(),
            generate->Render(),
            text(app.generated_password.empty()
                ? "Nenhuma senha gerada."
                : "Senha gerada: " + app.generated_password),
            use->Render(), cancel->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center; });
    }

    Component help_screen(App &app)
    {
        auto close = Button("Voltar", [&]
                            {
        app.screen = app.return_screen;
        app.active_component =
            app.screen == UiScreen::Entries ? 1 : 2; });
        auto controls = Container::Vertical({close});
        return Renderer(controls, [&, close]
                        { return vbox({text("Ajuda - comandos da TUI") | bold,
                                       separator(),
                                       text("Comandos escritos (Enter):"),
                                       text("  help                         Abre esta ajuda"),
                                       text("  view                         Visualiza a entrada"),
                                       text("  edit                         Edita a entrada selecionada"),
                                       text("  delete                       Exclui a entrada selecionada"),
                                       text("  new                          Cria uma nova entrada"),
                                       text("  search <termo>               Busca entradas"),
                                       text("  clear                        Limpa a busca"),
                                       text("  change-master                Altera a senha mestra"),
                                       text("  vault <caminho>              Troca o cofre"),
                                       text("  lock                         Bloqueia o cofre"),
                                       text("  quit                         Sai da TUI"),
                                       separator(),
                                       text("Atalhos:"),
                                       text("  / ou Ctrl+F                  Inicia uma busca"),
                                       text("  Space, E/D/N/V               Editar/Excluir/Nova/Visualizar"),
                                       text("  J/K                          Move para baixo/acima"),
                                       text("  Esc                          Volta ou sai"),
                                       separator(),
                                       close->Render()}) |
                                 border | center; });
    }

    Component change_master_screen(App &app)
    {
        auto password = Input(
            &app.new_master_password, "Nova senha", InputOption{.password = true});
        auto confirmation = Input(
            &app.confirm_master_password, "Confirmacao", InputOption{.password = true});
        auto save = Button("Alterar senha", [&]
                           {
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
        app.active_component = 1; });
        auto cancel = Button("Cancelar", [&]
                             {
        app.error.clear();
        app.screen = UiScreen::Entries;
        app.active_component = 1; });
        auto controls = Container::Vertical({password, confirmation, save, cancel});
        return Renderer(controls, [&, password, confirmation, save, cancel]
                        {
        Elements content = {
            text("Alterar senha mestra") | bold, separator(),
            text("Nova senha:"), password->Render(),
            text("Confirme a nova senha:"), confirmation->Render(),
            save->Render(), cancel->Render()
        };
        if (!app.error.empty()) {
            content.push_back(text("Erro: " + app.error) | color(Color::Red));
        }
        return vbox(content) | border | center; });
    }

}

int main(int argc, char *argv[])
{
    App app;
    std::string argument_error;
    app.vault_path = parse_vault_path(argc, argv, argument_error);
    if (!argument_error.empty())
    {
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
    auto help = help_screen(app);
    auto base_root = ftxui::Container::Tab(
        {login, entries, detail, form, deletion, generator, change_master, help},
        &app.active_component);
    auto root = ftxui::CatchEvent(base_root, [&](const ftxui::Event &event)
                                  {
        if (app.screen == UiScreen::Entries) refresh_entry_labels(app);
        if (event == ftxui::Event::Escape) {
            go_back(app, screen);
            return true;
        }
        if (app.screen == UiScreen::Entries &&
            event == ftxui::Event::Character(" ")) {
            app.command_prefix_active = true;
            app.command_hint = "Spc + E: Edit | Spc + D: Delete | Spc + N: New | Spc + V: View";
            app.command_line.clear();
            return true;
        }
        if (app.screen == UiScreen::Entries &&
            (event == ftxui::Event::Character("/") ||
             event == ftxui::Event::Special("\x06"))) {
            app.search_mode = true;
            app.command_line.clear();
            app.command_input->TakeFocus();
            app.notice = "Digite o termo da busca e pressione Enter.";
            return true;
        }
        if (app.screen == UiScreen::Entries &&
            app.command_prefix_active &&
            (event == ftxui::Event::Character("e") ||
             event == ftxui::Event::Character("d") ||
             event == ftxui::Event::Character("n") ||
             event == ftxui::Event::Character("v"))) {
            if (event == ftxui::Event::Character("e")) {
                if (select_current_entry(app)) begin_edit_entry(app);
            } else if (event == ftxui::Event::Character("d")) {
                open_delete_confirmation(app);
            } else if (event == ftxui::Event::Character("n")) {
                begin_new_entry(app);
            } else {
                show_selected_entry(app);
            }
            app.command_prefix_active = false;
            return true;
        }
        if (app.screen == UiScreen::Entries &&
            (event == ftxui::Event::Special("\x0e") ||
             event == ftxui::Event::ArrowDownCtrl) &&
            !app.entry_labels.empty()) {
            app.selected_index = std::min(
                app.selected_index + 1,
                static_cast<int>(app.entry_labels.size()) - 1
            );
            return true;
        }
        if (app.screen == UiScreen::Entries &&
            (event == ftxui::Event::Special("\x10") ||
             event == ftxui::Event::ArrowUpCtrl) &&
            !app.entry_labels.empty()) {
            app.selected_index = std::max(app.selected_index - 1, 0);
            return true;
        }
        if (app.screen == UiScreen::Entries &&
            (event == ftxui::Event::Character("j") ||
             event == ftxui::Event::Character("k"))) {
            if (event == ftxui::Event::Character("j")) {
                app.selected_index = std::min(
                    app.selected_index + 1,
                    static_cast<int>(app.entry_labels.size()) - 1
                );
            } else if (event == ftxui::Event::Character("k")) {
                app.selected_index = std::max(app.selected_index - 1, 0);
            }
            return true;
        }
        if (event == ftxui::Event::Special("\x0c") &&
            app.screen == UiScreen::Entries) {
            lock_vault(app);
            return true;
        }
        if (event == ftxui::Event::Special("\x0f") &&
            app.screen == UiScreen::Entries) {
            app.command_hint = "Ctrl + O: digite 'vault <caminho>' no terminal e pressione Enter";
            return true;
        }
        if (event == ftxui::Event::Special("\x11")) {
            app.vault.lock();
            app.master_password.clear();
            screen.Exit();
            return true;
        }
        if (event == ftxui::Event::Special("\x02") &&
            app.screen == UiScreen::Detail) {
            app.selected = nullptr;
            app.screen = UiScreen::Entries;
            app.active_component = 1;
            return true;
        }
        return false; });
    screen.Loop(root);
    app.vault.lock();
    app.master_password.clear();
    app.login_password.clear();
    app.form_password.clear();
    app.new_master_password.clear();
    app.confirm_master_password.clear();
    return 0;
}
