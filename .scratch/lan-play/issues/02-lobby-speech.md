# 02 Lobby speech

Status: resolved (2026-10-03)
Type: task
Blocked by: 01 (for its verification route only)

Speak the LAN lobby as decided in `.scratch/lan-play/spec.md`, section "Lobby speech".

- One hook in the lobby's frame function (`gm_Scene_OnlineLobby_OnFrame`, `src/melee/gm/gmonlinemode.c`) hands the fork the `OnlineLobbyView` once a frame; a fork reader compares it with the last frame and speaks what changed. Only the LAN players layout speaks; other layouts keep the scene announcement "Online lobby. No speech yet.", and the scene announcement stays silent for the LAN lobby.
- Gate on `pc_net_resim()` like all speech.
- The two returns ("Back to LAN play.", "Connection lost.") need the reason the session ended, which the lobby does not show. Find where it is still known: the online mode's exit routing in `gmonlinemode.c`, the peer-lost exit in `gmscene.c`, or `pc_net_peer_status()` before `onEnterLobby` disconnects. Prefer reading it in the fork at lobby entry over a new hook.
- Machine names come from the view's rows; match rows across frames by name, since rows can reorder.
- Unit tests as listed in the spec's "Tests".
- Verify with the `verify` skill at the online level if issue 01 found a script route, otherwise at the quick level plus the unit tests.
- Add the CLAUDE.md accessibility status line for LAN play and the lobby section of `docs/a11y/netplay.md`.

## Comments

### 2026-10-03: implemented

`src/pc/a11y/lobby_speech.cpp`, fed by `pc_a11y_lobby_frame` after the LAN path of `gm_Scene_OnlineLobby_OnFrame` draws the view; `game_access.c` copies the view into a fork snapshot, LAN play's player list only. The scene announcement is skipped for the LAN lobby. Unit tests in `test_lobby_speech.cpp`; `tools/a11y/lan_lobby.drive` now waits on the opening, the other machine found, "Starting.", "Back to LAN play." after holding B on character select, and "Connection lost." on b after `a quit` (new in `drive_net.py`: quit addressed to one instance closes it alone).

What the code found:

- The reason a session ended is gone by lobby entry: `onEnterLobby` disconnects, and `pc_net_disconnect` marks every ended session `PC_NET_PEER_LEFT`, a clean back-out included, while the copy that disconnects second may receive the other's goodbye first. So `hooks.cpp` keeps the last scene entered and character select's pending exit on its last frame, and `lobby_arrival` decides: character select with exit 2 (B held) is "Back to LAN play.", any other session scene "Connection lost.". No new hook. The pending exit is recorded even on re-run frames, since a back-out by the remote player can first show on one.
- The phase leaves two pairs apart: "Ready - waiting for host..." and "Connecting..." share the connecting phase, searching and the hidden-network line share searching. The fork tells them by the start of the base port's status line.
- The host passes briefly through "Waiting for host." before "Connecting." (it is ready before the election finishes), so the player who presses Start hears both. Left as is for the play test to judge.
- Wording beyond the spec: the last machine leaving adds "Searching for players."; a failure says "Start to retry." only when the base port offers it (another machine listed); "Connection lost." is followed by "LAN play." and the opening, "Back to LAN play." by the opening without the title.


### 2026-10-03: a crash seen during verification, not yet followed up

On the first verification pass, `css_setup.drive` and `css_classic.drive` (offline, unrelated to the lobby) both panicked a few seconds into the match, exiting with 0xC0000409:

```
lbVector_WorldToScreen: bad pos3d=00000000806f6564 x=-428443616.000 y=17.405 z=0.000 (raw x=cdcc4c3f y=418b3ce6 z=00000000)
PANIC src/melee/lb/lbvector.c:394: assertion "pos3d->x>-50000.0F&&pos3d->x<50000.0F" failed
```

Both passed on an unchanged rerun. None of this change's code runs during a match, so it looks like an intermittent base port bug, not listed among the `verify` skill's known failures. Worth its own issue if it shows up again.
