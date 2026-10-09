# 02 The skipped scene names the stage

Status: resolved (2026-10-09)
Type: task
Blocked by: 01

Spec: `.scratch/stage-select/spec.md`, "The skipped scene".

With the VS stage-select rule set to Random or Ordered, `gm_80167FC4` sets the stage in advance and stage select leaves on its first frame. The scene announcement probably still fires there; not yet checked in a run.

- First confirm in a run that the announcement fires.
- Find where the chosen stage can be read at that moment (the match's rules, `sss_data->vs.start.rules.stkind`, or the forced stage) and say "Stage: Corneria." instead of the opening, from issue 01's stage name table.
- The opening must not be said as well.

## Done when

- A unit test covers the skipped scene's words and the absence of the opening.
- A drive run with the rule set to Random (VS mode's rules screen) logs "Stage: ..." and the match, and no "Stage select." line. If the script joins verification, the `verify` skill lists it.

## Comments

### 2026-10-09, implementation (agent)

Implemented; hearing it is issue 07.

- **No new hook.** As issue 01 found, `pc_a11y_sss_frame` runs on the skipped scene's one frame, before its early return, and `sss_data->force_stage_id` already holds the stage there; `a11y_game_sss_state` reports it as `skipped` with `skipped_stage`. A run confirmed it: the hook fires once, and the scene announcement no longer fires for stage select at all (issue 01 took stage select out of the scene table), so nothing else is said.
- **Speech.** `SssSpeech::frame` treats the skipped snapshot as the arrival: "Stage: Corneria." in place of the opening, once, interrupting; the opening is never said. The stage's name comes from issue 01's table; a stage missing from it is "Stage: Unknown stage 21." and logged, as on hover. `kStage` joins `sss_names.hpp`.
- **Tests.** `test_sss_speech`: the stage named instead of the opening, said once over repeated frames, a stage missing from the table, and stage select's opening on the next arrival after a skipped scene (the rule set back to On).
- **Drive.** `tools/a11y/sss_skipped.drive` sets VS mode's Stage Selection rule to Random on the rules screen (stick Down four rows, Right once; a screenshot shows "RANDOM", "Stages chosen randomly."), backs out to character select and presses Start: "Stage: Mute City.", "Stage: Kongo Jungle.", "Stage: Pokémon Stadium." in three runs, each followed by the match, and no line said "Stage select". A screenshot of the second match showed Kongo Jungle. drive.py has no step for a line that must be absent, so the `verify` skill's entry for the script says to grep the transcript; it joins the menu drive scripts (now eleven).
- **Ordered** takes the same path (`gm_80167FC4` sets `force_stage_id` for it too) and was not run.
- **Ranked online** sets `force_stage_id` from the ranked session's stage in `onEnterSss`; if such a scene reaches the hook rather than the ranked list, it would say "Stage: ..." too, which seems right. Not run.
- **Verification**, quick level plus both stage select scripts: build, style, unit tests (`sss_speech` updated; `launcher_data` fails as known), a bounded `title` run, `sss_hover.drive` and `sss_skipped.drive` passed.
