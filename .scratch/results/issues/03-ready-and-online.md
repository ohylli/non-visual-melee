# 03 Ready, online and quick chat

Status: resolved (2026-10-27)
Type: task
Blocked by: 02

Spec: `.scratch/results/spec.md`, "Ready" and "Quick chat". Primer: `docs/a11y/netplay.md`.

- **Your ready.** Your Start says "Ready, waiting for P2." (every human not yet ready), or "Ready, waiting for your opponent." online. A second Start says "Not ready.". With one human nothing is said: the scene leaves.
- **Another human's ready.** It is said as "P2 ready." or "Your opponent is ready.", queued after current speech. Their un-ready is said as "P2 not ready." or "Your opponent is not ready." (proposed in writing).
- **While you are ready**, the reading cursor keeps working (proposed in writing).
- **Quick chat.** On results, `pc_a11y_chat_buttons` masks the D-pad while the reader is active, as it does on character select.
- **Leaving.** Settle open question 8: drop a queued announcement when the scene leaves, and check a disconnect mid-results.

## Done when

- Unit tests cover ready, un-ready, waiting with one, two and three others, another human's ready and un-ready, and the online forms.
- `tools/a11y/drive_net.py` runs fork against fork and against the base port build (`--base-b`):
  - both instances reach results, open the stats and step with the D-pad, and no quick chat phrase is sent;
  - one readies and hears "Ready, waiting for your opponent.", the other hears "Your opponent is ready.";
  - the first un-readies and readies again, and both then ready, and the session returns to character select.

  Recordings agree. Add a script for this or extend an existing one.
- Build, unit tests and style pass; the online level of the `verify` skill passes.

## Comments

### 2026-10-27, implementation (agent)

Implemented; ready is said as the spec has it, online in its own words, and quick chat gives the D-pad up on results while speech is on.

- **Where things live.** `game_access.c`: each port's `unplugged` (its pad reports an error) beside the ready flag; the snapshot's `online` is set by the tick hook. `results_speech.cpp`: `ready_changes`, comparing every panel's ready flag tick to tick from the panels' opening (the baseline is taken in `stats_opened`, so a new visit starts afresh), and `takes_dpad()`. `results_names.cpp`: the ready words and `port_list` ("P2, P3 and P4"). `hooks.cpp`: `screen_steers` became `screen_takes_dpad`, with a results case asking the reader. No new line in a base port file.
- **The wording**, as the driven run said it: a: `"Ready, waiting for your opponent."`, `"Not ready."`; b: `"Your opponent is ready."` and `"Your opponent is not ready."`, queued. Offline: `"Ready, waiting for P2 and P3."`, `"P2 ready."`, `"P2 not ready."` (unit tests only: offline a second human has no controller).
- **Who is said.** Humans only, and only those with a controller: the game readies a human whose pad reports an error on the tick the panels take input, and again whenever it is unplugged; without the `unplugged` check that would be said as "P2 ready." in every offline match with a second human. Online both pads read plugged in (`from_wire`). The "waiting for" list is every other human not yet ready.
- **A ready that ends results says nothing**, yours or another's: the scene reaches state 4 on that tick and leaves after a short delay. This extends the spec's one-human rule to the last of several, where "P2 ready." would be cut off by character select's opening a moment later.
- **Open question 8, leaving.** The fork cannot withdraw an announcement already handed to the screen reader, only cut it off with an interrupting one; every screen after results opens with an interrupting announcement (character select's opening, a scene's name, the lobby's return), and the reader queues nothing as the scene leaves. A lost connection, checked once by a LAN run (`lan_lobby.drive`'s steps to character select, `results_net.drive`'s to results, a's ready, `a quit`): b leaves results within a tick of `net: peer left` and says `"Connection lost. LAN play. Searching for players. B to go back."`. A session started at boot (the drivers' `MELEE_NET`) has no lobby: the copy stays on results offline, the lost player's port reads as unplugged, the game readies it, and the reader says nothing.
- **Quick chat.** `pc_a11y_chat_buttons` masks the D-pad on results while speech is on (`Speech::enabled`, as "features that take keys for speech" do), not on the Steering switch: the reading cursor speaks or does nothing. Checked by screenshots of both windows after their D-pad presses in a copy of `results_net.drive`: no chat line with `--muted`; without it (speech off) b showed "You: Need a moment". The chat hint line ("CHAT: D-PAD twice ...") stays on screen either way.
- **Tests.** `results_speech`: your ready with one, two and three others waiting, a human already ready left out, un-ready and ready again, one human (the scene leaving) and an unplugged second human said nothing of, another human's ready and un-ready queued and after yours on one tick, the last ready silent, the online forms with you on P2, the reading cursor kept through ready and un-ready and a new visit's fresh baseline, and `takes_dpad` with speech on and off.
- **The online run.** `tools/a11y/results_net.drive` (new), in `run_level.py online` as `results_net`, run `--muted`: player 1 sets one stock from the rules header (online the rules header works as offline), Pichu walks off Princess Peach's Castle, both hear the opening from their own side, a's D-pad opens both panels, each steps, a readies, un-readies and readies, b readies, both return to character select. Recordings agree. Against the base port build (`--base-b ../melee-pc-base/build`, at `upstream/master` 0fb0af3) b pages with A instead and is never pressed on the D-pad.
- **`results.drive`** passes unchanged; `run_level.py` now fails it on any ready line, the one human's ready being silent.
- **Verification**, online level with `MELEE_A11Y_CUES=1`: build with the "Rollback sections" line, style, unit tests 32 of 32 (`launcher_data` passed this time), a bounded `title` run, `run_level.py menus online` 31 of 31, and `run_level.py results_net --base-b ../melee-pc-base/build` passing.
- **Docs.** `native-menus.md`, "Results in detail": "Ready" and "Online and leaving" (open question 8). `netplay.md`: "Quick chat shares the D-pad" (was "Quick chat and steering share the D-pad"). The `verify` skill: the online level's runs, `results_speech`'s cases, `results.drive`'s silent ready, `results_net.drive` and the lost connection, and the chat screenshot check on results.
- **Left open.** The un-ready by another human offline is reachable only with a second controller, so it is unit-tested alone. CLAUDE.md's LAN play line still says quick chat leaves the D-pad to steering on character select only; issue 04 writes the status lines. The wording is for the play test.
