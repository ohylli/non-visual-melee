# LAN play: the minimum to play online

Status: agreed 2026-10-03 (design conversation with the maintainer)

## Goal

A blind player can play LAN matches against a sighted friend, both on fork builds, or alone on two PCs: from the main menu through the lobby, character select and stage select into a match, and back again, by speech. Minimum viable: the screens on the way get just enough speech to get through them.

Vocabulary is the glossary in `CONTEXT.md`; this spec added **Lobby** and **Quick chat** to it.

## Scope

In:

- **The lobby, LAN layout**: its opening, the machines it finds, its status changes and failures.
- **Quick chat muted on character select** while steering is on, because both use the D-pad.
- **Stage select online**: one more clause in its scene announcement.
- **Results**: its scene announcement says how to continue, offline and online.

Out, noted for later:

- A switch between quick chat and steering on character select, so a player can choose which the D-pad does.
- Speaking received quick chat phrases.
- The lobby's other layouts: direct connect, ranked and unranked. They keep "Online lobby. No speech yet."
- Choosing which machine to play; the base port pairs machines on its own.
- A launcher setting that turns accessibility off. A sighted player who wants silence starts the game with `MELEE_A11Y=0`; the maintainer tells their friend how.
- Speech for the stage select and results screens themselves.

## What the game gives us

Already spoken: VS. Mode, its Online entry and the Online screen with LAN play (the main menu tree, using the base port's own words), and online character select with steering.

### The lobby

- Reached by VS. Mode, Online, LAN play. It is the scene `GS_ONLINE_LOBBY`, the first state of the online game mode, flow in `src/melee/gm/gmonlinemode.c` (`gm_Scene_OnlineLobby_OnFrame`, `lobbyFillView`) and drawing in `src/melee/mn/mnonlinelobby.c`. Game text on a panel, all strings literals in C.
- Each frame the scene fills an `OnlineLobbyView` (`gmonlinemode.h`): title, up to 8 player rows (row 0 is this machine, "YOU"), a status line, a hint line. LAN uses only the players layout. `docs/a11y/native-menus.md` names the view as the read point.
- Machines find each other by mDNS (`src/pc/net_lan.c`). Each is listed by its hostname, not the launcher's player name. A machine whose protocol, version string or game image differ is listed "(other version)" and is never paired. The version string is the one the fork keeps, so fork and base port builds of a release pair.
- No host or join choice and no address to type. Start on either machine marks it ready; the machines elect a host (player 1), and a machine still idle in the lobby follows a proposal naming it. So the machine that did not press Start is pulled in too.
- Status line by state: "LAN: searching...", "This network hides other players - try DIRECT CONNECT", "<N> players found - press START", "Ready - waiting for host...", "Connecting...", "Starting..." with a countdown, "Failed: <reason>[ - <peer word>][ - START: retry]".
- B leaves for the Online screen at any time except while starting.
- Both players return to a fresh, searching lobby when anyone backs out of character select, and when the connection drops anywhere, a match included (`gmscene.c` ends the scene; every exit of the online mode routes to the lobby). The base port shows no reason in either case.

### Quick chat

`src/pc/net_chat.c`, polled from `src/pc/vi.c` with controller 1's physical buttons. It runs while a session is active on character select, results and the lobby. A D-pad press picks a group and a second press within 3 seconds sends a phrase to the other player, at most one each 2 seconds. On character select that is the D-pad steering also uses, so steering sends stray phrases. Chat is presentation only: what it reads or skips does not touch the simulation.

### Stage select online

Each player picks on their own machine; Start with nothing hovered picks random. The picks are exchanged and a coin flip on the shared seed chooses one. Nothing happens until both have picked.

### Results

Left by Start on both machines, back to character select. `docs/netplay-verification.md` records that the results screen exits on Start only in some cases and otherwise on an internal counter, and that its own harness could not leave results; find out what really continues before promising it in speech.

## Decisions

### Lobby speech

The fork reads the lobby once a frame (one hook in the lobby's frame function, handing over the view) and speaks what changed, polling rather than one hook per transition because the view already gathers everything the screen shows (ADR-0003's reasoning). Only the LAN layout speaks; the scene announcement leaves the lobby to it, as it does for character select.

- Opening: "LAN play. Searching for players. B to go back." When machines are already listed, the opening says them instead of searching.
- Returning from a session: "Back to LAN play." after backing out of character select, "Connection lost." when the peer was lost, each followed by the usual opening. The lobby itself does not show which happened, so the fork decides it where the session ends.
- A machine appearing: "Found <name>." The first one adds "Press Start to play." A machine leaving: "<name> left." A machine of another version: "Found <name>, other version, cannot play."
- Status changes: "Waiting for host.", "Connecting.", "Starting.", and the network-hides-players line in the fork's words.
- A failure: "Failed: <the base port's reason, as written>. Start to retry." The reasons stay exact, rare and useful in bug reports.
- Unsaid: ping, the host badge, the countdown.
- Both machines speak the same, whichever pressed Start.

Wording is a first proposal for the play test to judge.

### Quick chat on character select

While steering is on, quick chat ignores the D-pad on character select: one hook in `vi.c` that filters the buttons handed to `pc_net_chat_poll`. Chat keeps working in the lobby and on results. Steering is on whenever accessibility is (`MELEE_A11Y=0` turns both off unless `MELEE_A11Y_STEER=1`), so a friend playing without speech keeps full chat. Online compatibility is unaffected: chat never reaches the simulation, and the peer simply receives fewer phrases.

### Stage select and results announcements

- Stage select online: "Stage select. No speech yet. Press Start for a random stage, then wait for your opponent." Offline stays as it is.
- Results, offline and online: "Results. No speech yet. Press Start to continue.", worded to match what a drive run shows really continues.

## Tests

- Unit tests for the lobby reader: opening, finding and losing machines, each status, failure with reason, the two returns, the silent re-run rule.
- Whether two copies on one Windows machine find each other in the lobby decides how the lobby is verified (issue 01). If they do, `tools/a11y/drive_net.py` gains a LAN route and the `verify` skill's online level covers the lobby; if not, the lobby rests on unit tests and the maintainer's play test, as the launcher does.
- A drive run each for the stage select and results announcements, and one showing steering on online character select sends no quick chat (the chat line stays empty in both instances' logs or screenshots).

## Docs

- `docs/a11y/netplay.md`: the lobby route, what pairs machines, and whatever issue 01 learns about one-machine discovery on Windows.
- CLAUDE.md: one accessibility status line for LAN play.
- The `verify` skill: any new drive script or run.

## Issues

- 01 Two copies in one LAN lobby on Windows
- 02 Lobby speech
- 03 Quick chat muted on character select
- 04 Stage select and results announcements
- 05 Play test: LAN play on two machines
