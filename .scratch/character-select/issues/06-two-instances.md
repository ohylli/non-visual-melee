# 06 Two instances: steering under input delay, fork against base port

Status: resolved (2026-10-01)
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

### 2026-09-30: step 1, smoke check passed

A direct session comes up on Windows over loopback and walks the ordinary menus in sync, so no LAN lobby is needed to reach character select.

How it ran: two copies of `tools/a11y/drive.py` started side by side from Git Bash, each with its own `--out` (so its own `MELEE_LOG_FILE`) and its own key pipe (drive.py names the pipe after its own process id). drive.py passes its environment through, so the net settings went on the command line:

```
MELEE_NET_KEY=a11ytest MELEE_SEED=7 MELEE_NET=127.0.0.1:42051 MELEE_NET_PORT=42050 MELEE_NET_PLAYER=0 MELEE_NET_RECORD=a.rec python tools/a11y/drive.py --out a.log --timeout 60 a.drive &
MELEE_NET_KEY=a11ytest MELEE_SEED=7 MELEE_NET=127.0.0.1:42050 MELEE_NET_PORT=42051 MELEE_NET_PLAYER=1 MELEE_NET_RECORD=b.rec python tools/a11y/drive.py --out b.log --timeout 60 b.drive &
```

Instance a ran the steps of `tools/a11y/main_menu.drive`, then Down, A, A into VS. Mode, Melee, character select, after waiting for `net: rollback with`. Instance b only waited for its own "Character select." line; it was pressed nothing.

Evidence:

- Both logs have `net: rollback with` (P1 and P2, delay auto 2) and `net: handshake done seed=7 start_frame=120`; the handshake completes at the title screen, with the default `--scene title`.
- No `bind` failure, no `peer silent`, no `DESYNC` in either log.
- Both logs pass the same scenes at the same frames: title (0) to unlock notices (39) at frame 121, to the main menu (1) at 878, to character select (8) at 1000. Each scene change waits on a hand-off with the peer (`scene N hand-off waiting`).
- Both `[a11y]` transcripts match line for line up to character select, where a says "Character select. Player 1, no character." and b "Character select. Player 2, no character.": each follows its own port's hand, as intended.
- The two recordings (`MELEE_NET_RECORD`) are byte-identical, 1182 frames.
- Frame 600 stats: rollbacks 0, loss 0 %, ping 0 ms; about 60 frames a second on both.
- Harmless: each side drops a few early datagrams for session 00000000 before the guest learns the host's session id.

Route to character select for steps 2 and 3: the direct session from boot, instance a driving the menus exactly as the offline scripts do. Instance b's presses land on port 2, so b can choose its own character by stick without a lobby.

### 2026-09-30: steps 2 and 3, the driver and runs at five delays, all passed

The driver is `tools/a11y/drive_net.py`; `drive.py`'s run became a `Game` class it reuses, one per instance, each with its own key pipe. Script steps begin with `a` or `b`; waits look at that instance's output only. After the script it checks both logs for a session and for `net: DESYNC`, and compares the two recordings over the frames both recorded in the session: the side that leaves first records its next frame offline, its peer's port unplugged (`err` 0xff), so the comparison stops at the first `net: disconnected at frame` of either log. The script is `tools/a11y/css_net.drive`.

What the script does: a walks the menus into VS. Mode, Melee (b's menu speech follows along); a steps Up to its button, Up to Pichu, Right three times to Mewtwo, Up to Kirby, waiting for each glide's arrival, and chooses; b hears "Player 1: Kirby"; a steps Down to Mewtwo and on into the player slots to its own ("Player 1: human"). b pushes the stick up 450 ms, hears "Holding your coin" and the portraits the coin enters, and chooses (Ness in every run); a hears "Player 2: Ness. Ready to fight. Press Start." Then Start on a, and on stage select Start on both.

Found on the way:

- Online stage select needs a pick from each player (`netStageSel_*` in `mnstagesel.c`); Start on one side alone leaves the match waiting. Noted in the primer.
- Start pressed within milliseconds of Ready to Fight appearing is ignored; the script waits a second, as the offline scripts do.

Results, every run passing (no DESYNC, recordings equal, both in the match, rollbacks 0):

| Run | Delay | Frames recorded in agreement | Player 1: closed | Pichu | Each one-portrait step | Player 1: human |
|---|---|---|---|---|---|---|
| `--delay 0` | 0 | 1684 | 17 | 11 | 7 | 14 |
| `--delay 2` | 2 | 1722 | 19 | 13 | 9 (Player 3: closed 10) | 16 |
| `--delay 6` | 6 | 1725 | 23 | 17 | 13 | 20 |
| `--delay 10` | 10 | 1823 | 27 | 21 | 17 | 24 |
| `--sim-delay-ms 100` | auto: 2, then 8 at frame 240, 4 at 1620 (ping 203 ms) | 1635 | 25 | 19 | 15 | 22 |

Each glide took its delay-0 frames plus the input delay and arrived. The one-portrait steps are Pikachu, Jigglypuff, Mewtwo, Kirby and Mewtwo again. Glides to "Player 3: closed" and "Player 2: closed" are passed through by the next press of the search, so not every run logs their arrival. The slow-link run's glide frames match delay 8, the delay in force during character select.

Step 4 (fork against base port) remains.

### 2026-10-01: step 4, fork against base port, all passed

`upstream/master` was e833835, the commit of the fork's last base merge, so the version strings match (v0.2.2-beta). It was built in a worktree outside the repo (`../melee-pc-base`) with its own `build/`.

The base port does not build natively under MSYS2: abseil's time zone code fails on undeclared `WindowsCreateStringReference` and friends. The fork's own build fix from 64e1385 is missing upstream: the Windows SDK level pin for native MinGW builds, and `zlib1.dll` copied beside the executable (its `file_cache.cpp` part is upstream already). Those two `CMakeLists.txt` blocks went into the worktree uncommitted; neither touches what the game simulates.

`drive_net.py --base-b <build dir>` runs instance b from that build. The base port has no speech log, so `css_net.drive` gained steps addressed to `b-fork` (b's speech waits, run only against a fork build) and `b-base` (waits on `boot scene` lines for character select, scene 8, and stage select, scene 9, and a short sleep for the coin; run only against a base port build). Steps addressed to plain `b` run in both. A fork-against-fork run at delay 0 afterwards still passed with the same frames as before.

Results, every run passing: no `DESYNC` and no `peer silent` on either side, recordings equal, both in the match, rollbacks 0, b's log without a single `[a11y]` line. Player 2 chose Ness in every run, and player 1 heard it.

| Run | Delay | Frames recorded in agreement | Player 1: closed | Pichu | Each one-portrait step | Player 1: human |
|---|---|---|---|---|---|---|
| `--delay 0` | 0 | 1689 | 17 | 11 | 7 | 14 |
| `--delay 2` | 2 | 1719 | 19 | 13 | 9 | 16 |
| `--delay 6` | 6 | 1720 | 23 | 17 | 13 | 20 |
| `--delay 10` | 10 | 1807 | 27 | 21 | 17 | 24 |
| `--sim-delay-ms 100` | auto: 2, then 8 at frame 240, 4 at 1620 (ping 203 ms) | 1630 | 25 | 19 | 15 | 22 |

Each glide took the same frames as against a fork build. What was learned about running netplay on Windows is in `docs/a11y/netplay.md`; CLAUDE.md says to repeat these five runs with `--base-b` after a base merge.
