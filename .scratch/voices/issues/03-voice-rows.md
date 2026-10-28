# 03 The voice rows and the fallbacks

Status: ready-for-agent
Type: task
Blocked by: 01, 02

Spec: `.scratch/voices/spec.md`, "Choosing a voice", "The rows" and "Missing voices".

## What to build

- **The rows**, after the Speech switch in the Accessibility category, your voice's first: "Your voice" (a choice of the installed OneCore voices, sorted by language then name, saved by display name), "Your voice rate" (a slider, the voice's normal speed by default), "Your voice volume" (a slider, full by default) and "Preview your voice" (a button); then the same four for "Opponents' voice". Descriptions are the implementer's first proposal.
- **The options' words.** The display name alone, or with its language after it ("Microsoft Mark, English (United States)") when the installed voices span more than one language.
- **Samples.** A step on a Voice, Rate or Volume row plays "Your voice" or "Opponents' voice" in that voice at the new value, a moment after the step (a tuning value); a quick run of steps plays one sample, for the value it settles on. Preview plays the same at once.
- **A missing voice.** A saved voice not installed, or a default with no match: the Windows default voice is used, the file keeps the saved name, the row shows the voice in use, and once per run, after the proof-of-life, the screen reader says "Zira not installed, using Microsoft David."
- **OneCore unavailable.** Voice lines go to the screen reader, queued, and the screen reader says once "Voices unavailable, using the screen reader."
- **Changes take effect at once**, from either host and from a hand edit of `a11y.ini`.

## Done when

- Unit tests cover the rows' options and words (one language and several), the samples' delay and settling, the missing voice and its announcement, the file keeping a missing name, and OneCore unavailable.
- A drive run opens the port menu, steps both Voice rows and a Rate row and presses Preview; the readers' lines and the voice lines are in the log.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
