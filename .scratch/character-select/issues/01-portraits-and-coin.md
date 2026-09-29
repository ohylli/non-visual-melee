# 01 Portraits and the coin speak

Status: resolved (2026-09-29)
Type: task

The first slice of `.scratch/character-select/spec.md`: the local player hears which portrait their coin is over and what their hand holds, in VS modes. Nothing is written to the game. Read the spec and the primer `docs/a11y/native-menus.md` (section "Character select in detail") first.

## Machinery

- **Frame hook.** `pc_a11y_css_frame(...)`, declared in `a11y_hooks.h`, called once in `mnCharSel_Scene_OnFrame` (`src/melee/mn/mncharsel.c`). Every variable of the screen is `static` in that file, so the hook passes pointers to what the fork reads: the screen's data (`mnCharSel_804D6CB0`), the player slots (`mnCharSel_803F0DFC`), the portrait table (`icons`), and the flags it needs (the number of hands `mnCharSel_804D6CF5`, the pending exit `mnCharSel_804D6CF6`). It stays one call on one line. Say in the hook's comment whether the scene's frame function runs before or after the hands update.
- **Hand hook.** `pc_a11y_css_hand(port, state, held, x, y)`, called at the end of `mnCharSel_CursorThink`, at the `update_display` label. It passes plain numbers, since the hand's struct is defined only inside `mncharsel.c`.
- **Snapshot.** `game_access.c` turns what the hooks hand over into a plain struct for the reader: per player slot its kind, hovered portrait, previous hovered portrait and chosen character; per portrait its rectangle, character and locked state; the local hand's state, held thing and position. Issue 02 extends it.
- **Reader.** Pure code in `css_speech.cpp`: keeps the previous snapshot, compares, composes. It speaks through the one `Speech` owned by `hooks.cpp`.
- **Forgetting.** The reader forgets its previous snapshot when `pc_a11y_scene_entered` reports any scene.
- **Whose hand.** Port 1's hand offline, the hand of `pc_net_local_player()` when `pc_net_active()`. Single-player modes come in issue 07; until then the reader stays silent there beyond the opening announcement.
- **Netplay gate.** Silent while `pc_net_resim()` is true, through the shared gate.
- **Picking up a coin** sets the slot's hovered portrait to `0xD` as a placeholder. Decide "the coin is over portrait n" from the previous hovered portrait together with the hand's state, as the spec's facts describe, not from the hovered portrait alone.

## Speech in this slice

Wording and rules are in the spec, section "Stage 1: announcements". This slice covers:

- The opening: "Character select. Player 1, no character." It replaces "Character select. No speech yet." in the scene table. When the player arrives with a character already chosen (after a match), the opening names it.
- A carried coin entering a portrait: "Fox". Leaving every portrait: nothing.
- A choice: nothing from speech.
- The hand picking up a coin, or the coin jumping into it: "Holding your coin".
- The coin going back with the character cleared: "No character".
- B putting the coin back on the earlier choice: "Back to Fox".
- A free hand moved over portraits by the stick: nothing.
- The costume changing: "Costume 2".
- A missing name: "Unknown character 27", logged.

## The string table

- `css_names.cpp`, keyed by `CharacterKind`, with the names as drawn under the portraits in normal capitalisation.
- Take a screenshot of the screen in a run with everything unlocked (`--no-card` builds such a save) and check every name against it. Record the 25 names in the spec, in portrait order.

## Stage select, one sentence

- Confirm with a drive run that Start on stage select, with the hand not moved, starts a match on a random stage.
- Then change the scene table's entry to "Stage select. No speech yet. Press Start for a random stage."

## Done when

- `cmake --build build` passes, and the unit tests pass with a new test of the reader. It covers:
  - the opening with and without a chosen character;
  - the coin entering a portrait, moving to the next, leaving all;
  - a locked portrait under the coin, which says nothing;
  - a choice, which says nothing;
  - picking up one's own coin, and the coin jumping into the hand;
  - the character cleared by carrying the coin down;
  - B putting the coin back;
  - a costume change;
  - a free hand moving over portraits, which says nothing;
  - another player's coin moving, which says nothing;
  - an unchanged snapshot;
  - a missing name.
