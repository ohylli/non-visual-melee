# Direct Connect: playing a friend over the internet

Status: agreed 2026-10-24 (design conversation with the maintainer)

## Goal

A blind player can play a friend over the internet by Direct Connect: from the Online screen through DIRECT CONNECT's hub, a call by connect code or waiting for the friend's call, into character select, and back to the hub when the session ends, all by speech. Character select, stage select and the match already speak online.

Vocabulary is the glossary in `CONTEXT.md`. This spec added **Connect code**, **Direct Connect hub**, **Recent opponent**, **Code keyboard** and **Call** to it.

## Scope

In:

- **The Direct Connect hub**: its opening, moving over its rows, a code appearing on or leaving the clipboard, and the line under the list.
- **The code keyboard**: its opening, moving over the keys, typing, deleting and the error.
- **The call**: its status lines, reminders on a rhythm while it waits, failures, connecting and starting.
- **The way back**: returning to the hub after a session, after a failed call and from the code keyboard.

Out, noted for later:

- Unranked, Ranked and Profile. They keep "Online lobby. No speech yet." Unranked's search shares the call's status lines, so the call's reader should be built so a later feature can reuse it.
- `MELEE_LAN_DIRECT`, a developer fixture that routes Direct Connect through the LAN lobby's path. It stays silent.
- Bringing LAN play's existing rewordings in line with the speak-as-shown rule (CLAUDE.md, "The isolation rule"). That waits until the maintainer wants it.
- Typing a code on the PC keyboard. The clipboard row covers fast entry, and turning typed keys into grid moves would write input outside the pad hook's one sanctioned use.

## What the game gives us

The flow is `gm_Scene_OnlineLobby_OnFrame` in `src/melee/gm/gmonlinemode.c` (`directEntryFrame` for the hub and the keyboard), on `src/pc/net_match.c` for codes, recent opponents, the clipboard and the call. The scene is `GS_ONLINE_LOBBY`, the same one LAN play uses. Each frame it fills an `OnlineLobbyView` (`gmonlinemode.h`) and draws it. The internet lobbies return before the LAN hook (`pc_a11y_lobby_frame`) is called, so nothing past the scene announcement speaks today.

### The hub

