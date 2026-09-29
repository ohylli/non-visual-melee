/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene_speech.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <iterator>

namespace a11y {
namespace {

struct SceneWords {
    SceneKind kind;
    std::string_view words;
};

/* Every kind the decomp knows, each once; the wording is a first proposal for
 * the play test to judge. */
constexpr SceneWords kScenes[] = {
    /* Scenes to operate. */
    {SceneKind::GS_CSS, "Character select. No speech yet."},
    {SceneKind::GS_SSS, "Stage select. No speech yet."},
    {SceneKind::GS_RESULTS, "Results. No speech yet."},
    {SceneKind::GS_TOY_GALLERY, "Trophy gallery. No speech yet."},
    {SceneKind::GS_TOY_LOTTERY, "Trophy lottery. No speech yet."},
    {SceneKind::GS_TOY_COLLECTION, "Trophy collection. No speech yet."},
    {SceneKind::GS_TOU_SETUP, "Tournament setup. No speech yet."},
    {SceneKind::GS_TOU_BRACKET, "Tournament bracket. No speech yet."},
    {SceneKind::GS_TOU_ALT, "Tournament bracket. No speech yet."},
    {SceneKind::GS_ONLINE_LOBBY, "Online lobby. No speech yet."},
    {SceneKind::GS_PRIZE_INTERFACE, "Unlock notice. No speech yet."},
    {SceneKind::GS_GAMEOVER, "Continue screen. No speech yet."},
    /* A notice shown before a Camera Mode match, not the match itself. */
    {SceneKind::GS_CAMERA_VS, "Camera Mode notice. No speech yet."},

    /* Things to watch. */
    {SceneKind::GS_MOVIE_OPENING, "Opening movie. Press Start to skip."},
    {SceneKind::GS_TITLE, "Title screen. Press Start."},
    {SceneKind::GS_MOVIE_HOWTO, "How to play movie"},
    {SceneKind::GS_MOVIE_OMAKE15, "Special movie"},
    {SceneKind::GS_MOVIE_END, "Ending movie"},
    {SceneKind::GS_APPROACH, "New challenger"},
    {SceneKind::GS_INTRO_EASY, "Stage intro"},
    {SceneKind::GS_INTRO_NORMAL, "Stage intro"},
    {SceneKind::GS_CUTSCENE_LUIGI, "Cutscene"},
    {SceneKind::GS_CUTSCENE_BRINSTAR, "Cutscene"},
    {SceneKind::GS_CUTSCENE_EXPLOSION, "Cutscene"},
    {SceneKind::GS_CUTSCENE_3KIRBYS, "Cutscene"},
    {SceneKind::GS_CUTSCENE_GIANTKIRBY, "Cutscene"},
    {SceneKind::GS_CUTSCENE_STARFOX, "Cutscene"},
    {SceneKind::GS_CUTSCENE_FZERO, "Cutscene"},
    {SceneKind::GS_CUTSCENE_METAL, "Cutscene"},
    {SceneKind::GS_CUTSCENE_BOWSERTOY, "Cutscene"},
    {SceneKind::GS_CUTSCENE_GIGATRANSFORM, "Cutscene"},
    {SceneKind::GS_CUTSCENE_GIGADEFEATED, "Cutscene"},
    {SceneKind::GS_REGEND_TOYFALL, "Trophy award"},
    {SceneKind::GS_REGEND_CONGRATS, "Congratulations"},
    {SceneKind::GS_STAFFROLL, "Credits"},

    /* Silent. The main menu tree speaks its own opening. */
    {SceneKind::GS_MENU, ""},
    /* Matches, including the attract demo: the game's announcer speaks. */
    {SceneKind::GS_VS, ""},
    {SceneKind::GS_SUDDEN_DEATH, ""},
    {SceneKind::GS_TRAINING, ""},
    /* Passes unseen when the save loads, and its kind cannot tell; reading
     * its prompts is a later feature. */
    {SceneKind::GS_MEMCARD, ""},
    {SceneKind::GS_PROG_SCAN, ""},
    {SceneKind::GS_DEBUG_MENU, ""},
    {SceneKind::GS_COMING_SOON, ""},
    /* Unused by the game. */
    {SceneKind::GS_0x6, ""},
    {SceneKind::GS_UNK10, ""},
    {SceneKind::GS_INTRO_ALLSTAR, ""},
};

constexpr SceneKind kAllKinds[] = {
#define A11Y_SCENE_KIND_ITEM(name, number) SceneKind::name,
    A11Y_SCENE_KINDS(A11Y_SCENE_KIND_ITEM)
#undef A11Y_SCENE_KIND_ITEM
};

constexpr bool table_has_every_kind_once() {
    for (SceneKind kind : kAllKinds) {
        int found = 0;
        for (const SceneWords& scene : kScenes) {
            found += scene.kind == kind ? 1 : 0;
        }
        if (found != 1) {
            return false;
        }
    }
    return std::size(kScenes) == std::size(kAllKinds);
}
static_assert(table_has_every_kind_once(), "kScenes needs every scene kind, each once");

}  // namespace

std::optional<std::string_view> scene_announcement(int scene_kind) {
    for (const SceneWords& scene : kScenes) {
        if (static_cast<int>(scene.kind) == scene_kind) {
            return scene.words;
        }
    }
    return std::nullopt;
}

void SceneSpeech::entered(int scene_kind) {
    std::optional<std::string_view> words = scene_announcement(scene_kind);
    if (!words) {
        if (m_unknown_logged.insert(scene_kind).second && log_enabled()) {
            pc_log_line("[a11y] scene %d: not in the scene table, silent", scene_kind);
        }
        return;
    }
    if (!words->empty()) {
        m_speech.announce(*words, Mode::interrupt);
    }
}

}  // namespace a11y
