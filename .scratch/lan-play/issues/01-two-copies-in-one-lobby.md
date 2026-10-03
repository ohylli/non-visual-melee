# 01 Two copies in one LAN lobby on Windows

Status: resolved (2026-10-03)
Type: research

Find out whether two copies of the game on this machine find each other in the LAN lobby and reach character select, so the lobby can be verified by script. Spec: `.scratch/lan-play/spec.md`.

Every run follows the rules for agent runs in CLAUDE.md: no focus taken, silent audio, `--no-card`, speech off.

- Read how `tools/net_test.py --lan` runs two copies on one Linux machine (ports, environment, presses) and the mDNS notes at the top of `src/pc/net_lan.c`. The lobby skips loopback interfaces, so discovery may need a real network interface to be up.
- Each copy needs its own game port and log file. Find the environment that sets the LAN game port.
- Drive both through VS. Mode, Online, LAN play with `tools/a11y/drive_net.py`, or a LAN option added to it, without `MELEE_NET`. Evidence: `lan:` lines in both logs showing the other machine, then "players found", then character select in both.
- Windows Firewall may prompt the first time; a prompt would steal focus. If one appears or seems likely, stop and ask the maintainer to allow UDP 5353 and 41000 once.
- `MELEE_LAN_DIRECT=ip:port` skips discovery and can reach the connecting and failure states even if discovery fails.

## Outcome

Record what works under "Comments", in `docs/a11y/netplay.md`, and, if a LAN run becomes part of verification, in the `verify` skill. If discovery cannot work on one machine, say so in the netplay primer; issue 02 then rests on unit tests and issue 05.

## Comments

### 2026-10-03: discovery works on one machine

Two copies on this machine find each other in the LAN lobby and go on into character select, fork with fork and fork with the base port build (`../melee-pc-base`, `e833835`). The lobby is verified by script: `tools/a11y/drive_net.py --lan tools/a11y/lan_lobby.drive`, now in the `verify` skill's online and base merge levels. The primer's new section "Two copies in the LAN lobby" (`docs/a11y/netplay.md`) holds what stays true.

What it took:

- `--lan` drops `MELEE_NET`, `MELEE_NET_PLAYER` and `MELEE_NET_KEY`; each copy keeps its own `MELEE_NET_PORT` (42050, 42051), which is all the LAN game port needs. `MELEE_CACHE_DIR` stays shared, as for direct sessions: the lobby's machine id already mixes in the port, and a fresh cache dir costs a minute and a half of shader building.
- Discovery goes over the Ethernet interface (192.168.0.217); multicast loops back to the other copy. Each lists the other as `DESKTOP-FUG0K6B` as soon as both are announcing.
- Windows Firewall already had allow rules (UDP and TCP in, Private and Public) for both `build/melee.exe` paths, so no prompt appeared.
- `MELEE_LAN_DIRECT` was not needed and stays untried.
- The base port's recording runs from boot into the LAN session without a new header, so `--lan` skips the recording comparison, as `tools/net_test.py --lan` does.

Evidence from the passing runs, per copy:

```
a lan: found DESKTOP-FUG0K6B 192.168.0.217:42051
a lobby: 2 players found - press START
a lan: host election: we host as P1, guest DESKTOP-FUG0K6B 192.168.0.217:42051
a lobby: entering CSS at frame 119, seed 843266908
a [a11y] speak interrupt (off): "Character select. Player 1, no character."
b lan: found DESKTOP-FUG0K6B 192.168.0.217:42050
b lobby: 2 players found - press START
b lan: host election: DESKTOP-FUG0K6B 192.168.0.217:42050 hosts, joining as P2
b lobby: entering CSS at frame 119, seed 843266908
b [a11y] speak interrupt (off): "Character select. Player 2, no character."
```

For issue 02:

- The status line's count includes this machine: one other copy reads "2 players found - press START".
- Both copies share the hostname here, so rows matched by name collide on one machine. Two real machines have different names; the unit tests should cover two rows of one name anyway.
- When a closed during character select, b logged `net: peer left (reason 1)`, `net: disconnected at frame 262 (status 1)`, then `lan: stopped`, a new `lan: announcing` and the lobby's "LAN: searching...". That is the "Connection lost." return; a script reaches it once `drive_net.py` can end one instance alone (today `quit` ends both).
