# 01 Two copies meet by Direct Connect on this machine

Status: resolved (2026-10-24)
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

### 2026-10-24: they do not meet here

`drive_net.py --direct` boots both copies into DIRECT CONNECT's hub, each on its own port (a 42050, b 42051) with a fresh settings folder; `b dial a` types a's code on b's code keyboard; `tools/a11y/direct_connect.drive` is the script. Run with two fork builds:

- a waited as `PLAYER#HJWCY4JK`, and b's log named `#HJWCY4JK` as the code it called, so the typing works.
- Both reached the DHT (bootstrap resolved within a few seconds, 125 good nodes by the end).
- Both found each other at the router's public address, a at port 42050 and b at 42051: the router keeps each socket's own port. Each sent MatchHello there, three packets at every rediscovery, for three minutes.
- Neither logged `match: recv valid MatchHello`. No session, no character select.
- DHT nodes also handed both copies strangers' addresses; their hellos went unanswered, as expected.

Every DHT answer reached both copies through the router and both sent to the right public address, so the firewall and the internet connection are not the cause: the router does not loop UDP for its own public address back in. The router at 192.168.0.1 serves a web interface in TP-Link's style; the maintainer will look for a NAT loopback setting there. The fork-against-base run and the returns wait on that, since nothing pairs before them.

If no setting turns it on, pairing and the returns rest on unit tests and issue 04, as the outcome says. Recorded in `docs/a11y/netplay.md` ("Two copies on one machine") and the `verify` skill, which keeps the run out of every level.

### 2026-10-24: they meet with a port forward

The router is a TP-Link Archer AX73. It has no NAT loopback setting (searched for loopback, reflection, hairpin, boost; NAT Forwarding has Port Forwarding, Port Triggering, UPnP and DMZ; the Internet page has only Enable NAT). It does loop back forwarded ports: with a rule "melee test" forwarding UDP 42050-42051 to this machine, which the maintainer keeps until Direct Connect support is done:

- Fork against fork: they met about 15 seconds after the call, reached character select with both openings spoken, and the back-out returned both to the hub (`lobby: direct connect hub, 1 recent`).
- The second call, from b's recent opponent row, met too, and the session ended within two frames: a's first frames carried B, the call screen took it as "stop", and a went to the Online screen. It repeated on a rerun, and with accessibility and steering off, so it is the base port's: `net.c` keeps the last raw controller sample (`s_raw_last`) across sessions, `session_reset` does not clear it, and the back-out held B until the session ended. Recorded in `docs/a11y/netplay.md`, "A session after a back-out".
- Fork against base port (`--base-b`), the base port copy waiting and the fork copy calling (`a dial b`): they met, both reached character select, and when a closed, b logged its peer left and returned to the hub. Passed. b's run wrote a `contacts.txt` into the maintainer's settings folder, deleted afterwards.

So pairing, the back-out return and the lost-connection return can all be checked by script here, the back-out followed by a second session only once the stale B is fixed. `direct_connect.drive` stays out of the verify levels until then.
