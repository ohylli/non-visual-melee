/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage select's string table: the stages' names as the English screen draws
 * them for the hovered tile, so the blind player hears what a sighted player
 * reads (.scratch/stage-select/spec.md, "Stage names and descriptions"). The
 * game draws them as pictures, so there is no game text to read. The picture
 * has two lines, a small one over the name in large letters: the small line
 * is the stage's world ("Eagleland" over "Onett"), and is part of the name
 * only where the two read as one ("Hyrule" over "Temple", "Mushroom" over
 * "Kingdom"). The three stages under "Past Stages" share their names with
 * newer ones, so they add "N64" ("Dream Land N64"). Each stage also has a
 * description of a sentence or two, said after its name on every hover: the
 * ground's shape, what moves or harms, its feel. The fork wrote them from the
 * tiles' pictures (the screen shows no larger preview) and from the stages'
 * code in src/melee/gr/, and says nothing either does not show. Every other
 * word stage select speech says is here too. */
#pragma once
#include "stage_kinds.h"
#include <optional>
#include <string_view>

namespace a11y {

/* A stage's name; nothing for none or a kind missing from the table. */
std::optional<std::string_view> stage_name(A11yStage stage);
/* A stage's description, one or two sentences ending in a full stop;
 * nothing for none or a kind missing from the table. */
std::optional<std::string_view> stage_description(A11yStage stage);

/* The opening: the screen's name, then how to choose. */
inline constexpr std::string_view kStageSelect = "Stage select";
inline constexpr std::string_view kHowToChoose = "D-pad to choose, Start picks a random stage.";
/* The skipped scene, where the stage-select rule Random or Ordered chose
 * the stage: "Stage: Corneria." */
inline constexpr std::string_view kStage = "Stage";
/* The stage cursor answers to another player's controller (the Turns and
 * Loser rules): "Player 3 chooses. Steering needs controller 1." */
inline constexpr std::string_view kPlayer = "Player";
inline constexpr std::string_view kChooses = "chooses";
inline constexpr std::string_view kSteeringNeedsPort1 = "Steering needs controller 1.";

/* The random tile, and a locked one, drawn as "?": its stage is not named. */
inline constexpr std::string_view kRandom = "Random";
inline constexpr std::string_view kLockedStage = "Locked stage";
/* A or Start on a locked tile, which the game refuses. */
inline constexpr std::string_view kLocked = "Locked.";
inline constexpr std::string_view kUnknownStage = "Unknown stage";
/* A pick of another stage than the one last named, made during a glide:
 * "Picked Fountain of Dreams." */
inline constexpr std::string_view kPicked = "Picked";
/* A glide that did not get there: "Could not reach Corneria". */
inline constexpr std::string_view kCouldNotReach = "Could not reach";

}  // namespace a11y
