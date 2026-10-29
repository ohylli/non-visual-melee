/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_speech.hpp"
#include "menu_names.hpp"
#include "sentences.hpp"
#include <optional>
#include <string_view>

namespace a11y {
namespace {

/* A tree screen's hovered entry, as log lines name it. */
std::string entry_where(const A11yMenuState& state) {
    return "[a11y] menu " + std::to_string(state.menu) + " entry " + std::to_string(state.hovered);
}

/* A reader's key for the row in view. */
int row_key(const Reader& reader, const A11yMenuState& state) {
    return reader.key == RowKey::hovered ? state.hovered : state.center_text;
}

/* A leaf screen's row, as log lines name it: by its centre text or by its
 * number in the hovered entry. */
std::string row_where(const A11yMenuState& state) {
    const Reader* reader = menu_reader(state.menu);
    std::string out = "[a11y] menu " + std::to_string(state.menu);
    if (reader != nullptr && reader->key == RowKey::hovered) {
        return out + " row " + std::to_string(state.hovered);
    }
    return out + " centre text " + std::to_string(state.center_text);
}

/* The reader's row in view; nothing on a screen without a reader or for a
 * row its reader lacks. */
const ReaderRow* reader_row(const A11yMenuState& state) {
    const Reader* reader = menu_reader(state.menu);
    if (reader == nullptr) {
        return nullptr;
    }
    const ReaderRow* row = reader->row(row_key(*reader, state));
    if (row != nullptr && row->in_stock_match != nullptr && is_stock_match(state.rules.mode)) {
        return row->in_stock_match;
    }
    return row;
}

/* A value spoken on its own starts a sentence: "Centre". */
std::string capitalised(std::string text) {
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    }
    return text;
}

}  // namespace

void MenuSpeech::frame(const A11yMenuState& state) {
    bool arrived = !m_seen || state.menu != m_menu;
    const Reader* reader = menu_reader(state.menu);
    bool returned = arrived && m_seen && reader != nullptr && reader->opens(m_menu);
    bool moved = !arrived && state.hovered != m_hovered && is_tree_screen(state.menu);
    int key = reader != nullptr ? row_key(*reader, state) : -1;
    bool row_changed = !arrived && reader != nullptr && key != m_row;
    std::optional<std::string> value = row_value(state);
    bool value_changed = !arrived && reader != nullptr && !row_changed && value && value != m_value;
    bool refused = !arrived && reader != nullptr && state.refused && !reader->refused.empty();
    if (arrived) {
        m_row_hints.clear();
    }
    m_seen = true;
    m_menu = state.menu;
    m_hovered = state.hovered;
    m_row = key;
    m_value = value;
    if (arrived) {
        m_speech.announce(opening(state, returned), Mode::interrupt);
    } else if (moved) {
        m_speech.announce(hovered_entry(state), Mode::interrupt);
    } else if (row_changed) {
        std::string out = row_reached(state);
        if (reader->described) {
            add_sentence(out, row_description(state));
        }
        m_speech.announce(out, Mode::interrupt);
    } else if (value_changed) {
        std::string out = capitalised(*value);
        if (reader_row(state)->described_per_value) {
            add_sentence(out, row_description(state));
        }
        m_speech.announce(out, Mode::interrupt);
    } else if (refused) {
        m_speech.announce(std::string(reader->refused), Mode::interrupt);
    }
}

std::string MenuSpeech::opening(const A11yMenuState& state, bool returned) {
    std::optional<std::string_view> name = menu_screen_name(state.menu);
    std::string out;
    if (!name) {
        m_log_once("[a11y] menu " + std::to_string(state.menu) + ": not in the menu names table");
        return "Unknown screen " + std::to_string(state.menu);
    }
    add_sentence(out, *name);
    if (is_tree_screen(state.menu)) {
        add_sentence(out, hovered_entry(state));
    } else if (const Reader* reader = menu_reader(state.menu)) {
        add_sentence(out, row_reached(state));
        if (!returned) {
            add_sentence(out, reader->hint);
        }
        if (reader->described) {
            add_sentence(out, row_description(state));
        }
    } else {
        add_sentence(out, "No speech yet.");
    }
    return out;
}

