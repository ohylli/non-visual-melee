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

/* The rows' markup in the base port's classes (resources/launcher.rml and
 * port-menu.rml): a name beside a value button, a slider with its shown
 * value, or an action button; the help in data-desc, which both pages show
 * in their description bars and the readers read. A switch and a choice
 * share the value button, as the base port's two-state rows and its choice
 * rows (Aspect ratio, Post-processing) do. The port menu's rows repeat the
 * help under the name, as its own rows do. */
std::string row_markup(const ShownRow& row, SettingsHost host) {
    const std::string id = settings_row_id(row);
    const std::string& label = row.value != nullptr ? row.value->label : row.action->label;
    const std::string& help =
        row.value != nullptr ? row.value->description : row.action->description;
    std::string rml = "<div class=\"row\" data-desc=\"" + escaped(help) +
                      "\"><span class=\"name\">" + escaped(label);
    if (host == SettingsHost::port_menu) {
        rml += "<span class=\"detail\">" + escaped(help) + "</span>";
    }
    rml += "</span>";
    if (row.action != nullptr) {
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

/* The category's rows in one document. */
class Page {
public:
    Page(Rml::ElementDocument& document, SettingsHost host, AccessibilitySettings& settings,
        const SettingsPageGame& game)
        : m_document(document), m_host(host), m_settings(settings), m_game(game) {
        Rml::Element* page = document.GetElementById(kPageId);
        if (page == nullptr) {
            return; /* a base merge took the page away: nothing to build into */
        }
        std::string rml;
        for (const ShownRow& row : settings.shown()) {
            rml += row_markup(row, host);
            m_rows.push_back(Row{&row, settings_row_id(row), {}});
        }
        page->SetInnerRML(rml);
        for (Row& row : m_rows) {
            if (row.shown->value != nullptr) {
                row.text = shown_value(*row.shown->value);
            }
        }
    }

    ~Page() { stop_preview(); }
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;

    const Rml::ElementDocument& document() const { return m_document; }
    SettingsHost host() const { return m_host; }

    /* Enter, A or a click: a switch flips, a choice steps to its next
     * option, as the base port's choice rows do, Preview plays its cue,
     * again from its start when pressed again. A slider stays as it is: Left
     * and Right step it, and Enter wrapping it round from the top, as the
     * base port's sliders do, would be a jump the player cannot hear
     * coming. */
    bool press(const std::string& id) {
        Row* row = find(id);
        if (row == nullptr) {
            return false;
        }
        if (row->shown->action != nullptr) {
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
     * choice rows do. A Preview row's arrows are the page's, as on the base
     * port's action buttons: Left goes back to the category. */
    bool step(const std::string& id, int direction) {
        Row* row = find(id);
        if (row == nullptr || row->shown->action != nullptr) {
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

private:
    struct Row {
        const ShownRow* shown;
        std::string id;
        std::string text; /* the value as last written, empty for a button */
    };

    Row* find(const std::string& id) {
        auto it = std::find_if(
            m_rows.begin(), m_rows.end(), [&id](const Row& row) { return row.id == id; });
        return it != m_rows.end() ? &*it : nullptr;
    }

    Rml::Element* focused() const {
        Rml::Context* context = m_document.GetContext();
        return context != nullptr ? context->GetFocusElement() : nullptr;
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
    const Row* m_preview = nullptr; /* the Preview row whose cue this page started */
    bool m_writing = false;
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
        m_ids.clear();
        for (const ShownRow& row : settings.shown()) {
            m_ids.push_back(settings_row_id(row));
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
        m_ids.clear();
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

    const std::vector<std::string>& ids() const { return m_ids; }

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
    std::vector<std::string> m_ids; /* the rows' ids, in the rows' order */
    std::vector<std::unique_ptr<Page>> m_pages;
    bool m_registered = false;
};

PagesPlugin s_plugin;

}  // namespace

std::string settings_row_id(const ShownRow& row) {
    std::string id = "a11y-" + (row.value != nullptr ? row.value->key : row.action->key);
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

bool pc_a11y_settings_change(Rml::Event& event) {
    Rml::Element* target = event.GetTargetElement();
    a11y::Page* page =
        target != nullptr ? a11y::s_plugin.page(target->GetOwnerDocument()) : nullptr;
    return page != nullptr && page->changed(event);
}
