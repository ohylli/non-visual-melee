# 02 DIRECT CONNECT hub and code keyboard speech

Status: ready-for-agent
Type: task

Speak the Direct Connect hub and the code keyboard as decided in `.scratch/direct-connect/spec.md`, sections "Speak what the screen shows", "The hook", "The hub" and "The code keyboard".

- The hook: one `pc_a11y_lobby_frame(&view)` in the internet branch of `gm_Scene_OnlineLobby_OnFrame` (`src/melee/gm/gmonlinemode.c`), beside its `mnOnlineLobby_Update`. Add it to the footprint the CLAUDE.md grep finds; the call itself is the marker.
- `a11y_game_lobby_state` (`game_access.c`) gains Direct Connect's fields when the online kind is `ONLINE_KIND_DIRECT`: screen, title, subtitle, rows, cursor, keys and key cursor, message, hint, phase. The LAN snapshot and Unranked, Ranked and Profile stay as they are.
- The reader: in `lobby_speech.cpp` or a file of its own beside it. Either way, keep the call's status reading (issue 03) where Unranked could reuse it later.
- The scene announcement steps aside for Direct Connect, as `hooks.cpp` already does for LAN play. Unranked, Ranked and Profile keep "Online lobby. No speech yet."
- Say lines as shown, padding spaces collapsed. Only the grid's layout, the hovered key and the count typed are in the fork's words.
- Unit tests as listed in the spec's "Tests", for the hub and the keyboard.
- A drive script from the main menu to the hub and keyboard (opening, moves, typing, deleting, the error), then back with B. No step presses Y. Verify with the `verify` skill at the menus level, and add the script to the skill.
- Docs: the netplay primer's lobby section covers Direct Connect's hook and snapshot. CLAUDE.md gets a Direct Connect status line, and LAN play's line names only Unranked, Ranked and Profile as unspoken.

## Comments
