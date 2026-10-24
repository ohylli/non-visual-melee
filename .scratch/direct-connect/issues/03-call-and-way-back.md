# 03 The call's speech and the way back

Status: ready-for-agent
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