- A drive script under `tools/a11y/` enters VS. Mode, Melee, moves the hand up into the portraits with the stick, waits on a portrait's announcement, presses A, then B, and waits on each expected `[a11y]` line. Stick holds are inexact, so the script waits on "any portrait name" where it cannot know which.
- `tools/a11y/tree_walk.drive` still passes with the new opening announcement.
- `python tools/check_style.py` passes.
- The footprint grep shows the two new hooks.
- `CONTEXT.md` has the spec's new terms.
- The primer is corrected as the spec's "Docs" section lists, as far as this slice found it to be true.
- CLAUDE.md: one "Accessibility status" line, and the scene announcement's new wording where it is quoted.

## Comments

### 2026-09-29, implementation (agent)

Implemented; hearing it is issue 03.

- Hooks: `pc_a11y_css_frame` at the top of `mnCharSel_Scene_OnFrame`, which runs before the hands update (`gm_RunSimTick` calls the scene's frame function before `HSD_GObj_RunProcs`); `pc_a11y_css_hand` after `updateCursorDisplay` at `update_display`. `mncharsel.c` includes `a11y_hooks.h` itself, though `inlines.h` already brings it in. The hand hook is not reached on the early returns (a hidden hand, A on the rules header); the hidden hand keeps its last report, which is harmless while the reader follows port 1.
- The snapshot (`a11y_game_css_state`) turns the raw numbers into the local hand's presence, carried coin and position; per slot its kind (human, CPU, closed), portrait, the portrait under a carried coin, character and costume; per portrait its character, locked state and rectangle. The hand hook's reports live in `hooks.cpp` and are cleared on every scene change.
- **Bitfield layout.** `game_access.c` first read every player's character as Marth, whatever was chosen: `melee_game` compiles with `-mno-ms-bitfields` on Windows, the `melee` target does not, and `StartMeleeRules` (before the players' data) is 136 bytes one way and 144 the other. `a11y.cmake` now gives `game_access.c` the same option, and a static assertion on the rules' size fails the build if the two ever differ again (checked by building once with the wrong option). The base port's own `src/pc/net.c` and `src/pc/slp.c` read the same structs from the `melee` target without the option; not touched here.
- **Stale character.** On first arrival every slot is closed ("N/A") and player 1's character held Marth with no coin on any portrait. The opening names a character only when the slot is open and its coin rests on a portrait.
- **Pickup wording.** The frame a coin is picked up still shows where it lay; where it is under the hand shows a frame later. Spoken at once, "Holding your coin" was cut off 16 ms later by the portrait under the hand after B called the coin back. The pickup is now announced a frame later together with that portrait: "Holding your coin. Fox". A coin jumping into the hand over no portrait is "Holding your coin" alone. For the play test to judge.
- "Back to Fox" is told from a choice by the character: B leaves it unchanged and lands on a portrait other than the one under the coin; A sets it from the portrait under the coin. A choice of a random character in a hidden corner that happens to equal the earlier one would be read as "Back to"; not handled.
- Names checked against a screenshot with everything unlocked and recorded in the spec. The table also holds Sheik, for a slot that keeps her.
- Stage select: a drive made player 2 a CPU, chose a character, pressed Start, and on stage select pressed Start without moving; the match started (on Yoshi's Story). The code agrees: nothing is hovered on arrival (cell 30), and Start there rolls a random stage. The scene table now says "Stage select. No speech yet. Press Start for a random stage."
- `tools/a11y/css_portraits.drive` passes: the opening, the coin jumping in, a portrait, A (silent), X and Y ("Costume 2", "Costume 1"), B ("Holding your coin. Falco"), a push onto the next portrait, B ("Back to Falco"), B again and the coin carried down ("No character"). `tree_walk.drive` and `leaf_screens.drive` pass. The bounded `title` and `vs` runs exit cleanly.
- Unit tests: `css_speech` covers the issue's list plus a stale character, the online player number, a CPU's coin, a carried coin's costume, single-player modes and a leaving screen; `scene_speech` follows the table. `launcher_data` fails as before on Windows. `python tools/check_style.py` passes.
- Docs: `CONTEXT.md` has the spec's eight terms (Target under native menus, the rest under a new "Character select" heading). The primer gains the private state, the `0xD` placeholder, the chosen character and the bitfield gotcha, hovering at the coin, the three ways the coin reaches the hand, letting go, the hidden corners (from the code) and the unplugged controller, and loses the answered open question. Rewording "Read, don't steer" is left to issue 04, which brings the steering it describes; the single-player arrows to issue 07.
