# 01 Two copies meet by Direct Connect on this machine

Status: ready-for-agent
Type: research

Find out whether two copies of the game on this machine can meet by Direct Connect and reach character select. That decides whether pairing, the call's connecting and starting lines, and the two returns can be verified by script. Spec: `.scratch/direct-connect/spec.md`.

Every run follows CLAUDE.md's rules for agent runs: no focus taken, silent audio, `--no-card`, speech off (or `--muted` once speech is under test).

## Why it might work

Both copies join the public BitTorrent DHT from one public address, each on its own port. `net_match.c` drops candidates on private addresses, so each copy reaches the other only through the router's public address. The router then has to send those packets back inside: NAT loopback, also called hairpinning or NAT reflection. Many home routers do this by default; some have a setting for it, and some can't. The maintainer agreed to try it here.

## What to do

- Give `tools/a11y/drive_net.py` a Direct Connect route, like `--lan`: no `MELEE_NET`, `MELEE_NET_PLAYER` or `MELEE_NET_KEY`, and each copy on its own `MELEE_NET_PORT`. Windows Firewall already allows `build/melee.exe` in, and the base port worktree's build too, so no prompt is expected. Any other build directory would prompt and take focus.
- Each driven run gets a fresh settings folder, so each copy has a fresh identity and connect code. a waits ("Wait for a friend to call me") and logs `lobby: direct connect waiting as 'NAME#SUFFIX'`. b reads that code from a's log and types its eight characters on the code keyboard. That needs a step that turns a code into D-pad presses on the 4×8 grid, which wraps at every edge. Never press Y and never use the clipboard row: both use the maintainer's real clipboard.
- Evidence of success: `match:` lines in both logs, `lobby: entering CSS` in both, and character select's opening.
- If they meet, also try a fork build against the base port build (`--base-b ../melee-pc-base/build`), a calling b, and a back-out and a lost connection (`a quit`) for the returns.
- If they don't meet, record what the logs show (DHT reached, MatchHello sent to which address, no answer), so the cause can be told from a router without loopback.

## Outcome

Record it under "Comments" and in `docs/a11y/netplay.md`, and, if a two-copy run joins verification, in the `verify` skill. If the copies cannot meet here, pairing and the returns rest on unit tests and issue 04.

## Comments
