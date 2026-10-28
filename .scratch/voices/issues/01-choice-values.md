# 01 A choice kind of value in the accessibility settings

Status: ready-for-agent
Type: task

Spec: `.scratch/voices/spec.md`, "A choice kind of value". Read it first, then `docs/a11y/cues.md` ("Accessibility settings"), ADR-0007 and `docs/a11y/rmlui-screens.md`.

## What to build

- **The model.** A third `ValueKind` beside toggle and number: one of a list of named options. A typed handle like `Switch` and `Number` reads the option in use. Options come either from the registration (fixed, as the match announcements' damage rows will have) or from a function asked at run time (the Voice rows' installed voices).
- **The file.** `a11y.ini` saves the option's text, not an index. A saved text not among the options stays in the file unchanged until the player picks another; the value in use is then the setting's fallback (the registration names it). A changed choice is saved to its own line, as numbers are; hand edits are picked up by `poll()`.
- **The rows.** `settings_page.cpp` builds a choice row in both hosts, launcher Settings and the port menu, in the base port's own markup for a row that Left and Right step (spec, open question 4). Stepping wraps or stops at the ends as the base port's own choice rows do.
- **The readers.** `rmlui_reader.cpp` reads a choice row as it reads a slider: "label, option", and the option alone after a step.
- **Environment overrides** stay for switches only; a choice has none.

No row is shown yet: issue 03's Voice rows are the first. Test with a choice registered by the unit tests alone.

## Done when

- The accessibility settings' unit test covers: saved as text; a hand edit picked up; a saved option not in the list kept in the file and the fallback in use; options that change between reads; a step at each end.
- The rows' and readers' tests cover a choice row in both hosts.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
- `docs/a11y/rmlui-screens.md` names the choice row and its markup; the answer to open question 4 is there.
