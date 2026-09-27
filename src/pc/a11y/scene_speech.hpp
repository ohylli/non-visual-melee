/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Scene speech: names the scene the player has arrived in, once it has loaded,
 * from a table keyed by the scene's kind. Scenes to operate get their name and
 * "No speech yet.", things to watch their name and the way out, and matches
 * and the main menu tree nothing (.scratch/main-menu-tree/spec.md, "Scene
 * announcements"). */
#pragma once
#include "scene_kinds.h"
#include <optional>
#include <set>
#include <string_view>

namespace a11y {

class Speech;

enum class SceneKind : int {
#define A11Y_SCENE_KIND_ENUMERATOR(name, number) name = number,
    A11Y_SCENE_KINDS(A11Y_SCENE_KIND_ENUMERATOR)
#undef A11Y_SCENE_KIND_ENUMERATOR
};

/* What is said on entering a scene of this kind: empty for a silent scene,
 * nothing for a kind missing from the table. */
std::optional<std::string_view> scene_announcement(int scene_kind);

class SceneSpeech {
public:
    /* speech must outlive this. */
    explicit SceneSpeech(Speech& speech) : m_speech(speech) {}

    /* A scene has loaded and its first frame is next. Every arrival is
     * announced, even in the scene just left: character select after results
     * is a new arrival. A kind missing from the table stays silent and is
     * logged, once per kind. */
    void entered(int scene_kind);

private:
    Speech& m_speech;
    std::set<int> m_unknown_logged;
};

}  // namespace a11y
