# 01 The results snapshot and the opening

Status: ready-for-agent
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
