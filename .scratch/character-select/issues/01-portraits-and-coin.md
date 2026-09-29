# 01 Portraits and the coin speak

Status: ready-for-agent
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
