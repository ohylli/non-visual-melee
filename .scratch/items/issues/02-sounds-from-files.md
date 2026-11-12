# 02 The sounds: the preparation script and loading from resources

Status: ready-for-agent
Type: task
Blocked by: none

Spec: `.scratch/items/spec.md` ("The sounds"). Read it first, then `docs/a11y/cues.md` ("Sounds", "The engine's rules" for the lead-in) and `cue_sounds.hpp`.

## What to build

- **`tools/a11y/prepare_sounds.py`**, run with `uv run`, its dependencies (numpy, soundfile) declared in the script's own metadata block so uv installs them. It takes an original file by path (WAV or FLAC, any rate, any channel count) and an output name, and writes `resources/a11y/<name>.wav`, mono, 48 kHz:
  - silence trimmed at both ends below a threshold;
  - the end faded over a few milliseconds, so the trim never clicks;
  - normalized by peak to just under full scale;
  - no lead-in: the engine adds it.
  The thresholds and levels are constants in the script. `--help` says what it does.
- **Its report**: the result's length, and for a pulsed cue the fastest interval at which a pulse plays whole at a given lowest pitch (`--lowest-pitch`, in octaves down), so the maintainer knows which paces the engine will cut (issue 01).
- **Loading.** `cue_sounds` reads a sound from a file through `ma_decoder`, asked for mono 48 kHz floats, then shapes it as the other sounds are (the lead-in). The path is `SDL_GetBasePath()` plus `resources/a11y/`, as the base port finds its own resources. A missing or unreadable file gives an empty sound: the cue stays silent, with one log line naming the file and why; nothing else fails.
- **Placeholders.** Until the maintainer's files arrive, `resources/a11y/item_appearance.wav` and `resources/a11y/item_position.wav` are placeholders run through the script: short, plainly different sounds, the second short enough to pulse. The maintainer's files replace them by the same script run; the issue that brings them notes the command.
- **The package.** Check that `cmake --build build` copies `resources/a11y/` beside `melee.exe` and that `tools/package_windows.sh` takes it into the Windows package, with no base port file touched.

## Done when

- The script runs with `uv run` on a test file of each kind (stereo, 44.1 kHz, leading and trailing silence) and its report is right; it refuses a missing input with a clear message.
- A unit test loads a small WAV through the new reader into a `Sound` of the right length at 48 kHz, and a missing file gives an empty sound and a log line.
- `build/resources/a11y/` holds both files after a build.
- The `verify` skill names the script and how to check it; `cmake --build build`, the unit tests and `python tools/check_style.py` pass.

## Comments
