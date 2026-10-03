# 02 Lobby speech

Status: ready-for-agent
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
