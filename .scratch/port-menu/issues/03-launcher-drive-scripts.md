# 03 Launcher mode and launcher drive scripts

Status: ready-for-agent
Type: task
Blocked by: 02

The tooling's proof in `.scratch/port-menu/spec.md`: the launcher's speech, play-tested already (`.scratch/launcher-accessibility/`), becomes something a drive run checks. The scripts also guard issue 04's reader split.

## Machinery

- **A launcher mode for `drive.py`.** It starts the game without a disc argument, after writing into the run's fresh settings folder (issue 01) a `launcher.cfg` that names the disc, so the launcher opens on Home with the disc remembered (spec, open question 6). Speech is on in this mode only when the run asks for it, as elsewhere.
- **Never Choose disc.** The Windows file dialog takes focus. The mode or the scripts must make pressing it impossible by accident, for example a step that refuses that button. A comment names why.
- **Launcher scripts** in `tools/a11y/`, run by `run_level.py menus`:
  - the opening after the disc check
  - Home's buttons and status line
  - entering Settings and every tab
  - a setting changed and back
  - a slider moved
  - lines read with Up and Down on Controls and Online
  - typing in a text field, a refused character included
  - Back to Home
  - Play Melee, ending on the game's first scene

  Wait on each expected line. Write them from what the launcher says today, as `launcher_speech.cpp`, its test and the play-tested issues record it.

## Done when

- The launcher scripts pass, side by side with the rest of the menus level, which passes too.
- The maintainer's settings stay untouched by every run.
- The `verify` skill's menus level lists the launcher scripts and their expected lines. Its "Out of reach" no longer names the launcher. CLAUDE.md's "Verification" stops calling the launcher out of the drivers' reach.
