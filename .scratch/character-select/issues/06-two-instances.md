# 06 Two instances: steering under input delay, fork against base port

Status: ready-for-agent
Type: task
Blocked by: 04

The check that steering holds up online, from `.scratch/character-select/spec.md`, section "Tests". Base port online play is unverified on Windows, and the base port's own harness `tools/net_test.py` runs on Linux only, so this issue starts with finding out what works. Read `tools/net_test.py` for how the base port drives two instances, and the netplay parts of `docs/testing.md`.

Every run follows the rules for agent runs in CLAUDE.md: no focus taken, silent audio, `--no-card`, speech off.

## Step 1: smoke check

- Start two instances on this machine, connected over loopback: `MELEE_NET=127.0.0.1:<the other's port>`, `MELEE_NET_PORT`, `MELEE_NET_PLAYER` 0 and 1, the same `MELEE_NET_KEY`, each with its own `MELEE_LOG_FILE` and its own key pipe.
- Evidence of a session: a `net: rollback with` line in both logs, no `net: bind` failure, no `peer silent`.
- Find the route to character select. Either a direct session walks the ordinary menus in sync, or both instances go through VS. Mode, Online, LAN play, as `net_test.py --lan` does.
- If no session comes up on Windows, stop here. Record what failed under "Comments", set the status to `needs-info`, and say in CLAUDE.md that steering online rests on the unit tests of the glide until the base port's online play works on Windows.

## Step 2: a driver for two instances

- Extend `tools/a11y/drive.py`, or add a sibling that reuses it, to run two instances from one script: steps addressed to instance a or b, waits on either one's output.
- Both instances are in one session, so each simulates both controllers. A press on instance a shows in both logs.

## Step 3: runs at several delays

One script: instance a steps from its start position to a portrait three columns and one row away, chooses, and steps down to its player slot. Instance b chooses any character by stick, as `net_test.py` does, so the match can start. Then Start.

Run it with the input delay pinned at 0, 2, 6 and 10 frames (`MELEE_NET_DELAY`), and once with a slow link simulated (`MELEE_NET_SIM_DELAY_MS=100`) and the delay left automatic.

Each run passes when:

- instance a logs the expected announcement after each press, and each glide's arrival;
- instance b logs the same choice as the other player's ("Player 1:" and the character);
- neither logs `net: DESYNC`;
- the two recordings of controller data (`MELEE_NET_RECORD`) hold the same controller data for the frames both recorded;
- both reach the match.

## Step 4: fork against base port

- Build `upstream/master` in a second build directory. Use a git worktree, and leave the fork's own `build/` alone.
- The same script, instance b being the base port build. It has no speech log; the evidence is no `net: DESYNC` on either side and both reaching the match.
- The two builds pair only if their version strings match. If `upstream/master` has moved to a newer version than the fork's last base merge, say so and build the commit of that merge instead.

## Done when

- The smoke check's result is recorded, whichever way it went.
- If sessions work: the script is committed under `tools/a11y/`, all runs of steps 3 and 4 pass, and their results are recorded under "Comments" with the frames each glide took at each delay.
- CLAUDE.md, "Verification": how to run two instances, and that the run belongs after a change to steering and after a base merge. "Online compatibility": "untested so far" replaced by what was tested.
- The primer or `docs/a11y/` gains what was learned about running netplay on Windows.

## Comments
