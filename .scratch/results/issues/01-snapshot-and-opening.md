# 01 The results snapshot and the opening

Status: resolved (2026-10-27)
Type: task

Spec: `.scratch/results/spec.md`. Read it first, then `docs/a11y/native-menus.md` (game text) and `.scratch/training-menu/issues/01-reader.md`, the nearest earlier reader polled from the tick hook.

## Machinery

- **The snapshot.** `game_access.c` reads `lbl_8046DBE8` and its `MatchEnd`. The snapshot holds:
  - the state and sub-phase;
  - the match type, teams, the cancelled flag and the winners;
  - per port: slot type (human, CPU, empty), character (Zelda and Sheik as ended), team, place, KOs, falls, SDs, KO time, score, coins and bonus total.

  Never call `fn_80174284`. Recompute the static values rather than reading them.
- **"You".** You are controller 1's port offline and `pc_net_local_player()` online. Nobody is "you" when that port is a CPU or empty.
- **The reader.** It is fed from `pc_a11y_match_tick` in the results scene, behind `game_hook_may_speak()`. The scene hook starts a results visit. The reader must know whether this is the first results of the game run, for the keys.
- **The match type's words.** An accessor reads results' string table (SdRst on font 0) by number with its own symbol check, beside `menu_string`. It covers the match types for now; issue 02 adds the labels. Extend `MELEE_A11Y_TEXT_DUMP` to results and record what the indexes hold (spec, open question 1).
- **Scene words.** The results entries in `scene_speech.cpp`, offline and online, become silent.

## Done when

- The opening is said at state 2, as the spec's "The opening" has it, for every match type, teams, No Contest, ties, Bonus mode, and with and without "you". The keys are said on the first results of a run only.
- Unit tests (`test_results_speech`, new) cover those cases.
- `tools/a11y/results.drive` waits on the new opening instead of the old scene words, and passes.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
- `docs/a11y/native-menus.md` has a "Results in detail" section started with the state, flow and text findings, including the answers to open questions 1, 4 and 5 where they belong.

## Comments

### 2026-10-27, implementation (agent)

Implemented; results says its opening once the announcer has named the winner, and nothing on entry.

