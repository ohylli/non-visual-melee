# 04 Drive scripts, the verify skill, the docs and the status line

Status: resolved (2026-10-28)
Type: task
Blocked by: 03

Spec: `.scratch/voices/spec.md`, "Tests" and "Docs".

## What to do

- **The drive scripts.** Keep issue 03's port menu run as a script beside the other port menu scripts, and add the launcher's Settings: step a Voice row and press Preview there too.
- **The `verify` skill.** The scripts, the developer switch from issue 02, and their expected lines, at the level each belongs to.
- **The docs.** Finish `docs/a11y/speech.md`'s "Voices" section (the two voices, rendering and playing, queueing, the switches, missing voices, the answers to the spec's open questions) and extend its mental model; `docs/a11y/cues.md` and `docs/a11y/rmlui-screens.md` as the spec's "Docs" has them.
- **CLAUDE.md.** One "Accessibility status" line for the voices, naming what still waits: the play test, `.scratch/voices/issues/05-play-test.md`.

## Done when

- The `verify` skill's menus level runs the new scripts and they pass.
- The docs and the status line are in place, with no values or counts that tuning would change.

## Comments

### 2026-10-28, implementation (agent)

Done; issues 02 and 03 had already written most of the docs and the port menu's script, so this issue added the launcher's script and filled the gaps.

- **The drive scripts.** `tools/a11y/launcher_voices.drive` (new): the launcher's Accessibility category, Your voice a step Right and back, Your voice volume a step down and back, Preview your voice and Preview opponents' voice pressed with Enter, each change checked in `a11y.ini`. In `run_level.py`'s menus set as `--launcher --muted` with `A11Y_ROWS`, and counted like the port menu's: five samples in your voice and one in the opponents' voice. Issue 03's `port_menu_voices.drive` is kept as it was; the counts' patterns are now shared between the two.
- **The `verify` skill.** A bullet for `launcher_voices.drive` beside the port menu's; the voices named among what only the ear can judge ("Out of reach"); two values that tuning would change taken out (the port menu script's rate after a step, and the default voices' names in the bounded run's lines). `MELEE_A11Y_VOICE_PREVIEW` was already under "Bounded runs" with its dump check, where a developer switch's run belongs; left there.
- **The docs.** `docs/a11y/speech.md`: the mental model gains the voices' independence, the queueing in one sentence, their place under the switches and the volume chain, the rows, and the fallbacks; the rows' paragraph no longer names the default voices or the Rate slider's number for normal speed, and points to "The choice row" for open question 4. `docs/a11y/cues.md`: the mental model's volume chain now names the once voice's own chain. `docs/a11y/rmlui-screens.md` already had "The choice row" and the voices' Preview; unchanged.
- **CLAUDE.md.** A "Voices" status line after Results, naming the play test as what waits; the rows stay on the "Accessibility settings" line.
- **A finding.** In the launcher run, Preview pressed less than a quarter second after the volume step's sample began cut that sample (`voice yours cut: "Your voice"`) and played its own, as a sample about the same subject should; noted in the skill so a reader of the log does not take it for a fault.
- **Verification**, menus level: build with the "Rollback sections" line; style; unit tests 33 of 34 (`launcher_data`, known); a bounded `title` run with `MELEE_A11Y_SCREEN_READER=0` (the voices ready, then the title screen); `run_level.py menus` 28 of 28, `launcher_voices` among them.
- **Left open.** The play test, issue 05.
