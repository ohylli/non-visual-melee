/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_speech.hpp"
#include "css_names.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <cctype>
#include <optional>
#include <string_view>

namespace a11y {
namespace {

/* Appends one part of an announcement as its own sentence. */
void add(std::string& out, std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!out.empty()) {
        out += ". ";
    }
    out += part;
}

/* The words with their first letter made a capital: "No character". */
std::string capitalised(std::string_view words) {
    std::string out(words);
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}

/* "Player 2" for slot 1, as its tab shows "P2" in VS modes. */
std::string player(int slot) {
    return std::string(kPlayer) + " " + std::to_string(slot + 1);
}

std::string team(int number) {
    std::optional<std::string_view> word = team_word(number);
    return word ? std::string(*word) : std::string(kTeam) + " " + std::to_string(number + 1);
}

/* The character a slot shows as chosen: its coin at rest on a portrait. A
 * slot not yet opened, or one whose coin went back, may keep a stale one, and
 * a carried coin keeps the choice it came from. */
A11yCharacter chosen(const A11yCssSlot& slot) {
    return slot.kind != A11Y_CSS_CLOSED && !slot.carried && slot.portrait >= 0 ? slot.character :
                                                                                 A11Y_NO_CHARACTER;
}

/* "Player 2: CPU". */
std::string slot_kind(const A11yCssState& state, int slot) {
    return player(slot) + ": " + std::string(slot_kind_word(state.slots[slot].kind));
}

/* A player slot's slider: the knob a hand reaches it at, its name there, the
 * shorter word said before its value while it is held, and the value. */
struct SliderKind {
    A11yCssSlider slider;
    TargetKind knob;
    std::string_view name;
    std::string_view held_word;
    int A11yCssSlot::* value;
};

constexpr SliderKind kSliders[] = {
    {A11Y_CSS_CPU_LEVEL, TargetKind::cpu_level, kCpuLevelSlider, kCpuLevelHeld,
        &A11yCssSlot::cpu_level},
    {A11Y_CSS_HANDICAP, TargetKind::handicap, kHandicapSlider, kHandicapHeld,
        &A11yCssSlot::handicap},
};

const SliderKind* slider_kind(A11yCssSlider slider) {
    for (const SliderKind& kind : kSliders) {
        if (kind.slider == slider) {
            return &kind;
        }
    }
    return nullptr;
}

const SliderKind* slider_kind(TargetKind knob) {
    for (const SliderKind& kind : kSliders) {
        if (kind.knob == knob) {
            return &kind;
        }
    }
    return nullptr;
}

/* What the local hand is on, for hover announcements: the slider it holds,
 * or the button or knob under a free hand. A free hand over a portrait is
 * silent, and a carried coin speaks for itself. */
Target hovered(const A11yCssState& state) {
    const A11yCssHand& hand = state.hand;
    if (!hand.present || hand.coin >= 0) {
        return Target{};
    }
    if (const SliderKind* held = slider_kind(hand.slider)) {
        return Target{held->knob, hand.slider_slot};
    }
    Target target = target_at(state, hand.x, hand.y);
    return target.kind == TargetKind::portrait ? Target{} : target;
}

}  // namespace

void CssSpeech::frame(const A11yCssState& state) {
    /* Leaving the rules screen or name entry builds the screen afresh inside
     * the same scene: an arrival too. */
    bool arrived = !m_seen || (m_last.exit == A11Y_CSS_AWAY && state.exit != A11Y_CSS_AWAY);
    A11yCssState last = m_last;
    m_seen = true;
    m_last = state;
    if (arrived) {
        m_pickup_pending = false;
        m_speech.announce(opening(state), Mode::interrupt);
        return;
    }
    /* Single-player modes have one hand and their own targets; until they are
     * read, the opening is all they say. */
    if (state.hand_count != A11Y_CSS_SLOTS) {
        return;
    }
    /* The rules screen and name entry open inside this scene. */
    if (state.exit != last.exit &&
        (state.exit == A11Y_CSS_TO_RULES || state.exit == A11Y_CSS_TO_NAME_ENTRY))
    {
        std::string out(state.exit == A11Y_CSS_TO_RULES ? kRulesScreen : kNameEntryScreen);
        add(out, kNoSpeechYet);
        m_speech.announce(out, Mode::interrupt);
    }
    if (state.exit != A11Y_CSS_STAYING) {
        return;
    }

    std::string mine;
    std::string others;
    add(mine, coin_announcement(last, state));
    add(mine, slider_announcement(last, state));
    Target was = hovered(last);
    Target is = hovered(state);
    slot_announcements(last, state, was, is, mine, others);
    if (is != was && is.kind != TargetKind::none) {
        add(mine, target_words(state, is));
    }
    if (state.slots[state.local_slot].name_tags_open &&
        !last.slots[state.local_slot].name_tags_open)
    {
        add(mine, kNameTagsWindow);
        add(mine, kNoSpeechYet);
    }
    if (state.ready && !last.ready) {
        /* After whoever made it ready. */
        add(mine.empty() && !others.empty() ? others : mine, kReadyToFight);
    }
    if (!mine.empty()) {
        m_speech.announce(mine, Mode::interrupt);
    }
    if (!others.empty()) {
        m_speech.announce(others, Mode::queue);
    }
}

