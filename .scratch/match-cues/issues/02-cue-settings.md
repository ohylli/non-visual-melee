# 02 Cue settings and the tuning file

Status: ready-for-agent
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
