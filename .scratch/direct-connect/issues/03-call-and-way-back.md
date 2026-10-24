# 03 The call's speech and the way back

Status: resolved (2026-10-24)
Type: task
Blocked by: 02; 01 for the verification of pairing and the returns only

Speak the call and the returns as decided in `.scratch/direct-connect/spec.md`, sections "The call" and "The way back from a session".

- The call's opening, status changes with the clock left out of the comparison, connecting, starting and failures, all as shown.
- Reminders at 0:15 and 0:30, then every 30 seconds from 1:15, timed from the call's start. A reminder is the current line with its clock, queued and never interrupting. While a no-clock line shows, the fork counts the moments itself and repeats the line without a time.
- Returns: `lobby_arrival` already tells a back-out from a lost connection. Say "Back to DIRECT CONNECT." or "Connection lost. DIRECT CONNECT." with the hovered row. Back to the hub without a session (Start after a failed call) is "DIRECT CONNECT." with the hovered row, as from the keyboard.
- Gate on `pc_net_resim()`. Connecting and starting run with a session already up.
- Unit tests as listed in the spec's "Tests", for the call and the returns.
- The one-copy drive script calls a made-up code and waits through the 0:15 and 0:30 reminders and the 0:45 change, then stops with B. It needs internet access for the DHT. If issue 01 found the copies meet here, add a two-copy script for pairing and both returns to the `verify` skill's online level; otherwise those rest on unit tests and issue 04.
- Docs: the netplay primer gets the clock-free comparison and the reminder rhythm's reason; the `verify` skill gets the new scripts.

## Comments

### 2026-10-24: implemented

- The status line's reading is `src/pc/a11y/wait_status.cpp` (`WaitStatus`, `status_line`, `reminder_moment`), taking a line and the wait's clock, so Unranked's "Looking for an opponent  0:12" can reuse it. `DirectSpeech` opens the call with the line and its buttons (interrupting), and queues each change and each reminder. Lines are compared with the trailing "m:ss" left out; runs of spaces are collapsed as everywhere else.
- Reminders: the moments are read from the line's own clock where it shows one, so a reminder never says the second before its moment (the drawn clock is formatted a moment before the snapshot reads `pc_net_match_progress`), and from the call's clock behind a no-clock line, which keeps running there; the fork needs no clock of its own. Reminders run only while the phase is searching, so connecting, starting and a failure never repeat. A change on a moment's frame serves for it.
- The opening says buttons only while the call searches: the frame a hub row starts a call that fails at once ("DHT unavailable") still carries the hub's "A: select ...", and the keyboard's own-code failure shows none.
- Starting: the screen draws a countdown line, "Starting in 1...", above the status line (`mnOnlineLobby_Update`). The snapshot gains `direct.countdown`, formatted as drawn; the reader says it when it appears, after a status line change on the same frame.
- Returns: already in from issue 02; added the unit test for Start after a failure (call → hub, "DIRECT CONNECT." with the hovered row). Both returns checked in runs.
- Gated by the hook's `pc_net_resim()` check, as before; a test confirms a moment is said once.
- Tests: `direct_speech` gains the call's opening, the clock ignored, the rhythm through 2:17 (a skipped frame still reminds once), the line-clock rule, the no-clock repeats, connecting and starting (also on one frame), failures, the hub after a failure, and a fresh rhythm for each call.
- Scripts: `tools/a11y/direct_call.drive` (one copy, `#AAAAAAAA`: the opening, "Calling #AAA-AAA-AA 0:00", 0:15, 0:30, the 0:45 change, B to the Online screen), and `direct_back.drive` and `direct_lost.drive` (two copies: pairing with connecting's line and the countdown, then one return each), replacing issue 01's `direct_connect.drive`. All three are in `run_level.py online`; the two-copy ones share the forwarded ports, so they run one after the other, and `lan_lobby` moved to ports of its own (42150) to run beside them. With `--base-b` the two-copy ones are left out, since the base port build would write `contacts.txt` into the maintainer's settings folder; the `verify` skill says how to run them by hand.
- Verified: unit tests (`launcher_data` the known failure), style check, `run_level.py menus online` 30 of 30 in 177 s.

Findings:

- Connecting and starting show "Searching for an opponent..." on the internet path: `net_match.c` clears its reason once a hello is valid, and the lobby's fallback for no reason is that line. Said as shown, so a found call says "Searching for an opponent..." then "Starting in 1...". Connecting lasted a frame or two in every run; starting about two seconds. A base port fix ("Connecting to RYAN#K3X-Q2M-7A...") would help every player; left for the maintainer to decide. The play test (issue 04) should judge it by ear.
- During connecting and starting the screen lists both players' codes, which the spec leaves unsaid, so the player who waited never hears who called. A candidate for the play test.
- The waiting side has no change at 0:45, so its reminders go 0:15, 0:30, then 1:15: a 45-second gap the spec accepted.
- Once the two copies never found each other on the DHT in three minutes (only strangers' addresses discovered); an unchanged rerun met in about twelve seconds. Recorded in the `verify` skill as a flake to rerun.
- Back to the hub after a failure on a real call was not run: no script can make one here without the clipboard or typing your own code (which `dial` does not do). It rests on the unit test.