std::string CssSpeech::opening(const A11yCssState& state) {
    A11yCharacter character = chosen(state.slots[state.local_slot]);
    return std::string(kScreenName) + ". " + player(state.local_player) + ", " +
           (character != A11Y_NO_CHARACTER ? name(character) : std::string(kNoCharacter)) + ".";
}

std::string CssSpeech::coin_announcement(const A11yCssState& last, const A11yCssState& now) {
    int was_carrying = last.hand.coin;
    int carrying = now.hand.coin;
    if (carrying >= 0 && carrying != was_carrying) {
        /* The frame a coin is picked up still shows where it lay; where it
         * is now, under the hand, shows a frame later. Speaking at once
         * would have that portrait cut the pickup off. */
        m_pickup_pending = true;
        return "";
    }
    bool pickup_pending = m_pickup_pending;
    m_pickup_pending = false;
    if (pickup_pending && carrying >= 0) {
        return holding(now, carrying);
    }
    if (was_carrying >= 0 && carrying < 0) {
        return dropped(last, now, was_carrying);
    }
    /* X and Y change the costume of the carried coin, or of one's own. */
    int slot = carrying >= 0 ? carrying : now.local_slot;
    const A11yCssSlot& before = last.slots[slot];
    const A11yCssSlot& after = now.slots[slot];
    if (carrying >= 0 && after.over_portrait != before.over_portrait) {
        /* Leaving every portrait says nothing. Entering one also resets the
         * costume, which goes unsaid. */
        return after.over_portrait >= 0 ? name(now.portraits[after.over_portrait].character) : "";
    }
    if (after.over_portrait >= 0 && after.over_portrait == before.over_portrait &&
        after.portrait == before.portrait && after.character == before.character &&
        after.costume != before.costume)
    {
        return std::string(kCostume) + " " + std::to_string(after.costume + 1);
    }
    return "";
}

std::string CssSpeech::holding(const A11yCssState& now, int coin) {
    std::string out = coin == now.local_slot ?
                          std::string(kHoldingYourCoin) :
                          std::string(kHoldingCoinBefore) + std::to_string(coin + 1) +
                              std::string(kHoldingCoinAfter);
    int over = now.slots[coin].over_portrait;
    if (over >= 0) {
        out += ". " + name(now.portraits[over].character);
    }
    return out;
}

std::string CssSpeech::dropped(const A11yCssState& last, const A11yCssState& now, int coin) {
    const A11yCssSlot& before = last.slots[coin];
    const A11yCssSlot& after = now.slots[coin];
    /* Carried down into the player slots. */
    if (after.portrait < 0 || after.character == A11Y_NO_CHARACTER) {
        return capitalised(kNoCharacter);
    }
    /* B puts the coin back on the character chosen before, which A cannot
     * do: A chooses the portrait under the coin, and a different portrait is
     * a different character. */
    if (after.character == before.character && after.portrait != before.over_portrait) {
        return std::string(kBackTo) + " " + name(after.character);
    }
    /* A choice: the game's announcer names the character. */
    return "";
}

