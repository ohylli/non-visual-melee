# 01 A choice kind of value in the accessibility settings

Status: resolved (2026-10-28)
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

## Comments

### 2026-10-28, implementation (agent)

Implemented; no row is shown in the game yet, so the choice is exercised by the unit tests alone.

- **Where things live.** `value_model.hpp/.cpp`: `ValueKind::choice`; `ChoiceOption` (`text`, saved by the file, and an optional `label`, shown by the row); `ChoiceSpec` and `ValueGroup::add_choice`, returning the typed handle `Choice` (`option()`, the option in use; `saved()`, the file's text; `options()`); the free functions `choice_options`, `choice_index` and `choice_step`; `ValueModel::choose`. The choice's state sits in `NamedValue` (`text`, `default_text`, `options`, `list`, `fallback`); its numbers stay 0. `value_file.cpp` and `accessibility_settings.cpp`: `choose` beside `set`, sharing one path (read the file first, change the model, save the line, tell the listener). `settings_page.cpp`: the choice row.
- **How 03 registers the Voice rows.** Options asked at run time come from `ChoiceSpec::list`, a `std::function<ChoiceOptions()>` asked every time the options are needed, every frame of an open page among others, so it should hand back a copy of a list the voice engine keeps. The fallback, `ChoiceSpec::fallback`, gets the saved text and the options and returns an option's text: "Zira" as the default text with a fallback that matches by part of a name, then names the Windows default voice, gives the spec's default and its missing-voice rule in one. With designated initializers: `group.add_choice({.name = "voice", .label = "Your voice", .description = ..., .default_option = "Zira", .list = [this] { return m_voices; }, .fallback = ..., .shown = true})`. The row shows an option's `label` when set, so the language suffix goes there and the display name in `text`. Read the voice in use with `Choice::option()`; `Choice::saved()` tells a missing voice ("Zira not installed, ..."). A change from a row or a hand edit reaches the settings' listener like any other value.
- **The file.** A choice's line holds its text as written; the description lists fixed options ("Off, Every change or Thresholds, default Every change.") and leaves a run-time list unnamed ("One of the options the menus list, default Zira."), so the file never depends on what was installed when it was written. A saved text that names no option stays, in the model and the file, until the player picks another; the option in use is then the fallback's, else the default's, else the first. A fixed choice's unlisted text is logged ("damage.yours = Sometimes is none of the options; using Every change"), a run-time list's is not (its options may not be known yet, and 03 announces a missing voice itself).
- **Open question 4, the row's markup.** The base port's own choice rows (Aspect ratio, Anisotropic filtering, Post-processing, Graphics backend, Input delay) are a `button.value` showing the option, stepped by Left and Right through `Launcher::cycle` and `PortMenu::apply`, wrapping at either end. The choice row uses that markup, so the readers needed nothing new: they read it as a setting, "Test voice, Microsoft Mark, Left and right to change." and the help, then the option alone after a step. Recorded in `docs/a11y/rmlui-screens.md`, "The choice row".
- **Choices beyond the issue.** An option's text names it ignoring ASCII case, so a hand edit of "off" names "Off". An empty value is a line not understood, keeping the last text. Enter, A or a click steps to the next option, as the base port's choice rows do; the port menu plays its confirm sound, as for its own. A choice with no options listed (OneCore unavailable) shows "None" and a step does nothing; 03 may want other words there. A choice has no environment override: `override_value` refuses it.
- **Tests.** `accessibility_settings`: saved as text with its description, a hand edit (any case, a comment kept, an empty value), a saved option not listed kept through other saves with the fallback in use (the default's, a registration's own, one naming nothing), options asked at run time changing between reads (uninstalled, reinstalled elsewhere in the list, none), and a step at each end. `launcher_speech` and `port_menu_speech`: a test choice registered last in the shared fixture, with options asked at run time; the category walks include it, and each host steps it both ways and round, presses it, and (launcher) follows the options changing while the page is open.
- **Verification**, quick level plus the four RmlUi menu scripts: build with the "Rollback sections" line; style; unit tests (`launcher_data` failing as known); a bounded `title` run; `run_level.py launcher_a11y port_menu_a11y launcher_settings port_menu`, 4 of 4 passing.
- **Left open.** Nothing for this issue; the first rows are 03's.
