# 02 Pushed SDL events for the drivers

Status: ready-for-agent
Type: task
Blocked by: 01

The second piece of the tooling in `.scratch/port-menu/spec.md` ("The tooling"). The launcher and the port menu react to SDL events, which `MELEE_KEY_FIFO` never sends. This issue gives drive scripts a way to send them.

## Machinery

- **A fork-owned event pipe.** A switch, `MELEE_A11Y_EVENT_FIFO` (name to settle), names a pipe that a fork thread in `src/pc/a11y/` reads.
  - The thread pushes SDL events with `SDL_PushEvent`: key down and up, typed text, and gamepad button down and up.
  - It starts only when the switch is set, adds no hook to a base port file, and never takes window focus.
  - It logs each push so a drive step can see it was taken (spec, open question 3).
- **Facts first.** Answer the spec's open questions 1 to 3 before building the steps:
  - whether pushed events reach RmlUi and the two pages' loops without window focus
  - which gamepad event fields are read
  - how a step confirms a press while the game stands still
- **`drive.py` steps.** A key, typed text, and a gamepad button, each through the new pipe beside the existing key pipe. Their names and syntax go in `drive.py`'s `--help`. `drive_net.py` passes them through to an instance as it does the other steps.

## Done when

- A drive run opens the port menu with F1 from the main menu, moves through it, and closes it with Escape. The same with a gamepad: Back to open, D-pad, B to close. Each is shown by a screenshot and by the base port's own log lines, since the port menu does not speak yet.
- A run that changes a port menu value leaves the maintainer's settings untouched (issue 01).
- `python tools/check_style.py` passes, and the `verify` skill's quick level passes.
- The `verify` skill lists the new steps where it covers drive scripts. CLAUDE.md's "Driving the game" names the switch and what it reaches.
- `docs/a11y/rmlui-screens.md` gets a short section on how the drivers reach RmlUi pages: pushed events, and why the key pipe cannot.