- The view: `screen` is `LOBBY_SCREEN_MENU`, `title` is "DIRECT CONNECT", `subtitle` is "YOU NAME#K3X-Q2M-7A" (your connect code, its name taken from the launcher's Player name), plus `menu` rows, `cursor`, the `message` line under the list and the `hint` line.
- The rows, top to bottom:
  - "Call RYAN#K3X-Q2M-7A  from clipboard", only while a valid code other than your own is on the clipboard.
  - One row per recent opponent: the code padded with spaces, then "played 5m ago". A code is saved only after a call with it reached the session, so a typo is never remembered.
  - "Enter a friend's code".
  - "Wait for a friend to call me".
- The clipboard is read again every 30 frames. When the row count changes, the cursor jumps to the top row without a press.
- The message line is "Pick who to play. Both of you can call each other." After Y it becomes "Your code is copied - send it to a friend", and it shows an error when there is one.
- A or Start chooses the hovered row, Y copies your code to the system clipboard, and B goes back to the Online screen.

### The code keyboard

- The view: `screen` is `LOBBY_SCREEN_KEYS`, `keys` is the 32 characters "A" to "Z" then "2" to "7" in 4 rows of 8, and `key_cursor` is the hovered key. `menu[0]` is the entry line, "Friend's code   # K3X-Q2_-__". The message is "Type the 8 characters after the #" or the error "A code has 8 characters after the #". The hint is "A: type   B: delete   START: call".
- The D-pad moves over the grid, which wraps at every edge. A types the hovered key while fewer than 8 are typed. B deletes the last character, or goes back to the hub when nothing is typed. Start with 8 typed calls "#K3XQ2M7A" (no name: only the suffix identifies a player), and with fewer it shows the error.

### The call

- The view: `screen` is `LOBBY_SCREEN_PLAYERS` (the zeroed default), with a phase and the `message` status line. While the call searches with no failure, the hint is "Y: copy my code   B: stop" and the subtitle is your code.
- Status lines while searching, each with the call's clock "m:ss" counted from the call's start (`pc_net_match_progress`):
  - "Joining the online network...  0:05", until this side reaches the DHT.
  - "Calling RYAN#K3X-Q2M-7A  0:12". From 45 seconds it is "RYAN#K3X-Q2M-7A isn't answering yet. Still calling  0:47".
  - "Waiting for a friend to call you  0:12", when you chose to wait.
- Lines without a clock, set by `net_match.c`:
  - "Found them but couldn't connect. Still trying...", which keeps searching.
  - "Can't reach the online network - check your internet", after 45 seconds without the DHT. It clears itself if the network comes back.
- Then the connecting and starting phases, and character select.
- A failure, phase error: `net_match.c`'s own lowercase lines ("that is your own code", "invalid connect code"), or a peer status as "Peer left - START: search". Start goes back to the hub.
- B stops the call and leaves for the Online screen.

### After a session

Every way out of a Direct Connect session lands in a fresh lobby, whose entry opens the hub again (`gm_Scene_OnlineLobby_OnEnter`, `directEntryBegin`). The LAN lobby's reasoning holds here too: the lobby no longer knows why the session ended, and `lobby_arrival` already tells a back-out from a lost connection by the scene the session left.

## Decisions

### Speak what the screen shows

Every line is said as the screen shows it: the base port's words, names, abbreviations and number formats. That covers the title "DIRECT CONNECT", "YOU RYAN#K3X-Q2M-7A", "played 5m ago", the clock "0:15", the hints "A: select   Y: copy my code   B: back", and the lowercase failure lines. Runs of padding spaces count as layout, not words, and are collapsed. Only what the screen gives visually alone is put in the fork's words: the grid's layout, the hovered key, the count typed. The play test may overrule any of it.

### The hook

One more `pc_a11y_lobby_frame(&view)` in the internet branch of `gm_Scene_OnlineLobby_OnFrame`, beside its `mnOnlineLobby_Update`, so the fork reads the same view the screen draws. `game_access.c`'s snapshot grows to carry Direct Connect's fields when the online kind is `ONLINE_KIND_DIRECT`. Unranked, Ranked and Profile stay unread. The scene announcement steps aside for Direct Connect as it does for LAN play, and the lobby speaks its own opening on its first frame. Speech is gated on `pc_net_resim()` like all speech.

### The hub

- **Opening, from the Online screen:** the title, your code, the hovered row, the message line and the hint, as one announcement: "DIRECT CONNECT. YOU RYAN#K3X-Q2M-7A. Enter a friend's code. Pick who to play. Both of you can call each other. A: select   Y: copy my code   B: back". The cursor starts on the top row, here "Enter a friend's code" since nothing is on the clipboard and nobody has been played yet.
- **A move:** the new hovered row only, with no position ("2 of 4").
- **The rows changing without a press** (a code copied to or gone from the clipboard while the hub is open): the hovered row the cursor jumped to, which for a new code is "Call RYAN#K3X-Q2M-7A from clipboard".
- **The message line changing** ("Your code is copied - send it to a friend", an error): the new line.
- **Back to the hub without a session** (B on an empty code keyboard, Start after a failed call): the title and the hovered row, "DIRECT CONNECT. Enter a friend's code."

### The code keyboard

- **Opening:** the message line, the layout in the fork's words, the hint and the hovered key: "Type the 8 characters after the #. A to Z, then 2 to 7, 8 to a row, edges wrap. A: type   B: delete   START: call. A".
- **A move:** the hovered key alone ("K").
- **A typed character:** the character and the count, "K, 3 of 8".
- **A delete:** "Deleted K".
- **The error:** the message line as shown.

### The call

- **Opening:** the status line with its clock and the hint, as shown: "Joining the online network...  0:00. Y: copy my code   B: stop".
- **A status change:** the new line. The clock alone ticking is no change, so lines are compared with the clock left out. That covers joining → calling, calling → "isn't answering yet" at 0:45, the no-clock lines, connecting, starting and failures.
- **Reminders**, so a long wait never sounds like a hang: at 0:15 and 0:30, then (after the base port's own change at 0:45) every 30 seconds, 1:15, 1:45 and on, timed from the call's start. A reminder is the screen's current line with its clock as shown, "Calling RYAN#K3X-Q2M-7A  0:30". It applies to all three clock lines. While a no-clock line shows ("Found them but couldn't connect. Still trying..."), the same moments repeat that line without a time; the fork counts them itself since the view then carries no clock. Reminders queue behind other speech and never interrupt.
- **Connecting and starting:** their lines. Character select's own opening then takes over.
- **A failure:** the line as shown, "Peer left - START: search".

### The way back from a session

As LAN play does, decided by `lobby_arrival` at the lobby's entry:

- After a back-out: "Back to DIRECT CONNECT." and the hovered row.
- After a lost connection: "Connection lost. DIRECT CONNECT." and the hovered row.
- Your code and the hint are left out: the player has heard them on the way in.

## Tests

- Unit tests for the reader, from snapshots:
  - The hub's opening, moves, the rows changing without a press, and the message line.
  - The keyboard's opening, moves, typing, deleting and the error.
  - The call's opening, status changes ignoring the clock, and the reminder moments for both clock and no-clock lines.
  - Failures, the two returns, back to the hub without a session, and the silent re-run rule.
- A drive script for one copy: the Online screen → DIRECT CONNECT → the hub's opening → the code keyboard, typing and deleting → a call to a made-up code → the 0:15 and 0:30 reminders and the 0:45 change → B to stop. It needs internet access for the DHT, and no step presses Y: Y writes the maintainer's real clipboard.
- Whether two copies on this machine can meet by Direct Connect decides how pairing and the returns are verified (issue 01).

## Docs

- `docs/a11y/netplay.md`: rename "How the LAN lobby is read" to cover Direct Connect too. Add the internet branch's hook, the comparison that leaves out the clock, and what issue 01 learns about meeting on one machine.
- CLAUDE.md: one accessibility status line for Direct Connect. LAN play's line says other online lobbies have no speech yet; narrow it to Unranked, Ranked and Profile.
- The `verify` skill: the new drive script and any two-copy Direct Connect run.

## Issues

- 01 Two copies meet by Direct Connect on this machine
- 02 DIRECT CONNECT hub and code keyboard speech
- 03 The call's speech and the way back
- 04 Play test: Direct Connect with a friend
