# 04 Play test: Direct Connect with a friend

Status: ready-for-human
Type: task
Blocked by: 02, 03

The maintainer plays a friend over the internet by Direct Connect, fork build against fork build or base port build of the same release. First, both run `melee-netcheck.exe` to see whether their networks suit Direct Connect; the friend already has it.

Check by ear:

- The hub's opening, your code as shown, and whether "YOU", the "#" and the grouped characters are clear. Then copying it with Y and sending it to the friend.
- The friend's code copied from a chat while the hub is open: the clipboard row is said and the cursor is on it.
- Typing a code on the code keyboard: the layout sentence, the keys, "K, 3 of 8", deleting, the error.
- A call and a wait: the clock's lines, the reminders at 0:15 and 0:30 and every 30 seconds after 0:45. Too many, too few, or the clock read oddly?
- A failure, if one happens, as shown.
- Connecting, starting, character select online.
- Backing out of character select, "Back to DIRECT CONNECT.", and a lost connection, "Connection lost. DIRECT CONNECT."
- The recent opponent's row on the next visit, "played 5m ago" read as shown.
- Whether any line read as shown fails by ear and should be reworded (CLAUDE.md, "The isolation rule").

Record findings under "Comments" and as new issues.

## Comments

### 2026-10-24: left by the review of issues 02 and 03

For the maintainer to decide, most severe first:

- A reminder can follow a change by about a second. A change counts for a reminder only on the same frame, so "Joining the online network... 0:14" turning into "Calling X 0:14" is followed by "Calling X 0:15" a second later, and "Found them but couldn't connect. Still trying..." at 0:29 is said again at 0:30. When it happens depends on the network. Options: let a change cover any moment within a few seconds after it, or leave it to the play test. Recommended: listen for it here, then add the cover if it is heard (`WaitStatus::frame`, `src/pc/a11y/wait_status.cpp`).
- A failure that comes during a call says its line alone, while the screen also shows "B: back". The call's opening now says the buttons as drawn ("that is your own code. B: back"), but a change into a failure is said as the line, as the spec says. Options: add the buttons to a change into the failed phase, or keep the line alone. Recommended: judge by ear whether the player knows how to get out after "invalid connect code".
- The waiting player never hears who called: connecting and starting show "Searching for an opponent..." (the base port's line for a call with no reason) while the screen lists both players' codes. Options: add the opponent's code to the countdown announcement in fork code, or a base port fix ("Connecting to RYAN#K3X-Q2M-7A...") in its own commit that helps every player. Recommended: the base port fix, if the play test confirms it.
- Y on the hub says nothing after the keyboard's error: B back from the keyboard leaves "A code has 8 characters after the #" under the list (the base port does not clear it), and the error takes the line before "Your code is copied - send it to a friend", so the copy changes nothing on screen. Options: a one-line base port fix clearing `direct_error` as the keyboard is left (`directEntryFrame`, `src/melee/gm/gmonlinemode.c`), in its own commit, unmarked as it helps every player; or leave it. Recommended: the base port fix.
- `WaitStatus` is meant for Unranked's search too, but its rhythm leaves out 0:45 for Direct Connect's "isn't answering yet" change, which Unranked does not have. Recommended: pass the moments in when Unranked's feature reuses it, not before.
- `CLAUDE.local.md`'s base port worktree note says to rerun `run_level.py online --base-b` after a base merge; the two-copy Direct Connect runs it leaves out are now a step of the `verify` skill's base merge level, by hand with the settings folder's cleanup. Recommended: add them to the note (a private file, left to you).
