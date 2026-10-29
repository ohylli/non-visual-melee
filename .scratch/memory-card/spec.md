# Memory card prompts: speech

Status: agreed 2026-10-29 (design conversation with the maintainer)

The maintainer agreed:

- The scope: every memory card prompt the game shows, read in the game's own words.
- What a prompt says, what moving the choice says, and the keys.
- The first prompt of a run waits for earlier speech; later prompts interrupt.
- A `drive.py --card` switch to reach the prompts in agent runs.

The wording is a first proposal for the play test to judge.

## Goal

A blind player starting the game for the first time hears the "Create Game Data?" prompt, moves between Yes and No, and hears what each leads to, so the first save is made knowingly. The same holds for the rarer prompts (a corrupted save, a failed autosave).

Vocabulary is `CONTEXT.md`: memory card prompt, choice, announcement, scene. How the game's menus run and its game text are in `docs/a11y/native-menus.md`.

## Scope

In:

- **The memory card scene, `GS_MEMCARD`** (`src/melee/gm/gmscmemcard.c`, its message window in `gm_1ADD.c`), in both the modes that run it:
  - at startup (`GM_BOOT`), when the save cannot simply be loaded;
  - later (`GM_MEMCARD`), at a mode change after an autosave failed or the card folder appeared or disappeared.
- **Every prompt either mode can show**, read by its text number, the unreachable ones included at no extra cost.

Out:

- **Other card screens**: VS camera snapshots (`SdVsCam`), the snapshot album (`mnsnap.c`) and Erase Data (`mndatadel.c`). They have their own screens and tables.
- **The corner saving icon** (`lbCardGame_InitScene`): visual only, and says nothing worth hearing.
- **Disc regions other than the US one**: unchecked (see "Regions").

## What the game gives us

From reading `gmscmemcard.c`, `gm_1ADD.c` and `extern/aurora/lib/dolphin/card.cpp`, and from six driven runs on 2026-10-29.

### The card on PC

- aurora emulates slot A as a folder of save files: `<user dir>/USA/Card A/`, where the user dir is `pc_get_pref_path()` (`%APPDATA%\melee-pc`, or `MELEE_USER_DIR`).
- A fresh user dir gets an empty, formatted card folder. So a new player's card is present and holds no save: the "no saved Game Data" prompt.
- `--no-card` (`aurora_card_set_present(false)`, `src/pc/main.c`) means no card at all: "There is no Memory Card in Slot A." `gmmain_lib.c` then builds a save with everything unlocked. It is a command-line switch only.
- A valid save loads silently: the scene shows nothing and the opening movie starts.
- `MELEE_BOOT_SCENE` skips `GM_BOOT` and so the scene; `drive.py`'s default `--scene title` does. A run that wants the scene uses `--scene ""`.

### State

All in the file-static `enter_data` of `gmscmemcard.c`:

- `.decision`: the scene's state.
- `.unk1C`: the hovered choice, 0 for the left one (Yes, OK), 1 for the right one (No, Try Again).
- `.unk0.unk0`: 0 at startup, 1 in `GM_MEMCARD`.

### Prompts

Every prompt goes through `gm_801AEE6C(kind, text_id, line_count)`:

- **Kind 0**: a message with no choice. A or Start continues.
- **Kind 1**: Yes and No (strings 3 and 4).
- **Kind 2**: OK and Try Again (strings 2 and 5).

`gm_801AF250` picks a prompt from the card check's result and, for some prompts (codes 1 and 2 in `GM_MEMCARD`, 3 and 9), moves the default to the right choice just after `gm_801AEE6C` returns.

