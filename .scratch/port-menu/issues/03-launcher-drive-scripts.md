# 03 Launcher mode and launcher drive scripts

Status: resolved (2026-10-19)
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

## Comments

### 2026-10-19, implementation (agent)

Implemented: two launcher drive scripts in the menus level, run in a new launcher mode of `drive.py`.

- **The launcher mode.** `drive.py --launcher` starts the game with `--no-card` and no disc, after writing into the run's fresh settings folder (between `fresh_settings_dir` and `Popen`) a `launcher.cfg` of two lines, `disc "<path>"` and `check_updates 0` (`write_launcher_cfg`). The game rewrites the file in full at start.
- **Open question 6, what `launcher.cfg` needs: the `disc` line alone**, its path quoted as `std::quoted` writes it (backslashes doubled). `check_updates 0` keeps the update check off: with it on, a fresh folder's run says "Update available: v0.3.1-beta" and the banner today, which follows the network and the base port's latest release. **Waiting out the disc check: nothing extra.** The reader holds its opening until the status line settles and focus reaches a control, which it does on Play once the check passes, so the first wait on the opening (`"Melee launcher. Ready to play, Disc not verified. Play Melee, button. Disc: melee.iso, ..."`) is all a script needs.
- **Never Choose disc.** `drive.py` refuses a `key` or `gamepad` step that would press the focused control (`Return`, `Space`, `Keypad Enter`, `a`, `start`) while the last focus announcement in the log names Choose disc, and in launcher mode any such press made before an announcement has said where focus is (`check_press`, `REFUSED_FOCUS`, a comment naming why). The same refusal covers the other controls that leave the window: Home's Discord and Settings' Join Discord (the browser), the update banner's buttons (the browser; Update Now on other platforms), and Display mode (fullscreen). Tried: Return and gamepad a on Choose disc, Return before the opening, and by mistake Return on Display mode and on Discord while writing the scripts; each was refused. Quit is left pressable: it only ends the run.
- **A deviation: accessibility on, screen reader muted.** The line reading on a Settings tab is gated on the accessibility switch (`LauncherReader::step` checks `Speech::enabled()`), so with `MELEE_A11Y=0`, as every driven run had it, Down on Controls went straight from the tab to Back and Online's lines were skipped. A new fork switch, `MELEE_A11Y_SCREEN_READER=0` (`Config::screen_reader`, `speech.cpp`, unit test `speech`), keeps accessibility on but never starts Prism and logs each announcement marked `(muted)`; `drive.py --muted` sets it, and `run_level.py` runs both launcher scripts with `--launcher --muted` (its new `ARGS` table). Under it the cue engine opens too (it follows the switch), on the dummy audio driver. Speech in launcher mode stays off unless the run asks, as elsewhere.
- **The scripts.** `launcher_home.drive`: the opening, Down over every button and round, Up round, Verify disc started and canceled (the button's "Cancel verification" and "Verify disc", then the status line's "Verification canceled."), and Play Melee ending on "Title screen. Press Start." (a long wait: Play waits for shaders still compiling and then starts by itself). `launcher_settings.drive`: Settings' opening, Right through every tab and round, Vertical sync off and on, the Internal resolution slider a step and back, Controls' lines down to Back and back up over two, Player name typed into (`#ab1` at the field's start: "# not allowed", "APLAYER", "ABPLAYER", "Full, 8 characters at most", then Backspace twice back to "PLAYER"), Online's lines in their turn (the connect code is the run's own, matched as `PLAYER#\w+`), Shift+Tab and Tab, and Home again by Back to launcher and by Escape. Each step waits on its line, written from what the launcher said in exploratory runs, which matched `test_launcher_speech.cpp` throughout.
- **Checked.** Build, `python tools/check_style.py`, unit tests (`launcher_data` failing as known; `speech` with the new case), a bounded `title` run with `MELEE_A11Y_SCREEN_READER=0` (`screen reader off`, the two announcements `(muted)`, clean exit), and the menus level: 19 of 19 passed side by side in 71 s. Every file under `%APPDATA%\melee-pc` kept its modification time through every driven run (the exploratory runs, the refusals and the menus level); each run wrote its own `build/drive/<name>.log.settings/launcher.cfg`. The one file that moved was `dawn_cache.db`, a shader cache, at the exit of the hand-started bounded run, whose command in the `verify` skill sets `MELEE_USER_DIR` but not `MELEE_CACHE_DIR`.
- **Docs.** `docs/a11y/rmlui-screens.md` "How a drive run opens the launcher", `docs/a11y/speech.md` (the new switch), CLAUDE.md ("Driving the game", "Verification", the speech status line), the `verify` skill (the menus level's nineteen scripts with the launcher's expected lines, the launcher mode and its refusals, `--muted`, the muted bounded run, "Out of reach" now naming the port menu until it speaks, raw controller input, the refused launcher controls and speech by ear), `drive.py --help`, `run_level.py`'s docstring.
- **Findings for the maintainer.** Escape in one of Online's text fields does nothing: the field keeps it, so Home is reached from there only after moving focus off the field, though the footer says "ESC / B Back" (base port behaviour; a driven run showed it, an ear has not). While Up and Down read the lines next to a text field, focus stays in the field, so Left and Right move its cursor and say a character ("L", "R").
- **For issue 04.** Both launcher scripts guard the reader split: run `run_level.py launcher_home launcher_settings`. A port menu script that checks the reader's line reading needs `--muted` for the same reason. The refusal reads focus from the log alone, so once the port menu speaks it applies there too; `port-menu.rml` has a Display mode row, and a reader that names it as the launcher's does ("Display mode, Windowed, Enter to change") will have it refused, which is right, since fullscreen would take the screen there as well.
