# 06 Port menu drive scripts

Status: ready-for-agent
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
