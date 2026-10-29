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
