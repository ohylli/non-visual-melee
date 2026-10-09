# 04 Steering on stage select

Status: ready-for-agent
Type: task
Blocked by: 01, 03

Spec: `.scratch/stage-select/spec.md`, "Steering" and "Steps".

## Machinery

- **Stage cursor hook** in `fn_8025A310` after the clamp: the cursor's position and the stick the game applied this frame. Offline the stick is merged, so it may be another port's. One line, declared in `a11y_hooks.h`. The glide advances here, once per simulated frame.
- **Targets** in a new stage select targets file on issue 03's shared step algorithm:
  - rows built from the tiles' positions, with undrawn tiles absent and locked tiles present;
  - Icicle Mountain and Flat Zone in rows 1 and 2, remembering which row the player came from;
  - the first press while nothing has been hovered goes to Icicle Mountain.
- **Where steering is off**: as the spec lists.

## Behaviour

- Every direction names the destination at the press; an edge repeats the current tile.
- Hovers crossed during a glide go unsaid and arrival is silent. A glide ending elsewhere names where it is, and a failed one says "Could not reach Corneria".
- The player's stick abandons the glide.
- A pick landing on a different stage from the last one spoken: "Picked Fountain of Dreams."
- Under the Turns or Loser rule with another player's cursor: no steering.

## Done when

- Unit tests cover:
  - steps from every tile in every direction, with all stages unlocked and with a fresh save's locks;
  - the tall tiles from both rows;
  - the first press in each direction;
  - steps from empty space;
  - the mid-glide pick;
  - no steering for another port.
- A drive script under `tools/a11y/` steps into the grid, walks rows and columns waiting on each name, steps to Random and picks it, and checks the glide lines. A second picks with A during a glide and waits on "Picked ...".
- A run with steering on, left on stage select with no input, shows the stick at rest.
- The `verify` skill lists the scripts; `python tools/check_style.py` passes; the footprint grep shows the hook.
- Docs:
  - ADR-0004's consequence about stage select's formula is updated;
  - `docs/a11y/native-menus.md` records the movement rule;
  - the CLAUDE.md status line mentions steering.

## Comments