std::string CssSpeech::slider_announcement(const A11yCssState& last, const A11yCssState& now) {
    const A11yCssHand& before = last.hand;
    const A11yCssHand& after = now.hand;
    std::string out;
    /* The value moves with the hand, and may move on the frame A lets go. */
    if (const SliderKind* held = slider_kind(before.slider)) {
        int was = last.slots[before.slider_slot].*held->value;
        int is = now.slots[before.slider_slot].*held->value;
        if (is != was) {
            add(out, std::string(held->held_word) + " " + std::to_string(is));
        }
    }
    if (after.slider != A11Y_CSS_NO_SLIDER && before.slider == A11Y_CSS_NO_SLIDER) {
        add(out, kHoldingSlider);
    } else if (after.slider == A11Y_CSS_NO_SLIDER && before.slider != A11Y_CSS_NO_SLIDER) {
        add(out, kReleased);
    }
    return out;
}

void CssSpeech::slot_announcements(const A11yCssState& last, const A11yCssState& now, Target was,
    Target is, std::string& mine, std::string& others) {
    /* A change is the local hand's when the hand is on the button that makes
     * it, or holds the slot's coin or slider; any other is another
     * player's, spoken only online. */
    auto on = [&](TargetKind kind, int slot) {
        return was == Target{kind, slot} || is == Target{kind, slot};
    };
    auto holds = [&](int slot) {
        return last.hand.coin == slot || now.hand.coin == slot || last.hand.slider_slot == slot ||
               now.hand.slider_slot == slot;
    };
    std::string theirs;
    for (int i = 0; i < A11Y_CSS_SLOTS; i++) {
        const A11yCssSlot& before = last.slots[i];
        const A11yCssSlot& after = now.slots[i];
        /* The local player's own slot also opens as their hand first
         * reaches the portraits, which the coin jumping in says. */
        bool own = i == now.local_slot;
        if (after.kind != before.kind) {
            if (on(TargetKind::slot_button, i)) {
                add(mine, slot_words(now, i));
            } else if (!own) {
                add(theirs, slot_words(now, i));
            }
            continue;
        }
        if (now.teams && last.teams && after.team != before.team) {
            std::string colour = team(after.team);
            if (on(TargetKind::team_button, i)) {
                add(mine, capitalised(colour) + " " + std::string(kTeam));
            } else {
                add(theirs, player(i) + " " + std::string(kTeam) + ": " + colour);
            }
        }
        /* The local hand's choices, for itself or a CPU, are left to the
         * game's announcer. */
        A11yCharacter character = chosen(after);
        if (!own && !holds(i) && character != A11Y_NO_CHARACTER && character != chosen(before)) {
            add(theirs, player(i) + ": " + name(character));
        }
    }
    if (now.teams != last.teams) {
        std::string words = target_words(now, Target{TargetKind::teams});
        add(on(TargetKind::teams, -1) ? mine : theirs, words);
    }
    if (now.online) {
        add(others, theirs);
    }
}

std::string CssSpeech::target_words(const A11yCssState& state, Target target) {
    int i = target.index;
    switch (target.kind) {
    case TargetKind::none:
        break;
    case TargetKind::portrait:
        return name(state.portraits[i].character);
    case TargetKind::slot_button:
        return slot_kind(state, i);
    case TargetKind::team_button:
        return player(i) + " " + std::string(kTeam) + ": " + team(state.slots[i].team);
    case TargetKind::cpu_level:
    case TargetKind::handicap: {
        const SliderKind* knob = slider_kind(target.kind);
        return player(i) + " " + std::string(knob->name) + ": " +
               std::to_string(state.slots[i].*knob->value);
    }
    case TargetKind::name_box:
        return player(i) + " " + std::string(kNameBox);
    case TargetKind::teams:
        return std::string(kTeamsButton) + ": " + std::string(state.teams ? kOn : kOff);
    case TargetKind::rules:
        return std::string(kRulesButton);
    case TargetKind::back:
        return std::string(kBackButton);
    }
    return "";
}

std::string CssSpeech::slot_words(const A11yCssState& state, int slot) {
    std::string out = slot_kind(state, slot);
    if (state.slots[slot].kind != A11Y_CSS_CLOSED) {
        A11yCharacter character = chosen(state.slots[slot]);
        out +=
            ", " + (character != A11Y_NO_CHARACTER ? name(character) : std::string(kNoCharacter));
    }
    return out;
}

std::string CssSpeech::name(A11yCharacter character) {
    if (std::optional<std::string_view> known = character_name(character)) {
        return std::string(*known);
    }
    if (m_logged_missing.insert(character).second && log_enabled()) {
        pc_log_line("[a11y] character %d: not in the character names table", character);
    }
    return std::string(kUnknownCharacter) + " " + std::to_string(character);
}

}  // namespace a11y
