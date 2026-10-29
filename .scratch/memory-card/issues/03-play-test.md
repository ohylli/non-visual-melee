# 03 Play test by ear

Status: ready-for-human
Type: task
Blocked by: 02

The maintainer starts the game with NVDA as a new player would and judges the wording of `.scratch/memory-card/spec.md`.

## What to try

Use a test folder, never the real `%APPDATA%\melee-pc`: set `MELEE_USER_DIR` and `MELEE_CACHE_DIR` to an empty folder (the `verify` skill, "Every game run"), copy your `a11y.ini` in if you like, and start `build\melee.exe` from it with no `--no-card` and no `MELEE_BOOT_SCENE`.

- **The first prompt.** Does "Create Game Data?" follow "Non-Visual Melee ready" without cutting it off? Is the hovered "Yes" and the keys clear?
- **Moving.** Right to No, Left to Yes, and a press against each end.
- **No, then Continue.** "Continue without saving or loading Game Data?" with OK and Try Again; Try Again back to the first prompt.
- **Yes.** Is the long "Game Data has been created…" text worth hearing whole, and "A to continue" right?
- **A second start** with the same folder: no prompt, straight to the opening movie.

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and `CLAUDE.md`'s status line says the memory card prompts were play-tested.

## Comments

### 2026-10-29, code review (agent)

The review's fixes are in their own commit ("A11y: memory card prompts, review fixes"). Left for the maintainer, most severe first:

- **A later visit's first prompt waits behind old speech.** The first prompt of a game run is queued, to let the proof-of-life announcement finish (`MemcardSpeech::say_prompt`). When the save loads at startup, the run's first prompt comes later, from a `GM_MEMCARD` visit after a failed autosave, and it then waits behind whatever the last screen was still saying. Options: queue only on the startup visit (the scene hook's `mode_kind` already tells `GM_BOOT` from `GM_MEMCARD`, so no new hook), or keep it. Recommended: queue only at startup.
- **The prompts after a failed autosave are unreached.** Overwrite and "Continue without saving?" go through the same hooks and reader but were never shown in a run, so neither their words nor their moved default (No on Overwrite) has been heard or logged. Options: find a way to make an autosave fail in a driven run (a card folder made read-only mid-run, say), or leave them to the reader's shared path. Recommended: leave them until a player meets one.
- **A third copy of the game text step.** `MemcardSpeech::game_words` repeats `ResultsSpeech::game_words` and `MenuSpeech`'s description decode: find the string, decode it, log its trouble once, make it speakable; only the log prefix differs. Options: one helper beside `spoken_text` in `game_text.cpp` for the three readers, or leave the copies. Recommended: share it, as the sentence helpers were.
