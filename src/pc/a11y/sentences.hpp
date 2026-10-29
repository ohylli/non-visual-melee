/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <string>
#include <string_view>

/* Joining the parts of one announcement, shared by every reader. An empty
 * part is left out, so a reader can pass words it may not have. */

namespace a11y {

/* Whether the words end a sentence themselves, as the rules header's text
 * does ("2-minute KO fest!"). */
inline bool ends_sentence(std::string_view words) {
    return !words.empty() && (words.back() == '.' || words.back() == '!' || words.back() == '?');
}

/* Appends a part as its own sentence: "Main Menu. 1-P Mode. Solo Smash!" A
 * part that ends a sentence itself keeps its own mark. */
inline void add_sentence(std::string& out, std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!out.empty()) {
        out += ends_sentence(out) ? " " : ". ";
    }
    out += part;
}

/* Appends a part to the sentence under way, after a comma: "P2, you". */
inline void add_part(std::string& out, std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!out.empty()) {
        out += ", ";
    }
    out += part;
}

/* Ends the text with a full stop unless it ends a sentence already. */
inline void end_sentence(std::string& out) {
    if (!out.empty() && !ends_sentence(out)) {
        out += '.';
    }
}

}  // namespace a11y