- **Where things live.** `game_access.c`: `a11y_game_results`, the snapshot (`A11yResults`: state, sub-phase, match kind, teams, cancelled, the banner's player and team, the local port, the match type's game text, and per port the slot, character, team, place, rank, KOs, falls, SDs, whether KO'd with the time, score and coins), each cell recomputed as the drawing code does with its clamps; `a11y_game_results_string`, SdRst by number with its own symbol check beside `menu_string`. `results_speech.hpp/.cpp`: the reader, `ResultsSpeech` (`visit_started`, `tick`), and the pieces issue 02's Z line can reuse: `results_you`, `results_order`, `results_player_name` ("Mario, P2", "Link, P1, you") and `results_summary` ("0 KOs, 1 fall, 0 SDs, KO'd at 1:16"). `results_names.hpp/.cpp`: the fork's words (places, teams, counts, the keys). `hooks.cpp`: the tick hook feeds the reader in `GS_RESULTS` behind `game_hook_may_speak()` and runs `menu_text_dump_once()` there; the scene hook starts a visit. No new hook in a base port file. `spoken_text` moved from `menu_speech.cpp` to `game_text.cpp` for both readers.
- **Scene words.** `GS_RESULTS` is silent in the scene table. With the online results entry gone the table had no online words left, so `scene_announcement` and `SceneSpeech::entered` lost their `online` parameter.
- **The wording**, as the speech log has it: `"Stock Battle. Winner: Mario. 1st Mario, P2, 0 KOs, 0 falls, 0 SDs, not KO'd. 2nd Link, P1, you, 0 KOs, 0 falls, 1 SD, KO'd at 0:01. Any button shows the stats. Start when ready."` (the debug match, first results of the run); `"Time Battle. No contest. Link, P1, you, 0 KOs, 0 falls, 0 SDs, total 0. Mario, P2, ..."` (L+R+A+Start); and with four CPUs nobody is "you": `"Stock Battle. Winner: Link. 1st Link, P1, 1 KO, 0 falls, 0 SDs, not KO'd. 2nd Mario, P2, 1 KO, 1 fall, 0 SDs, KO'd at 2:29. ..."`. A team match says `"Winner: Blue team."` and each player's team after the port ("2nd Link, P1, you, Red team, ..."). Screenshots of the Stock and No Contest summaries agreed with every number said.
- **Open question 1, the decoded strings.** SdRst on the NTSC-U disc: 317 strings, none malformed. The match types are 2 to 5 as the spec has it. Stat labels are 8 to 55, one string per row of the stats lists: a label row, then an empty string for the row that carries the value; "Attempted"/"Grabs" (38, 39) and "Transformation"/"Time" (54, 55) span two rows. Bonus names start at 60, not 58: the code adds 60 to `fn_8016F280(fn_8016F1F0(i))` (`fn_80174468`); 56 to 59 are "Friendly Fire", "Coin Points", "Swiped Coins", "Lost Coins". No unit strings ("%", "ft.") are in the table. The spacing rule and the units are issue 02's to settle at runtime; PAL is unchecked (no PAL disc), and `pc_region_sis_index` has no shift for this table. All in `native-menus.md`, "Results in detail".
- **Open question 4, the banner and ties.** Read from the code, no tie met in a run: outside teams the banner names `x6` = `x4`, the highest port whose `is_small_loser` is 0; in a team match the team `x5` and, as `x6`, the player of that team with the lowest `is_small_loser`. The place image is the winner's (1st with a crown) for `is_small_loser` 0 (teams: `x6`), otherwise `is_big_loser` (teams: the team's), which tied players share; `is_small_loser` is the game's tie-broken order. The opening says players by place, the banner's player first among the tied, then by `is_small_loser` and port.
- **Open question 5, name tags.** `player_standings[i].x4` is the tag's slot, 0x78 for none (drawn "Ｐ２", full-width), otherwise `GetPersistentNameData(slot)->namedata`, a Shift-JIS C string (`fn_8017BB94`). The fork decodes no tag anywhere; issue 02 needs a small Shift-JIS decoder, at least full-width ASCII.
- **Choices beyond the spec.** Bonus mode on a cancelled match shows its points as a dash, so the opening leaves the points out there ("Link, P1, you."). An unknown character is named by its port alone ("Winner: P2. 1st P2, ..."), logged once; a missing match-type text is left out, logged once. The opening is said on the first tick at state 2 or later, so a summary passed between two ticks would still be said (it cannot be in practice: state 2 waits for a press).
- **Tests.** `results_speech` (new suite): the opening waiting for state 2 and said once a visit, every match type, the spec's four-player Stock example word for word, Time's signs, Coin and Bonus with their singulars, No Contest in port order, Bonus mode's No Contest, a team match, a tie for 3rd and a tie for 1st with the banner's player first, nobody "you" on a CPU or empty port, online "you" on P2, empty ports left out, the keys on the first visit only, an unknown match type and an unknown character. `scene_speech` updated for the silent results.
- **Verification**, quick and menus levels with `MELEE_A11Y_CUES=1` (the tick hook changed): build with the "Rollback sections" line; style; unit tests (`launcher_data` failing as known); a bounded `title` run; `run_level.py menus` plus the seven match scripts, 30 of 30 passing, `results.drive` waiting on the new opening; probe runs of a No Contest and of the four-CPU debug match, and a text dump of SdRst (`MELEE_A11Y_TEXT_DUMP=1`, now dumping on results' first tick).
- **For issue 02.** The snapshot is ready to grow: add the pages, ready flags and rows beside `players`. `lbl_8046E190` and `lbl_804D3FC8` are not read (recompute them). `x0_23` reads 0 or 2 between ticks (1 lasts within one tick). The stats lists' label and value rows are separate entries (above), which bears on the hidden first row (open question 6). Name tags need decoding (above). `results_order`, `results_player_name` and `results_summary` give the Z line its parts; the Z line's order ("Mario, P2, 4th, ...") differs from the opening's ("4th Mario, P2, ...").
- **Left open.** The drive scripts for a timed match, a team match and the four-CPU match are issue 04's; the No Contest and four-CPU probes were ad hoc. The wording is for the play test.
