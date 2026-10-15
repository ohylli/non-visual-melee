# 01 The Training menu reader

Status: resolved (2026-10-15)
Type: task

The reader of `.scratch/training-menu/spec.md`: everything but the drive script. Read the spec first, then `docs/a11y/native-menus.md` and `docs/a11y/gameplay-state.md` ("The tick hook", "Pause, freezes and slow motion").

## Machinery

- **Snapshot.** `game_access.c` reads `gm_80473814`: the row (`x00`), whether the menu is open, and the hovered row's value from `menu_values`. Find what marks the menu open (spec, open questions 1 and 2).
- **Tick hook.** `pc_a11y_match_tick` in `hooks.cpp` feeds the reader each tick, whether or not the cue system exists; the cue system's tick stays as it is.
- **Session.** The hint is said on the first opening after each entry to the Training scene (spec, open question 3).
- **Names.** Row names and value words as the spec has them. Item names shared with Item Switch, keyed by the item (spec, open question 4); Item Switch's speech must not change.
- **Held directions.** The quarter-second rule on rows and values, on a clock the test can fake.

## Done when

- `cmake --build build` passes, and the unit tests pass with the reader's test covering the spec's list ("Tests").
- `test_menu_speech` still passes unchanged for Item Switch's names.
- A bounded run shows the match cues still start and stop as before.
- `python tools/check_style.py` passes.
- `docs/a11y/native-menus.md`'s Training menu entry corrected and extended as the spec's "Docs" says, with the answers to the spec's open questions where they belong.

## Comments

### 2026-10-15, implementation (agent)

Implemented; the Training menu speaks its opening, rows and values in every Training match.

- **Where things live.** `game_access.c`: `a11y_game_training_menu`, the snapshot (`A11yTrainingMenu`: open, row, the row's value) and the row numbers (`A11Y_TRAINING_*`). `training_speech.hpp/.cpp`: the reader, `TrainingSpeech`, with `session_started` and `tick(menu, now)`. `training_names.hpp/.cpp`: the rows, the value words and the fork's copy of `gmTraining_ItemTable`'s kinds. `item_kinds.h` and `item_names.hpp/.cpp`: the shared item names. `hooks.cpp`: the tick hook feeds the reader in the Training scene before the cue system's tick, which stays as it was; the scene hook starts a session. No new hook in a base port file.
- **Open question 1, what marks the menu open: `x01`.** The menu's own update (`fn_801891F4`) sets it on the first tick it runs with the pause word's bit 2 and clears it on the tick the bit clears, applying the values: once per Start, and the 10-tick lockouts mean a close and reopen are two real Starts. The pause bit would do nearly as well but is set elsewhere; `anim_frames` is drawing only.
- **Open question 2, input during the slide-in: yes.** The update reads the controller from the first tick the menu is open. A press on the opening tick itself is in the opening ("Training menu. Item: Food."); a press within a quarter second after it is held back by the rule below and then said queued after the opening, not interrupting it. Measured: Start, then Down 60 ms later, gave the opening and `speak queue (off): "Item: Food"` 0.43 s later. Every later change interrupts.
- **Open question 3, the session: the scene hook.** `pc_a11y_scene_entered` with `GS_TRAINING` runs right after the scene's start has reset the menu, so it calls `session_started`. Measured through the menus: Finish, Training character select, Start, Start on stage select, and the next opening had the keys again with the cursor back on Speed.
- **Open question 4, the shared names: by `ItemKind`.** `item_kinds.h` mirrors the 31 kinds Item Switch names, checked against the decomp's numbers in `game_access.c`; `item_names.cpp` names them; Item Switch's rows now carry their item (`item_switch(row, kind)`, beside Random Stage's `stage_switch`) instead of a name, and `ReaderRow::spoken_name` looks it up. `test_menu_speech.cpp` is unchanged and passes; its target links `item_names.cpp` now.
- **Open question 5, for issue 02.** `press Right 2000` holds the direction for the whole two seconds and the game auto-repeats through it, so a script can hold a direction; no fast presses needed.
- **The final wording**, as the speech log has it (`[a11y] speak interrupt (off): "..."` in agent runs):
  - opening, first of a session: `"Training menu. Speed: x1. Up and down for rows, left and right to adjust, A on Item, Reset and Finish, Start to close."`; later ones `"Training menu. CP Damage: 57."`, `"Training menu. Reset."`;
  - rows: `"Speed: x1"`, `"Item: Food"`, `"No. of CPs, 1"`, `"CP, Stand"`, `"CP Damage: 0"`, `"Camera: Normal"`, `"Reset"`, `"Finish"`, no full stop;
  - values alone: `"x2"`, `"x1.5"`, `"x1"`, `"2/3"`, `"1/2"`, `"1/4"`; the item names; `"1"` to `"3"`; `"Stand"`, `"Walk"`, `"Evade"`, `"Jump"`, `"Attack"`, `"Human"`; the damage number; `"Normal"`, `"Zoom"`, `"Free"`;
  - an unknown value is its number, and logs `[a11y] training menu row 1: value 31 not in the training menu names table` once; the hidden row says nothing and logs `[a11y] training menu row 5: not in the training menu names table` once (the game never puts the cursor there).
- **Timing for issue 02.** The opening and the cue stops come on the same tick, the opening first. The held-direction rule runs on real time (`TrainingSpeech::kSettle`, 250 ms, `steady_clock`), checked once a tick. The game's first auto-repeat comes 20 ticks (a third of a second) after the press, more than a quarter second, so **a held direction says the press, then the first repeat, then the value it settles on**, not the press and the settled value alone as the spec's summary has it. Measured, Right held 2 s on CP Damage: `"1"`, `"2"` 0.35 s later, then `"20"` a quarter second after the last repeat; Down held 1 s from CP Damage: `"Camera: Normal"`, `"Reset"`, then the settled row. At 1/4 speed every repeat is far apart in real time (the first 1.33 s after the press), so each is said. A script waiting on the settled value should not pin the number: the repeats a hold gets depend on the input driver's timing.
- **Deviation kept for the play test.** The quarter second stays as the spec has it; widening it past a third of a second (about 0.4 s) would make a hold "first, then settled" but would also hold back taps closer than that. The play test (issue 03) should judge the extra second word.
- **Tests.** `training_speech` (new suite, fake clock and bridge): the opening with and without the keys, the keys once per session and again in the next, the remembered row, every row down and round with the wrap and Up skipping the hidden row, every value word with the wraps, the item names matching Item Switch's by kind, the hidden row never named, Reset leaving other rows unsaid, a slow series, a burst of values and of rows, a burst then a pause, a burst then closing, a press during the opening and on its tick, reopening, an unknown value and nothing while closed.
- **Verification**, menus and online levels with `MELEE_A11Y_CUES=1`: build with the "Rollback sections" line; style; unit tests (`launcher_data` failing as known); a bounded `training` run with cues on, both position cues starting as before; probe runs of the menu from the `training` boot scene and through the menus; the sixteen menu scripts, the six match scripts (`match_position` with its six `cue your position st` lines) and the four online runs, all passing.
- **Docs.** `native-menus.md`: the Training menu's entry corrected (no pause notice) and a new section, "The Training menu in detail", with the answers above; its candidate hook point now says the tick hook. `gameplay-state.md`, "The tick hook": it feeds the reader in Training. The `verify` skill: the new suite, the tick hook no longer idle without cues, and `match_position`'s Training lines. The CLAUDE.md status line and the drive script are issue 02's.
- **Left open.** Whether NVDA reads "No. of CPs" as "no" (the spec's fallback is "Number of CPs"), and the quarter second, both for the play test.
