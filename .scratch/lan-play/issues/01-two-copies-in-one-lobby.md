# 01 Two copies in one LAN lobby on Windows

Status: ready-for-agent
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