std::string MenuSpeech::hovered_entry(const A11yMenuState& state) {
    std::string out;
    if (state.pc_label != nullptr) {
        add_sentence(out, plain_capitals(state.pc_label));
    } else if (std::optional<std::string_view> name = menu_entry_name(state.menu, state.hovered)) {
        add_sentence(out, *name);
    } else {
        m_log_once(entry_where(state) + ": not in the menu names table");
        add_sentence(out, "Unknown entry " + std::to_string(state.hovered));
    }
    add_sentence(out, description(state.pc_description, state.description, entry_where(state)));
    return out;
}

std::string MenuSpeech::row_reached(const A11yMenuState& state) {
    std::string out = row(state);
    const ReaderRow* named = reader_row(state);
    if (named != nullptr && !named->hint.empty() && m_row_hints.insert(named->hint).second) {
        add_sentence(out, named->hint);
    }
    return out;
}

std::string MenuSpeech::row(const A11yMenuState& state) {
    std::string out;
    if (const ReaderRow* named = reader_row(state)) {
        out = named->spoken_name();
    } else {
        m_log_once(row_where(state) + ": not in the menu names table");
        const Reader* reader = menu_reader(state.menu);
        out = "Unknown row " + std::to_string(reader != nullptr ? row_key(*reader, state) : -1);
    }
    if (std::optional<std::string> value = row_value(state)) {
        out += ": " + *value;
    }
    return out;
}

std::optional<std::string> MenuSpeech::row_value(const A11yMenuState& state) {
    const ReaderRow* row = reader_row(state);
    if (row == nullptr) {
        return std::nullopt;
    }
    const A11yRules& rules = state.rules;
    switch (row->value) {
    case RowValue::channel:
        return std::string(channel_word(state.mono));
    case RowValue::balance:
        return balance_words(state.balance);
    case RowValue::deflicker:
        return std::string(on_off_word(state.deflicker));
    case RowValue::rules_mode:
        return known(rules_mode_word(rules.mode), rules.mode, state);
    case RowValue::time_limit:
        return minutes_words(rules.time_limit);
    case RowValue::stock_count:
        return std::to_string(rules.stock_count);
    case RowValue::handicap:
        return known(handicap_word(rules.handicap), rules.handicap, state);
    case RowValue::damage_ratio:
        return damage_ratio_words(rules.damage_ratio);
    case RowValue::stage_selection:
        return known(stage_selection_word(rules.stage_selection), rules.stage_selection, state);
    case RowValue::stock_time_limit:
        return minutes_words(rules.stock_time_limit);
    case RowValue::friendly_fire:
        return std::string(on_off_word(rules.friendly_fire));
    case RowValue::pause:
        return std::string(on_off_word(rules.pause));
    case RowValue::score_display:
        return std::string(on_off_word(rules.score_display));
    case RowValue::self_destructs:
        return known(self_destructs_words(rules.self_destructs), rules.self_destructs, state);
    case RowValue::switched:
        return known(switch_word(state.confirmed), state.confirmed, state);
    case RowValue::item_frequency:
        return known(item_frequency_word(state.confirmed), state.confirmed, state);
    case RowValue::none:
        break;
    }
    return std::nullopt;
}

std::string MenuSpeech::known(
    std::optional<std::string_view> word, int number, const A11yMenuState& state) {
    if (word) {
        return std::string(*word);
    }
    m_log_once(
        row_where(state) + ": value " + std::to_string(number) + " not in the menu names table");
    return std::to_string(number);
}

std::string MenuSpeech::row_description(const A11yMenuState& state) {
    const Reader* reader = menu_reader(state.menu);
    const std::uint8_t* text = reader != nullptr && reader->key == RowKey::hovered ?
                                   state.description :
                                   state.center_text_string;
    return description(nullptr, text, row_where(state));
}

std::string MenuSpeech::description(
    const char* plain, const std::uint8_t* game_text, const std::string& where) {
    if (plain != nullptr) {
        return spoken_text(plain);
    }
    if (game_text == nullptr) {
        m_log_once(where + ": no description");
        return "";
    }
    return spoken_game_text(game_text, m_text, m_log_once, where + ": description");
}

}  // namespace a11y
