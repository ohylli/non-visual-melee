# 04 `game_access.c` compiled by GCC on Macs

Status: ready-for-agent
Type: task

Spec: `.scratch/macos-port/spec.md`, "A fork bug only a Mac would hit". Read it first, then the base port's `docs/building.md`, "macOS", and the GCC routing in the root `CMakeLists.txt` (the `C_COMPILER_LAUNCHER` lines for `melee_game`).

## Why

`game_access.c` reads decomp structs, among them `MapCollData::line_count`, a disc struct stored big-endian. GCC reads it right through `scalar_storage_order`; Apple's clang, which builds the `melee` target on a Mac, has no such attribute and `DISC_STRUCT` is empty there, so the count would come back byte-swapped and the floor tests behind the gap and ledge warnings would loop over the wrong lines, perhaps past the end.

## What to build

- **`game_access.c` in a target of its own**, a small static library built from `a11y.cmake`, given `tools/gcc_launcher.py` as its C compiler launcher on macOS, as `melee_game` has (not on iOS, which has its own launcher and no fork support). The same target on every platform, so the Windows build exercises it.
- **Same flags as now**: the include directories and definitions the file gets from the `melee` target today, `-mno-ms-bitfields` on Windows and `-Wno-scalar-storage-order` under GCC (`a11y.cmake`'s comment explains both). Compare the compile command before and after in `build/compile_commands.json` or `ninja -t commands`.
- **The rollback snapshot sections.** The root `CMakeLists.txt` marks sections for `melee` and `melee_game` (`melee_enable_windows_snapshots`, `melee_enable_apple_snapshots`); `game_access.c` is fork code and must stay outside what rollback saves and restores, as it is now. Find out how the new target is treated, make it match today's, and check the link's "Rollback sections" verification line.
- **A comment** at the target saying why it exists.

## Done when

- On Windows the build, the unit tests and `python tools/check_style.py` pass, the "Rollback sections" line shows as before, and a match run's gap and ledge warnings behave as before (verify at the menus level, with the match cue scripts).
- The compile command for `game_access.c` on Windows is unchanged apart from where it sits.
- Issue 06's Mac CI build shows `game_access.c` compiled by Homebrew GCC; until 06 lands, say in a comment that this half is unchecked.
