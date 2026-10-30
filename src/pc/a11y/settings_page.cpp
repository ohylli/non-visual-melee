/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings_page.hpp"
#include "a11y_hooks.h"
#include "accessibility_settings.hpp"
#include "rmlui_reader.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Plugin.h>
#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace a11y {
namespace {

/* The base port's empty page for the category, in both documents. */
constexpr const char* kPageId = "page-accessibility";
/* A Preview row's button. The row's name says what it plays. */
constexpr const char* kPlay = "Play";
/* The button of a row that opens a submenu. The row's name says which. */
constexpr const char* kOpen = "Open";
/* The line at the top of the page that names the open submenu, hidden on the
 * top level. */
constexpr const char* kTitleId = "a11y-menu-title";
/* Between the names of the submenus a nested one is inside, on its title
 * line, as the pages' own title bars join theirs: "Melee PC / Settings". */
constexpr const char* kTitleSeparator = " / ";
/* A choice with no options listed now and no words of its own for why
 * (ChoiceSpec::none). */
constexpr const char* kNoOption = "None";

/* Text for RML: what would read as markup, escaped. */
std::string escaped(std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

/* A value as its row shows it, as the base port's rows show theirs: "On",
 * "80%", "40 units", a choice's option in use by its label ("Microsoft Zira,
 * English (United States)"); held by an override from the environment, "Off
 * (environment)", as the launcher shows an overridden vertical sync. */
std::string shown_value(const NamedValue& named) {
    std::string text;
    if (named.kind == ValueKind::toggle) {
        text = named.value != 0.0f ? "On" : "Off";
    } else if (named.kind == ValueKind::choice) {
        const ChoiceOptions options = choice_options(named);
        const int index = choice_index(named, options);
        if (index >= 0) {
            text = options[static_cast<std::size_t>(index)].shown();
        } else {
            text = named.none ? named.none() : kNoOption;
        }
    } else {
        text = format_value(named);
        if (named.unit == "%") {
            text += "%";
        } else if (!named.unit.empty()) {
            text += " " + named.unit;
        }
    }
    if (named.overridden) {
        text += " (environment)";
    }
    return text;
}

/* A number for an attribute, the shortest text that reads back the same. */
std::string number(float value) {
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

bool is_slider(const ShownRow& row) {
    return row.value != nullptr && row.value->kind == ValueKind::number;
}

/* What names a row, whichever it holds: a value, an action or the submenu
 * it opens. */
struct RowWords {
    const std::string& key;
    const std::string& label;
    const std::string& help;
};

RowWords words_of(const ShownRow& row) {
    if (row.value != nullptr) {
        return RowWords{row.value->key, row.value->label, row.value->description};
    }
    if (row.action != nullptr) {
        return RowWords{row.action->key, row.action->label, row.action->description};
    }
    return RowWords{row.opens->key, row.opens->label, row.opens->description};
}

/* The title line's markup, as each page heads a block of text of its own
 * (an .info block's heading); its text is written as a submenu opens. It is
 * for a sighted helper: the readers leave it out of the lines they read
 * (page_stops), as opening a submenu has said its name. */
std::string title_markup(SettingsHost host) {
    const char* heading = host == SettingsHost::port_menu ? "h2" : "h3";
    return std::string("<div id=\"") + kTitleId + "\" class=\"info\" aria-hidden=\"true\"><" +
           heading + "></" + heading + "></div>";
}

/* The submenu's title line: the names of the submenus it is inside, then
 * its own. */
std::string title_text(const NamedMenu& menu) {
    return menu.parent != nullptr ? title_text(*menu.parent) + kTitleSeparator + menu.label :
                                    menu.label;
}

/* The rows' markup in the base port's classes (resources/launcher.rml and
 * port-menu.rml): a name beside a value button, a slider with its shown
 * value, or an action button; the help in data-desc, which both pages show
 * in their description bars and the readers read. A switch and a choice
 * share the value button, as the base port's two-state rows and its choice
 * rows (Aspect ratio, Post-processing) do. A row that opens a submenu is an
 * action button too, marked as one with the class "submenu". The port
 * menu's rows repeat the help under the name, as its own rows do. */
std::string row_markup(const ShownRow& row, SettingsHost host) {
    const std::string id = settings_row_id(row);
    const RowWords words = words_of(row);
    const std::string& help = words.help;
    std::string rml = "<div class=\"row\" data-desc=\"" + escaped(help) +
                      "\"><span class=\"name\">" + escaped(words.label);
    if (host == SettingsHost::port_menu) {
        rml += "<span class=\"detail\">" + escaped(help) + "</span>";
    }
    rml += "</span>";
    if (row.opens != nullptr) {
        rml += "<button id=\"" + id + "\" class=\"value action submenu\">" + kOpen + "</button>";
    } else if (row.action != nullptr) {
        rml += "<button id=\"" + id + "\" class=\"value action\">" + kPlay + "</button>";
    } else if (is_slider(row)) {
        const NamedValue& named = *row.value;
        rml += "<div class=\"slider-control\"><input type=\"range\" class=\"range\" id=\"" + id +
               "\" min=\"" + number(named.min) + "\" max=\"" + number(named.max) + "\" step=\"" +
               number(named.step) + "\" value=\"" + number(named.value) + "\" /><span id=\"" + id +
               "-val\" class=\"slider-value\">" + escaped(shown_value(named)) + "</span></div>";
    } else {
        rml += "<button id=\"" + id + "\" class=\"value\">" + escaped(shown_value(*row.value)) +
               "</button>";
    }
    return rml + "</div>";
}

/* The category's rows in one document, and its submenus
 * (docs/adr/0009-the-accessibility-settings-split-into-submenus-inside-the-one-category.md):
 * every level's rows are built at once, those of every level but the open
 * one hidden. The open level follows focus: a row taking focus shows its
 * own level, and focus outside the page, or the page hidden, shows the top
 * level, so every way out of the category comes back to it. */
class Page final : public Rml::EventListener {
public:
    Page(Rml::ElementDocument& document, SettingsHost host, AccessibilitySettings& settings,
        const SettingsPageGame& game)
        : m_document(document), m_host(host), m_settings(settings), m_game(game) {
        Rml::Element* page = document.GetElementById(kPageId);
        if (page == nullptr) {
            return; /* a base merge took the page away: nothing to build into */
        }
        std::string rml = title_markup(host);
        for (const ShownRow& row : settings.shown()) {
            rml += row_markup(row, host);
            m_rows.push_back(Row{&row, settings_row_id(row), {}, nullptr});
        }
        page->SetInnerRML(rml);
        m_page = page;
        m_title = page->GetChild(0);
        for (std::size_t i = 0; i < m_rows.size(); ++i) {
            Row& row = m_rows[i];
            row.element = page->GetChild(static_cast<int>(i) + 1);
            if (row.shown->value != nullptr) {
                row.text = shown_value(*row.shown->value);
            }
        }
        show(nullptr, true);
        /* Focus does not bubble; the capture pass still reaches the
         * document. */
        document.AddEventListener(Rml::EventId::Focus, this, true);
        document.AddEventListener(Rml::EventId::Hide, this);
        m_listening = true;
    }

    /* The document is still loaded here: a page goes as its document
     * unloads, or as the plugin stops, before RmlUi shuts down. */
    ~Page() override {
        stop_preview();
        if (m_listening) {
            m_document.RemoveEventListener(Rml::EventId::Focus, this, true);
            m_document.RemoveEventListener(Rml::EventId::Hide, this);
        }
    }
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;

    const Rml::ElementDocument& document() const { return m_document; }
    SettingsHost host() const { return m_host; }
    /* The open submenu, null on the top level. */
    const NamedMenu* open_menu() const { return m_open; }

    /* The open level's rows' ids, in their order. */
    std::vector<std::string> level_ids() const {
        std::vector<std::string> ids;
        for (const Row& row : m_rows) {
            if (row.shown->menu == m_open) {
                ids.push_back(row.id);
            }
        }
        return ids;
    }

    /* Enter, A or a click: a switch flips, a choice steps to its next
     * option, as the base port's choice rows do, Preview plays its cue,
     * again from its start when pressed again, a row opening a submenu
     * opens it. A slider stays as it is: Left and Right step it, and Enter
     * wrapping it round from the top, as the base port's sliders do, would
     * be a jump the player cannot hear coming. */
    bool press(const std::string& id) {
        Row* row = find(id);
        if (row == nullptr) {
            return false;
        }
        if (row->shown->opens != nullptr) {
            open(*row->shown->opens);
        } else if (row->shown->action != nullptr) {
            /* No sound of the menu's: the cue is the answer. */
            m_game.preview(row->shown->action->group);
            m_preview = row;
        } else if (row->shown->value->kind == ValueKind::choice) {
            choose_next(*row->shown->value, 1);
        } else if (!is_slider(*row->shown)) {
            flip(*row->shown->value);
        }
        return true;
    }

    /* Left or Right: a switch flips either way, as the base port's
     * two-state settings do; a slider steps by its value's step; a choice
     * steps through its options, round from either end, as the base port's
     * choice rows do. On a row holding no value, Right opens a submenu and
     * Left goes back a level (back); on the top level a Preview's arrows
     * and an opening row's Left are the page's, as on the base port's
     * action buttons: Left goes back to the category. */
    bool step(const std::string& id, int direction) {
        Row* row = find(id);
        if (row == nullptr) {
            return false;
        }
        if (row->shown->value == nullptr) {
            if (direction < 0) {
                return back();
            }
            if (row->shown->opens != nullptr) {
                open(*row->shown->opens);
                return true;
            }
            return false;
        }
        const NamedValue& named = *row->shown->value;
        if (named.kind == ValueKind::toggle) {
            flip(named);
        } else if (named.kind == ValueKind::choice) {
            choose_next(named, direction);
        } else {
            change(
                named, named.value + static_cast<float>(direction) * named.step, MenuSound::move);
        }
        return true;
    }

    /* Escape or B, and Left on a row holding no value: out of the open
     * submenu, onto the row that opens it, one level up. False on the top
     * level, where the page goes back to the category itself. */
    bool back() {
        if (m_open == nullptr) {
            return false;
        }
        const NamedMenu& menu = *m_open;
        show(menu.parent);
        for (const Row& row : m_rows) {
            if (row.shown->opens == &menu) {
                focus(row);
                break;
            }
        }
        return true;
    }

    /* Up (-1) or Down (1) in the port menu, whose own column is the top
     * level's, copied as the category opens: inside a submenu, round its
     * own rows, from the last to the first and back. False on the top
     * level, which the menu moves through itself. */
    bool focus_step(int direction) {
        if (m_open == nullptr) {
            return false;
        }
        std::vector<const Row*> rows;
        int at = direction > 0 ? -1 : 0;
        const Rml::Element* control = focused();
        for (const Row& row : m_rows) {
            if (row.shown->menu != m_open) {
                continue;
            }
            if (m_document.GetElementById(row.id) == control) {
                at = static_cast<int>(rows.size());
            }
            rows.push_back(&row);
        }
        if (rows.empty()) {
            return false;
        }
        const int count = static_cast<int>(rows.size());
        focus(*rows[static_cast<std::size_t>((at + direction + count) % count)]);
        return true;
    }

    /* A slider's Change: RmlUi stepped it with the keyboard's arrows, or the
     * pointer dragged it. */
    bool changed(Rml::Event& event) {
        Rml::Element* target = event.GetTargetElement();
        Row* row = target != nullptr ? find(target->GetId()) : nullptr;
        if (row == nullptr) {
            return false;
        }
        /* The page writing its own slider echoes back as a Change. */
        if (m_writing || !is_slider(*row->shown)) {
            return true;
        }
        const NamedValue& named = *row->shown->value;
        const float value = event.GetParameter<float>("value", named.value);
        if (value != named.value) {
            change(named, value, MenuSound::move);
        }
        return true;
    }

    void frame(bool open) {
        refresh();
        if (!open) {
            show(nullptr);
        }
        if (m_preview != nullptr &&
            !(open && focused() == m_document.GetElementById(m_preview->id)))
        {
            stop_preview();
        }
    }

    /* The rows' values, written where they changed. */
    void refresh() {
        for (Row& row : m_rows) {
            if (row.shown->value == nullptr) {
                continue;
            }
            const NamedValue& named = *row.shown->value;
            std::string text = shown_value(named);
            if (text == row.text) {
                continue;
            }
            row.text = std::move(text);
            if (is_slider(*row.shown)) {
                if (Rml::Element* slider = m_document.GetElementById(row.id)) {
                    m_writing = true;
                    slider->SetAttribute("value", named.value);
                    m_writing = false;
                }
                write(row.id + "-val", row.text);
            } else {
                write(row.id, row.text);
            }
        }
    }

    void stop_preview() {
        if (m_preview != nullptr) {
            m_preview = nullptr;
            m_game.stop_preview();
        }
    }

    /* Focus moved, or the page was hidden (the port menu closing): the
     * level shown follows. */
    void ProcessEvent(Rml::Event& event) override {
        if (event.GetId() == Rml::EventId::Hide) {
            show(nullptr);
            return;
        }
        const Rml::Element* target = event.GetTargetElement();
        if (const Row* row = row_of(target)) {
            show(row->shown->menu);
        } else if (!inside_page(target)) {
            show(nullptr);
        }
        /* Else a click on the page between its rows, or on the title line,
         * which takes focus without leaving the category: the open level
         * stays. */
    }

private:
    struct Row {
        const ShownRow* shown;
        std::string id;
        std::string text;      /* the value as last written, empty for a button */
        Rml::Element* element; /* the row, its control inside */
    };

    Row* find(const std::string& id) {
        auto it = std::find_if(
            m_rows.begin(), m_rows.end(), [&id](const Row& row) { return row.id == id; });
        return it != m_rows.end() ? &*it : nullptr;
    }

    /* The row the element is in, if it is in one. */
    const Row* row_of(const Rml::Element* element) const {
        for (; element != nullptr && element != &m_document; element = element->GetParentNode()) {
            for (const Row& row : m_rows) {
                if (row.element == element) {
                    return &row;
                }
            }
        }
        return nullptr;
    }

    /* Whether the element is the page or inside it. */
    bool inside_page(const Rml::Element* element) const {
        for (; element != nullptr; element = element->GetParentNode()) {
            if (element == m_page) {
                return true;
            }
        }
        return false;
    }

    Rml::Element* focused() const {
        Rml::Context* context = m_document.GetContext();
        return context != nullptr ? context->GetFocusElement() : nullptr;
    }

    /* Shows the rows of one level, null for the top, and hides the rest;
     * the title line names a submenu. `force` writes every row, as the page
     * is built. */
    void show(const NamedMenu* menu, bool force = false) {
        if (menu == m_open && !force) {
            return;
        }
        m_open = menu;
        for (const Row& row : m_rows) {
            displayed(*row.element, row.shown->menu == menu);
        }
        if (m_title != nullptr) {
            if (Rml::Element* heading = m_title->GetFirstChild()) {
                heading->SetInnerRML("");
                if (menu != nullptr) {
                    heading->AppendChild(m_document.CreateTextNode(title_text(*menu)));
                }
            }
            displayed(*m_title, menu != nullptr);
        }
    }

    /* Into a submenu, on its first row, with the title line above it in
     * view: scrolled to the row alone, a long page hides it. */
    void open(const NamedMenu& menu) {
        show(&menu);
        for (const Row& row : m_rows) {
            if (row.shown->menu == &menu) {
                focus(row);
                if (m_title != nullptr) {
                    m_title->ScrollIntoView();
                }
                return;
            }
        }
    }

    /* As the base port moves focus onto a row: the page's display may just
     * have changed, which RmlUi lays out only on update. */
    void focus(const Row& row) {
        m_document.UpdateDocument();
        if (Rml::Element* control = m_document.GetElementById(row.id)) {
            control->Focus();
            control->ScrollIntoView();
        }
    }

    /* The element's display as the page's style sheet sets it, or none. */
    static void displayed(Rml::Element& element, bool shown) {
        if (shown) {
            element.RemoveProperty("display");
        } else {
            element.SetProperty("display", "none");
        }
    }

    void flip(const NamedValue& named) {
        change(named, named.value != 0.0f ? 0.0f : 1.0f, MenuSound::confirm);
    }

    /* The options as they stand now: a run-time list may have changed since
     * the row was last written. With none, the press does nothing. */
    void choose_next(const NamedValue& named, int direction) {
        const std::string option = choice_step(named, choice_options(named), direction);
        if (!option.empty()) {
            after_change(m_settings.choose(named, option, Clock::now()), MenuSound::confirm);
        }
    }

    void change(const NamedValue& named, float value, MenuSound sound) {
        after_change(m_settings.set(named, value, Clock::now()), sound);
    }

    /* After a change saved through the settings, whose listener does the
     * rest (a switch's last announcement included); the readers hear the
     * new value from the page. In the port menu, its sound, and "Saved." on
     * its status line, as after a change of its own rows (a copy of
     * PortMenu::saved's text and element, to check at a base merge); the
     * reader never speaks that. */
    void after_change(bool changed, MenuSound sound) {
        refresh();
        if (m_host != SettingsHost::port_menu) {
            return;
        }
        if (m_game.port_menu_sound) {
            m_game.port_menu_sound(sound);
        }
        if (changed && m_settings.saves()) {
            write("menu-status", "Saved.");
        }
    }

    /* As the base port writes a label: its text replaced by a text node. */
    void write(const std::string& id, const std::string& text) {
        Rml::Element* element = m_document.GetElementById(id);
        if (element == nullptr) {
            return;
        }
        element->SetInnerRML("");
        element->AppendChild(m_document.CreateTextNode(text));
    }

    Rml::ElementDocument& m_document;
    SettingsHost m_host;
    AccessibilitySettings& m_settings;
    const SettingsPageGame& m_game;
    std::vector<Row> m_rows;
    Rml::Element* m_page = nullptr;  /* #page-accessibility, null with no page */
    Rml::Element* m_title = nullptr; /* the title line, null with no page */
    const NamedMenu* m_open = nullptr;
    const Row* m_preview = nullptr; /* the Preview row whose cue this page started */
    bool m_writing = false;
    bool m_listening = false;
};

std::optional<SettingsHost> host_of(const Rml::ElementDocument& document) {
    if (is_document(document, "launcher.rml")) {
        return SettingsHost::launcher;
    }
    if (is_document(document, "port-menu.rml")) {
        return SettingsHost::port_menu;
    }
    return std::nullopt;
}

/* Builds a page's rows as its document loads and drops them as it unloads.
 * RmlUi keeps a pointer to a registered plugin until it shuts down, so this
 * is a static object, as the readers' plugins are. */
class PagesPlugin final : public Rml::Plugin {
public:
    int GetEventClasses() override { return EVT_BASIC | EVT_DOCUMENT; }

    void OnDocumentLoad(Rml::ElementDocument* document) override {
        if (m_settings == nullptr) {
            return;
        }
        if (const std::optional<SettingsHost> host = host_of(*document)) {
            m_pages.push_back(std::make_unique<Page>(*document, *host, *m_settings, m_game));
        }
    }

    void OnDocumentUnload(Rml::ElementDocument* document) override {
        std::erase_if(m_pages, [document](const std::unique_ptr<Page>& page) {
            return &page->document() == document;
        });
    }

    /* By now RmlUi has unloaded every document, and with it every page. */
    void OnShutdown() override { m_pages.clear(); }

    void start(AccessibilitySettings& settings, SettingsPageGame game) {
        m_settings = &settings;
        m_game = std::move(game);
        m_top_ids.clear();
        for (const ShownRow& row : settings.shown()) {
            if (row.menu == nullptr) {
                m_top_ids.push_back(settings_row_id(row));
            }
        }
        if (!m_registered) {
            Rml::RegisterPlugin(this);
            m_registered = true;
        }
    }

    void stop() {
        m_pages.clear();
        if (m_registered) {
            Rml::UnregisterPlugin(this);
            m_registered = false;
        }
        m_settings = nullptr;
        m_top_ids.clear();
    }

    Page* page(const Rml::ElementDocument* document) const {
        for (const std::unique_ptr<Page>& page : m_pages) {
            if (&page->document() == document) {
                return page.get();
            }
        }
        return nullptr;
    }

    void frame(SettingsHost host, bool open) {
        for (const std::unique_ptr<Page>& page : m_pages) {
            if (page->host() == host) {
                page->frame(open);
            }
        }
    }

    bool focus_step(SettingsHost host, int direction) {
        for (const std::unique_ptr<Page>& page : m_pages) {
            if (page->host() == host && page->focus_step(direction)) {
                return true;
            }
        }
        return false;
    }

    /* The rows of the page with a submenu open, else the top level's. The
     * hook asking takes no document; one page at most has a submenu open,
     * as the launcher's document is gone before the port menu's loads, and
     * either shows its top level again once focus leaves its rows. */
    std::vector<std::string> ids() const {
        for (const std::unique_ptr<Page>& page : m_pages) {
            if (page->open_menu() != nullptr) {
                return page->level_ids();
            }
        }
        return m_top_ids;
    }

    bool slider(const std::string& id) const {
        if (m_settings == nullptr) {
            return false;
        }
        for (const ShownRow& row : m_settings->shown()) {
            if (settings_row_id(row) == id) {
                return is_slider(row);
            }
        }
        return false;
    }

private:
    AccessibilitySettings* m_settings = nullptr;
    SettingsPageGame m_game;
    std::vector<std::string> m_top_ids; /* the top level's rows' ids, in the rows' order */
    std::vector<std::unique_ptr<Page>> m_pages;
    bool m_registered = false;
};

PagesPlugin s_plugin;

}  // namespace

std::string settings_row_id(const ShownRow& row) {
    std::string id = "a11y-" + words_of(row).key;
    std::replace(id.begin(), id.end(), '.', '-');
    return id;
}

void settings_pages_start(AccessibilitySettings& settings, SettingsPageGame game) {
    s_plugin.start(settings, std::move(game));
}

void settings_pages_stop() {
    s_plugin.stop();
}

void settings_pages_frame(SettingsHost host, bool open) {
    s_plugin.frame(host, open);
}

bool settings_pages_focus_step(SettingsHost host, int direction) {
    return s_plugin.focus_step(host, direction);
}

std::string SubmenuEntry::frame(const Rml::ElementDocument& document) {
    const Page* page = s_plugin.page(&document);
    const NamedMenu* open = page != nullptr ? page->open_menu() : nullptr;
    const NamedMenu* was = std::exchange(m_open, open);
    if (open == nullptr || open == was) {
        return "";
    }
    /* Back up from a submenu inside it: not an entry. */
    for (const NamedMenu* menu = was; menu != nullptr; menu = menu->parent) {
        if (menu == open) {
            return "";
        }
    }
    return open->label;
}

}  // namespace a11y

std::vector<std::string> pc_a11y_settings_rows() {
    return a11y::s_plugin.ids();
}

bool pc_a11y_settings_slider(const std::string& id) {
    return a11y::s_plugin.slider(id);
}

bool pc_a11y_settings_press(Rml::ElementDocument* document, const std::string& id) {
    a11y::Page* page = a11y::s_plugin.page(document);
    return page != nullptr && page->press(id);
}

bool pc_a11y_settings_step(Rml::ElementDocument* document, const std::string& id, int direction) {
    a11y::Page* page = a11y::s_plugin.page(document);
    return page != nullptr && page->step(id, direction);
}

bool pc_a11y_settings_back(Rml::ElementDocument* document) {
    a11y::Page* page = a11y::s_plugin.page(document);
    return page != nullptr && page->back();
}

bool pc_a11y_settings_change(Rml::Event& event) {
    Rml::Element* target = event.GetTargetElement();
    a11y::Page* page =
        target != nullptr ? a11y::s_plugin.page(target->GetOwnerDocument()) : nullptr;
    return page != nullptr && page->changed(event);
}
