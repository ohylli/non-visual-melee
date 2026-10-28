# 05 Play test by ear

Status: ready-for-human
Type: task
Blocked by: 04

The maintainer chooses and listens to the voices with NVDA and judges `.scratch/voices/spec.md`.

## What to try

- **Choosing.** Step through the Voice rows in the launcher's Settings and in the port menu. Is the sample's delay after NVDA's words right? Do the voices' names read well, and is the language needed? Should voices of other languages (a Finnish one here) be listed at all?
- **Rate and volume.** Step each slider across its range. Is the normal speed a good default? Is a full-volume voice level with the game and NVDA?
- **Preview.** Press each Preview with NVDA talking. Do the voices stay clear over NVDA and each other?
- **The fallbacks.** If convenient, pick a voice and remove it (or edit `a11y.ini` to a name not installed): is the startup line clear?
- **The voices' sound.** Any clicks at a line's start or end, uneven loudness between lines, or delay before a line starts?

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and CLAUDE.md's status line says the voices were play-tested. The voices in a match are judged by `.scratch/match-announcements/issues/06-play-test.md`.

## Comments

### 2026-10-28, code review (agent)

Review of issues 01 to 04 (`dd1dd0c..10b0856`); the plain fixes are in their own commit. Left for the maintainer, most important first:

- **The startup news can be cut off.** In the game, "Zira not installed, using …" or "Voices unavailable, using the screen reader." waits for the first scene's announcement (`VoiceSettings::scene_announced`, called from `pc_a11y_scene_entered`), but that call comes after every scene, silent ones too. When the first scene is silent (the memory card scene on a real start), the news is queued there and the next scene's opening interrupts it, never to be said again. Options: call it only once a scene has actually spoken, or say the news again later as the settings file's problems are. Recommendation: the first; check by ear which scene a real start says first.
- **A render that fails mid-run drops its line unsaid** (`Voices::take_news`): only the log knows. Options: send that one line to the screen reader, or leave it, since a render that worked at startup rarely fails. Recommendation: send it to the screen reader once the match announcements land.
- **The Rate and Volume rows' samples while the voices are unavailable** go to the screen reader ("Your voice"), where rate and volume change nothing. Options: no sample there, or keep it as the change's confirmation. Recommendation: no sample.
- **A voice's sample is not stopped when focus leaves its Preview row**, unlike a cue's preview: it is a line of a second and plays out (now said in `settings_page.hpp`). Stop it too if that is heard as wrong.
- **`prism_shutdown` skipped at exit** (`PrismVoiceBridge::close`). Sound as it stands: the voices close only as the game exits, the worker frees its instances and is joined, and the process ends the context. A likely cause of the hang is that the voices' context runs Prism's availability polling, which may call into the worker's single-threaded apartment, which pumps no messages once it waits; turning polling off in the voices' `PrismConfig` might let the shutdown return. Worth trying only if the voices ever need to close mid-run.
- **The `after_speech` hand-off** (`Switches`, `hooks.cpp`) builds the voices' rows inside the switches' constructor so they land after Speech. It works, but the row order now hangs on a side effect, and the next feature wanting rows between switches needs another hand-off. Options: split the switches' registration in two steps, or let a group be registered after a named one. Recommendation: the two steps, when the match announcements add their rows.
- **"Voice" has two meanings**: CONTEXT.md's voice (your voice, the opponents' voice) and the cue engine's playback voice ("once voice", `CueEngine::Voice`). A CONTEXT.md line for the engine's sense, or "engine voice" where both meet, would settle it.
- **`NamedValue` carries the choice's six fields beside the number's**, and several functions switch on the kind; gathering a choice's fields into one member would show the kind-specific state in one place. A judgement call; it works as it is.
