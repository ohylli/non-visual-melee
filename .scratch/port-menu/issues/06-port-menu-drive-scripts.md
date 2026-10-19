# 06 Port menu drive scripts

Status: resolved (2026-10-19)
Type: task
Blocked by: 04, 05

The checks of `.scratch/port-menu/spec.md`, "Tests", "Drive scripts".

## Machinery

- **A port menu script** in `tools/a11y/`, from the main menu. It:
  - opens the menu with F1 and hears the opening
  - walks every tab, with the first control of each
  - changes a setting and back
  - moves a slider
  - reads the lines on Controls and Online
  - types in a text field, a refused character included
  - checks the update status after Check now, if it can do so without depending on the network (otherwise leave it out and say why)
  - closes with Escape and hears "Back to game"
  - opens again with a gamepad Back and closes with B

  Wait on each expected line. "Saved." must never appear as speech.
- **A mid-match script**, or a step in an existing match script. It opens the menu during a match with cues on, hears the opening, closes it, and shows the cues stop and start again (issue 05).
- Both run in the `verify` skill's menus level, or the match scripts' set for the second, through `run_level.py`.

## Done when

- Both scripts pass, side by side with their levels, which pass too.
- The `verify` skill lists them with their expected lines. Its "Out of reach" names only what the drivers still cannot do: raw gamepad input for remapping, and anything judged by ear. CLAUDE.md's "Verification" says the same.
- CLAUDE.md gets an "Accessibility status" line for the port menu, naming this issue directory and that the play test (issue 07) still waits.

## Comments

### 2026-10-19, implementation (agent)

Implemented: two drive scripts, the port menu's in the menus level and the mid-match one beside the match scripts, each with a `run_level.py` check for the line too many that a script cannot fail on.

- **`port_menu.drive`** (menus level, `drive.py --muted` for the line reading): from the main menu, `key F1` and the opening on Graphics; Vertical sync off and on; every tab with its first control; Master volume a step down and back; Controls' keyboard lines read backwards with Up from Return to game and on to Restore defaults, pressed for its status line ("Defaults restored.", spoken queued once settled), then forwards again; Player name typed into with `#ab1` ("# not allowed", "APLAYER", "ABPLAYER", "Full, 8 characters at most", Backspace back to "PLAYER"); Online's lines in their turn to Return to game; Escape to "Back to game" and a game press heard after it; `gamepad back` opening it on Online, the tab used last; `leftshoulder`, `dpdown`; `gamepad b` to "Back to game" and a game press again. `run_level.py` fails it on any `"Saved` line (`ABSENT`), so neither "Saved." nor "Saved automatically." may be spoken.
- **The update check is left out.** Check now asks the network (GitHub's latest release), and the base port writes its result only at the next saved change or opening; a script would depend on the network and the latest release, as the launcher's did before `check_updates 0`. Restore defaults stands in as the status line news a script can check without the network.
- **`match_port_menu.drive`** (beside the match scripts, `run_level.py match_port_menu`, which sets the debug match switches, `MELEE_A11Y_CUES=1` and `--muted`): at GO both position cues start; `key F1` logs your, the opponent's and the ledge warning's stops, then the opening; `key Escape` logs "Back to game", then both starts with the same targets. Link dashes off the left edge (`cue gap start`), `key F1` stops every voice the gap warning's among them before the opening, and `gamepad b` logs "Back to game" and the gap warning's start again. The waits check the order (stops before the opening, starts after "Back to game"); a new `BETWEEN` check in `run_level.py` fails the run on any cue start or stop logged between a port menu opening and the "Back to game" after it. Checked by feeding the check a passing transcript and one with a cue start inserted after each opening (failed with both lines named).
- **Checked.** Build; `python tools/check_style.py`; unit tests (27 of 28, `launcher_data` failing as known); with `MELEE_A11Y_CUES=1`, the menus level (20 scripts) and the seven match scripts side by side: 27 of 27 passed in 109 s.
- **Docs.** The `verify` skill (the quick level's match list, the menus level's twenty scripts, both scripts with their expected lines and checks, the event steps' paragraph, the cue paragraph's pointer, "Out of reach" now raw controller input, the refused controls and anything judged by ear), CLAUDE.md ("Verification", a status line for the port menu naming this directory and that the play test waits, and the match cues line saying they stand silent while the menu is open), `run_level.py`'s docstring.
- **For the maintainer and the play test (issue 07).**
  - With no controller in port 1, every binding row on Controls reads "-, Unbound, Enter to change" (the row's name is `-` until a controller is there), and the Controller row says "No controller in this port." Whether a connected controller's rows read better is for the play test and then the remapping feature.
  - Restore defaults with no controller in the port still says "Defaults restored.".