The texts are game text from `SdMsgBox.usd` (`SIS_MessageData`), loaded into font slot 3 by `gm_801AEBB0`. Decoded on the US disc (" / " marks the screen's line breaks):

| Id | Text | Kind | Reachable on PC |
|---|---|---|---|
| 6 | The Memory Card in Slot A / has saved Game Data. / Overwrite this Game Data? | Yes/No, No hovered | after a failed autosave |
| 7 | The Memory Card in Slot A / has saved Game Data. / Load from this Memory Card? | Yes/No | practically never |
| 8 | The Game Data on the Memory Card / in Slot A is corrupted. Erase this / and create new Game Data? | Yes/No, No hovered | a corrupted save (seen) |
| 9 | Erasing the corrupted data... / Do not touch the Memory Card / or POWER Button. | message | one frame at most |
| 0xA | The Memory Card in Slot A / has no saved Game Data. / Create Game Data? | Yes/No, Yes hovered | a fresh install (seen) |
| 0xB | Creating new Game Data. / Do not touch the Memory Card / or POWER Button ! | message | one frame at most |
| 0xC | Game Data has been created. / This game saves data automatically / while you play. Please do not / touch the Memory Card or POWER / Button while saving or copying. | message | after Yes on 0xA (seen) |
| 0xD | Game Data could not / be created. | message | a disk write error |
| 0xE to 0x15 | not enough space, no more files, must be formatted, formatted, could not format, could not be accessed, incompatible, damaged | mixed | practically never |
| 0x16 | There is no Memory Card / in Slot A. | message | `--no-card` (seen) |
| 0x17 | Continue without saving? | OK/Try Again, OK hovered | after a failed autosave |
| 0x18 | Continue without saving / or loading Game Data? | OK/Try Again, OK hovered | after No (seen) |

The card calls are synchronous on PC, so "Erasing…" and "Creating…" are replaced on the next frame and never seen.

### Flow

The common sequences, all seen in runs:

- Create Game Data? → Yes → Game Data has been created → the opening movie.
- Create Game Data? → No → Continue without saving or loading Game Data? → OK → the opening movie, no save written; or Try Again → Create Game Data? again.
- No Memory Card → Continue without saving or loading Game Data? → Try Again → No Memory Card again.
- Corrupted → Yes → Game Data has been created → the opening movie.

### Input

- Left and Right on the D-pad or stick, from any connected controller, move the choice. It stops at the ends rather than wrapping. A move plays the move sound.
- A or Start confirms, with the confirm sound. B does nothing.

### Startup timing

The fork's "Non-Visual Melee ready" is said at startup. On a fresh install the prompt is up about 3 seconds after launch.

## Decisions

### Hosting

- **Two hooks**, each one line in `gmscmemcard.c`, declared in `a11y_hooks.h`:
  - `pc_a11y_memcard_prompt(kind, text_id)` at the start of `gm_801AEE6C`: a new prompt, its kind and its text number.
  - `pc_a11y_memcard_frame(hovered_choice)` at the top of `gm_Scene_MemCard_OnFrame`, passing `enter_data.unk1C`.
- **A reader in `src/pc/a11y/`** keeps the prompt last shown and the choice last said. A prompt is spoken only once it has stayed up through a whole frame of the scene, so the one-frame "Erasing…" and "Creating…" stay silent and the game's moved default is the choice said. Implementation confirms this in a run: "Creating new Game Data" must never reach the speech log.
- **The words** are read by number from `SIS_MessageData` on font slot 3, with a symbol check, through `a11y_game_string` (`game_access.c`), decoded with `decode_game_text` and spoken with `spoken_text` (`game_text.cpp`), as the menu descriptions and results are. A text that cannot be decoded is logged once through `LogOnce`; the choice names (strings 2 to 5) come from the same table.
- **Read only.** The reader writes nothing in the game.
- **The usual speech gate**: `game_hook_may_speak()`.
- **Scene words.** `GS_MEMCARD` stays silent in `scene_speech.cpp`: the prompt is the scene's opening.

### What is said

- **A prompt with a choice**: its text, the hovered choice, and, on the first prompt with a choice in a run, the keys. "The Memory Card in Slot A has no saved Game Data. Create Game Data? Yes. Left and Right choose, A confirms."
- **Moving the choice**: the choice alone. "No". A press against the end says nothing, as the game plays nothing.
- **A message**: its text and the key, every time. "Game Data has been created. This game saves data automatically while you play. Please do not touch the Memory Card or POWER Button while saving or copying. A to continue."
- **The text as shown**: line breaks become spaces; the long text is said whole.
- **Interrupting.** The first prompt of a run is queued after whatever is being said (the ready line). Every later prompt and every choice move interrupts.
- **Confirming** says nothing of its own; the next prompt, or the next scene's announcement ("Opening movie. Press Start to skip."), follows.

### Regions

Only the US disc is checked. `pc_region_sis_index` has no PAL shift for `SIS_MessageData`; a wrong table or an undecodable text is logged once and the prompt is said without it.

## Tests

- **Unit tests** (a new `memcard_speech` suite): the prompt held for a frame before it is said, a one-frame prompt never said, the moved default said, the keys on the first prompt with a choice only, a choice move said alone, a press at the end silent, messages with "A to continue", queueing for the first prompt and interrupting after it, an undecodable text.
- **`drive.py --card`** (issue 01): a run with an empty card in its own settings folder, or a prepared save copied in, so agent runs can reach the prompts. `--no-card` stays the default.
- **Drive scripts** (issue 02): the fresh install's Yes path and No, Try Again, OK path, and the corrupted save with a prepared save. The no-card prompt with `--scene ""` alone.

## Docs

- `docs/a11y/native-menus.md`: a "Memory card prompts in detail" section with the state, the prompt function, the table and the flow above, and the screen inventory pointing to it.
- The `verify` skill: `--card`, the new scripts and their expected lines.
- `CLAUDE.md`: the drive section names `--card`; one "Accessibility status" line for the memory card prompts.
