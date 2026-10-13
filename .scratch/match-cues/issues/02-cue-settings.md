# 02 Cue settings and the tuning file

Status: resolved (2026-10-13)
Type: task
Blocked by: 01

The model of named values that switch, set the loudness of and tune the cues, with a text file as its first front end. Read the spec's "Cue settings" and "The volume chain" first.

## Machinery

- **The model**: values each with a key, a default, a range, a unit and a one-line description, in groups. A cue registers its own values (its issue defines them), so the model knows nothing about any cue. Code reads a value through a typed handle, not by looking up a string each tick. The model is the part the port menu will render later: it must be enough to draw a switch or a slider for every value.
- **Values defined here**: all cues on or off; cue volume, 0 to 100%. Per cue, registered by a helper every cue uses: on or off, its own volume 0 to 100%, its boost.
- **The file front end**: plain text, `key = value` a line, grouped by cue under `#` comment headings, every value preceded by its description as a comment.
  - Path: `a11y-cues.ini` in `SDL_GetPrefPath(NULL, "melee-pc")`, where the base port keeps its settings; `MELEE_A11Y_CUE_SETTINGS=<path>` overrides it.
  - A missing file is written with every value at its default.
  - Values the code knows but the file lacks (a newer cue) take their defaults; keys the file has but the code does not are logged once and kept.
  - Read at engine start, then checked about twice a second through a `poll()` called from scene entry here and from the match tick hook in issue 03. Only a changed file is parsed, and only changed values are applied.
  - A line that cannot be read is logged and spoken ("Cue settings, line 12 not understood."), keeping that value's last setting; the other lines apply. Out of range is clamped, and logged.
- **Into the engine**: the all-cues switch, the cue volume and each cue's switch, volume and boost feed the volume chain of issue 01. A cue switched off stops its voices with the ramp.
- **Agent runs**: `drive.py` and `drive_net.py` set `MELEE_A11Y_CUE_SETTINGS` to a file in their run's scratch folder, so no agent run reads or writes the maintainer's file.

## Done when

- Unit tests: a missing file is written with defaults and descriptions; parsing; clamping; an unreadable line keeps its last value and is reported with its line number; a reload applies only what changed; an unknown key is kept; a newly registered value takes its default.
- A bounded run with a scratch settings path writes the default file and logs its path.
- `python tools/check_style.py` passes.
- The `verify` skill gains `MELEE_A11Y_CUE_SETTINGS` and notes that the drivers set it.
- `docs/a11y/cues.md` gains a section on cue settings: the model, the file, where it lives, live reload, and that the port menu will be a second front end.
- `CONTEXT.md` gains "Cue settings".

## Comments

### 2026-10-13, implementation (agent)

Implemented; no cue registers values yet (issue 03 adds the first).

- **Where things live.** `settings.hpp/.cpp`: the model (groups of `Setting`s with key, label, description, unit, default, range and slider step; `Switch` and `Number` handles; `set` for a front end) and the text format both ways (`to_text`, `read_text`), with no file IO. `settings_file.hpp/.cpp`: the file front end (`load`, `poll`, logging, the spoken error). `cue_settings.hpp/.cpp`: the all-cues values, the per-cue helper `add_cue` and `apply` into the engine. `hooks.cpp` owns them (`s_settings`, `s_cue_settings`, `s_settings_file`), made in `open_cue_settings` once the engine has opened; `Config` gained `cue_settings` (`MELEE_A11Y_CUE_SETTINGS`).
- **The API, for 03.** In `open_cue_settings`, before the file is read: `a11y::CueGroup cue = s_cue_settings->add_cue("your_position", "Your position cue", boost);` registers the group with its switch, volume (0 to 100%) and boost (1 to 8, a factor) and hands out the next cue number (`cue.values.cue`, for the `VoiceSpec`). The cue then adds its mapping values with `cue.group.add_number(NumberSpec{name, label, description, unit, default, min, max, step})` and keeps the `Number` handles; `.value()` reads the current value, no lookup. The match tick hook calls `poll_cue_settings()` (in `hooks.cpp`'s anonymous namespace), which applies changed levels to the engine itself; a driver just reads its handles each tick.
- **A cue switched off** is its level off: the engine glides it to silence over its 10 ms gain glide while its voices keep running, so switching it back on resumes them where they are. No `stop` is logged for it. A driver may still check `cue.values.on.on()` to skip its work, but then it must also stop and restart its voices itself.
- **Missing values are appended.** Beyond the issue: when the file lacks values the code has (a newer cue), `load` appends them with their descriptions under their group's heading at the end of the file, so the maintainer finds a new cue's values without deleting their tuned file; nothing they wrote is touched. Only at `load`: a line removed while the game runs just goes back to its default. Unknown keys stay in the file and in the model (`Settings::unknown`, written back by `to_text` under a heading of their own), logged once per key.
- **Parsing.** Switches take on, off, yes, no, true, false, 1, 0; numbers may carry their unit (`80%`, `0.5 s`), a leading `+` and a decimal comma; `#` starts a comment anywhere; a UTF-8 byte order mark (Notepad) and CRLF are fine. Numbers are read and written with `from_chars`/`to_chars`, so the locale never matters, and written in their shortest form (`0.7`, not `0.699999988`). Each value's comment ends with its range and default ("0 to 100%, default 100%.").
- **Reload.** `poll` looks every 500 ms at most: the file's write time and size first, then the text; only a changed text is parsed, and only values that changed are applied and logged (`[a11y] cue settings: cues.volume = 40`). A deleted file is written again with the defaults, which then apply. The errors of one read are one announcement ("Cue settings, lines 2 and 4 not understood."), queued, spoken once per change of the file.
- **Errors can go unheard; open for 03.** Speech at startup is cut off by the launcher's, and at scene entry by screens that speak their opening on their first frame (the main menu tree, character select). Scene entry now polls after the scene's own announcement, which helps only for scenes announced there. `SettingsFile::problem()` keeps the last read's error text; issue 03 could queue it when a match starts, where the maintainer tunes and listens. In a match the tick hook's poll speaks into quiet.
- **Drivers.** `drive.py` (and so `drive_net.py`, through its `Game`) sets `MELEE_A11Y_CUE_SETTINGS` to `<out>.cues.ini` beside the transcript, removed before the run so each starts from the defaults. A non-empty `MELEE_A11Y_CUE_SETTINGS` in the environment is passed through instead, for a run with tuned values. An empty one counts as unset: a shell slip of mine once passed an empty value, and the game, which takes empty for "the default path", wrote a defaults file into the maintainer's `%APPDATA%\melee-pc`; it was new (the log said "written with defaults"), and I removed it. The `verify` skill now warns about this for hand-started runs.
- **Verification**, online level, with `MELEE_A11Y_CUES=1` exported so the settings code ran in every scripted run: build with the "Rollback sections" line; style; unit tests (`settings` new, ten tests over files in `build/settings_test`; `launcher_data` fails as known); two mutations (a bad line falling back to the default, every value reported as changed) each fail a test; a bounded `title` run with a scratch path writes the default file and logs its path; a `main_menu.drive` run whose file had an out-of-range value, a bad line and an unknown key, edited mid-run to another bad line and a new volume, logged the clamp, both errors and their (off) announcements, the unknown key once, and the reload of `cues.volume` at the next scene; the sixteen menu drive scripts; the five `css_net.drive` runs and `lan_lobby.drive`, no desync. All passed on the first run, each with its own settings file, and the maintainer's folder was checked to have none afterwards.
