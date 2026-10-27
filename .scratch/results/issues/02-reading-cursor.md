# 02 The stats panels and the reading cursor

Status: resolved (2026-10-27)
Type: task
Blocked by: 01

Spec: `.scratch/results/spec.md`, "The stats panels and the reading cursor".

## Machinery

- **The snapshot grows.**
  - Each port's page, ready flag and the number of pages.
  - Every row of each page, as data: page 0's opponents with KOs and falls (name tags where the game uses them) and SDs; the 23 stats with their values; each bonus's kind and points.
  - Leave out the hidden first row, if it exists (open question 6).
- **Labels.** The stat labels, units and bonus names are decoded from SdRst. Stat labels get a space before each inner capital (open questions 1 and 2).
- **Controller 1's presses.** The reader sees the newly pressed D-pad directions and Z (open question 3).
- **The reading cursor**: the fork's own state, starting on your stats.
  - A page change resets it to the page's first row.
  - D-pad up and down step through the rows, repeating the row at an edge.
  - D-pad left and right switch players, keeping the row number clamped and saying the player then only what differs (the spec's table). At an edge the edge player is said again.
  - Z says the player's summary line.
- **Speech.**
  - The stats opening, by your press or another's, says your page's name and first row, with the keys on the first results of the run.
  - Your page changes are said; the game's silent ends stay silent.
  - Others' paging and all scrolling stay silent.
  - Bonus mode opens on Bonuses.

## Done when

- Unit tests cover every row form, page changes and their silent ends, stepping and its edges, each "what differs" case of the spec's table, the player edges, Z, the stats opened by another player, the keys once a run, and an unknown label.
- A drive script reaches the stats and checks a page change each way, a step down and up, a player switch, and Z. It can extend `results.drive` or be a new script beside it.
- Build, unit tests and style pass.
- The "Results in detail" section has the pages, the labels and the answers to open questions 2, 3, 6 and 7.

## Comments

### 2026-10-27, implementation (agent)

Implemented; the stats panels and the reading cursor speak as the spec has them, with the deviations below.

- **Where things live.** `game_access.c`: the snapshot grows by the page count, the KOs and falls page's headings (SdRst 8, 9, 12), the 23 stat labels (each label row's string and its value row's), the local controller's newly pressed D-pad directions and Z, and per port the page, the ready flag, the opponents (port, KOs dealt, falls taken, name tag), the SDs row, the 23 stat values and the bonuses (kind, name, points). The stat values come from the game's own value functions (`fn_8017AE0C` to `fn_8017BB30`, which only read the `MatchEnd`) through a fork table of the Stats page's rows; the bonuses are counted again from `x44C`, never through `fn_80174284` or `lbl_8046E190`. `game_text.cpp`: `decode_shift_jis` for name tags. `results_speech.cpp`: `ResultsRow`, the rows of each page, the panels' opening, page changes, the cursor's steps, switches and Z. `results_names.cpp`: page names, `stat_label`, the stats keys. No new hook.
- **The wording**, as a driven run said it: `"KOs and falls. KOs. A and B change page. D-pad up and down read the rows, left and right switch player. Z the player's summary. Start when ready."`, `"P2 Mario, 0"`, `"Falls"`, `"Mario, P2, P1 Link, you, 0"`, `"Mario, P2"` at the right edge, `"Mario, P2, 1st, 0 KOs, 0 falls, 0 SDs, not KO'd."`, `"Stats. KOs, 0."`, `"Damage given, 17"`, `"Link, P1, you, 17"`, `"Bonuses. Smash-less, 1500."`, `"Run, Don't Walk, 6500"`. A screenshot agreed with the values said.
- **Open question 2, the values.** The screen draws every value as a bare `"%d"`: no "%", no distance unit, times in whole seconds (Ground time 19 after a 20-second match). The fork says the number alone ("Damage given, 87", not the spec's "87%"), and "none" for "---", which no stat has shown.
- **Open question 3, the presses.** `HSD_PadCopyStatus[local port].trigger` after the tick, the pad the panels read: port 0 offline, `pc_net_local_player()`'s online (the synced input). Online the D-pad reaches that pad whatever quick chat's mask does, since chat reads the physical controller on its own; the panels ignore the D-pad and Z, and in state 2 any press opens both machines' panels, which is right.
- **Open question 6, the hidden first row.** Real: every list's entry 0 duplicates entry 1 (on Bonuses a lone "−" value row) and is drawn one row above the clip window; the list's last entry is never drawn. The snapshot builds rows from what they mean, so neither reaches the cursor.
- **Open question 7, the first press.** It only opens: state 2 sets state 3 and nothing else, and the panels take input only after their slide. A first A opened page 0 in a run. The reader reads no row on the tick the panels open, whatever the press.
- **Labels.** Every label of the 23 reads right with the rule: a space before a capital after a lower-case letter, two-row labels joined, and each word after the first lower-cased unless all capitals, as the spec's "Damage given" ("Attempted grabs", "Transformation time", "KOs"). Bonus names are said as written.
- **Choices beyond the spec.** An empty Bonuses page (no bonus earned) says "No bonuses" as its row ("Bonuses. No bonuses.", "Fox, P3, No bonuses"). At either end of the players, the player's name alone. The page change says the cursor player's first row, since the cursor keeps its player. Z's line has the team after the place in a team match, and no place on a cancelled match. The reader follows controller 1's port's panel even when it holds a CPU (controller 1 pages a CPU's panel when plugged into its port), or the first player's when that port is empty. A name tag is copied with a terminator, since the game's eight bytes can lack one. An unknown stat label is said "Stat 4", a missing bonus name "Bonus 40" and a missing heading in the fork's word, each logged once. If the opening and the panels' opening fell on one tick, the second would be queued; that cannot happen in practice.
- **Tests.** `results_speech` gains the panels: the opening by your press (which reads no row) and by another's, the keys once a run, the page changes and their silent ends, Bonus mode opening on Bonuses, every row of KOs and falls with its edges, a name tag and "you" on an opponent's list, the 23 stats in order with "none", the bonus rows and an empty page, each case of the spec's "what differs" table, the player order and edges with you, online you and nobody you, Z, the cursor while ready, and unknown labels. `game_text` gains the Shift-JIS decoder.
- **Verification**, menus level: build with the "Rollback sections" line, style, unit tests (`launcher_data` failed as known on one run and passed on another), a bounded `title` run, `run_level.py menus` 23 of 23 with `results.drive` extended: a D-pad press opening the panels, a step down and up, a switch to Mario, Z, A and B each way. Probe runs read all 23 of Mario's stats and his bonuses.
- **Docs.** `native-menus.md`, "Results in detail": "The stats panels" (pages, values, the hidden rows, bonus rows, paging, the first press), "Labels and words", "The reading cursor's input", and name tags now decoded. The `verify` skill's lines for `results.drive` and `results_speech`.
- **Left open.** PAL numbering of SdRst stays unchecked. No tag has met a run: the Shift-JIS decoder is tested by hand-made bytes. The wording is for the play test.
