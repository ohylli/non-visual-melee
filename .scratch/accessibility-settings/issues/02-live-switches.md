# 02 Live switches for accessibility, speech, steering and the cues

Status: ready-for-agent
Type: task
Blocked by: 01

`.scratch/accessibility-settings/spec.md`, "Switches" and "Environment variables". The switches move from `config_from_environment` (`speech.cpp`) into the model and become changeable while the game runs.

## Machinery

- **Values.** `accessibility.on`, `speech.on` and `steering.on` (names the implementer's to settle) join the model as its first group, ahead of `cues`, all shown, default on.
- **Hierarchy.** Accessibility over Speech, Steering and Cues; Cues over each cue's switch. A higher switch overrides without changing the saved values below it.
- **Live.** Every reader of `a11y::Config` that copied it at startup reads the live value instead. The cue engine is always created; Cues off mutes it. Accessibility off silences the readers and scene speech, stops the cues and stops steering. Steering off mid-glide returns the stick to centre that frame (spec open question 6).
- **The last announcement.** Turning Accessibility or Speech off says "Accessibility off" or "Speech off" before going quiet (spec open question 4). Turning either back on announces the switch's new value, and the reader then announces the focused row (open question 5).
- **Overrides.** `MELEE_A11Y`, `MELEE_A11Y_STEER` and `MELEE_A11Y_CUES`, when set, override the file for the run in both directions and are never written; a later change through the model applies and saves. `MELEE_A11Y=1` forces accessibility on.
- **Online.** Nothing here changes game state; the gates on `pc_net_resim()` stay as they are.

## Done when

- Unit tests cover the hierarchy, a live change of each switch, the overrides never written, and the last announcement before silence.
- The drive scripts that set `MELEE_A11Y=0` with `MELEE_A11Y_STEER=1` behave as before. A drive run with steering off in the run's `a11y.ini` steers nothing on character select, and the D-pad reaches quick chat.
- The `verify` skill's quick, menus and online levels pass, and `python tools/check_style.py`.
- `docs/a11y/speech.md` and the `verify` skill describe the switches as settings with the environment as overrides.
