# 02 DIRECT CONNECT hub and code keyboard speech

Status: resolved (2026-10-24)
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

### 2026-10-24: implemented

- The hook: `pc_a11y_lobby_frame(&view)` after `mnOnlineLobby_Update` in the internet branch of `gm_Scene_OnlineLobby_OnFrame`, so it also sees Unranked, Ranked and Profile, which the snapshot leaves unread.
- The snapshot: `A11yLobbyState` gains `direct_connect` and an `A11yDirectState` (screen, title, subtitle, rows, cursor, keys, key cursor, hint, and `call_ms` from `pc_net_match_progress` on the call); phase and message are the LAN fields, shared. `a11y_game_direct_connect()` mirrors `internetLobby()`, so `MELEE_LAN_DIRECT` stays unread and keeps "Online lobby. No speech yet.". The phase tells the screens apart where the view's screen does not: the hub and keyboard always show the found phase and a call never does. The frame a hub row starts a call keeps the hub's screen and rows with the call's phase (read as the call), and the frame B leaves an empty keyboard shows a zeroed screen (skipped).
- The reader: `src/pc/a11y/direct_speech.cpp`, its own class beside `LobbySpeech`, fed from the same hook; `hooks.cpp` hands the arrival to it as the scene announcement steps aside. Runs of spaces are collapsed in every line, the hints' included (a screen reader says them the same); announcements join their parts with full stops and end without one. A move says the hovered row; the rows changing, or the hovered row's text changing apart from a recent opponent's "played 5m ago", says it too. The line under the list is said when it changes, queued after a move on the same frame. On the keyboard a character typed wins over the move and the cleared error of its frame.
- The returns' openings ("Back to DIRECT CONNECT." / "Connection lost. DIRECT CONNECT." with the hovered row) and back to the hub from any other screen ("DIRECT CONNECT." with the hovered row) were cheap to put in with the opening, so they are in and unit-tested; issue 03 verifies them in a run.
- Tests: `direct_speech` (unit), `tools/a11y/direct_hub.drive` in the menus level, with `run_level.py` failing it on "Online lobby. No speech yet.". Verified: menus and online levels, 27 of 27; style check; a hand run of "Wait for a friend to call me" and B, the call silent here.

Findings:

- B on the hub did nothing: the base port masked B whenever the hub or keyboard drew the frame, though the hub's hint says "B: back". Fixed in its own commit, unmarked as it helps every player: the mask now skips the hub's screen. Not fixed upstream on any branch as of today.
- After B leaves the keyboard with its error showing, the hub's line under the list still shows "A code has 8 characters after the #" (the base port does not clear it). The fork says only the title and the hovered row on that return, so the stale line goes unsaid; left as is.
- Start pressed again with the error already showing changes nothing in the view, so nothing is said. The game plays its back sound.

For issue 03: the call goes in `DirectSpeech::opening` and `screen_changed`, whose `A11Y_DIRECT_CALL` cases are empty, and in `frame`, which has no per-screen change branch for the call yet. Keep the status line's reading (splitting the clock off, the reminder moments) in a file of its own taking a line and `call_ms`, so Unranked's "Looking for an opponent  0:12" can reuse it.

