# 04 `game_access.c` compiled by GCC on Macs

Status: resolved (2026-11-12)
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

## Comments

### 2026-11-12, implementation (agent)

Implemented on Windows; the Mac half is unchecked until issue 06's CI job builds it.

- **The target.** `a11y_melee_game`, a static library in `a11y.cmake` built from `game_access.c` alone, on every platform, linked into `melee`; the file left `melee`'s source list and its `set_source_files_properties` line. It links `melee_game` privately, which gives it the game's include directories and definitions the way `melee` gets them and puts `libmelee_game.a` after it on the link line, so its references to the game resolve. Its own options: the a11y include directory, `MELEE_FP_FLAGS` as `melee` has them, `-mno-ms-bitfields` on Windows and `-Wno-scalar-storage-order` under GCC. On macOS (not iOS) its C compiler launcher is `tools/gcc_launcher.py`, as `melee_game`'s.
- **The name.** `gcc_launcher.py` sends a compile to GCC only when an argument holds `melee_game` (or a decomp path); everything else goes back to clang. The target's object directory, `CMakeFiles/a11y_melee_game.dir/...`, is that argument, hence the name; the comment at the target says so. A base port change to that test would quietly send the file back to clang, which the guard below catches.
- **A guard in the file.** `game_access.c` now stops with an `#error` under clang (outside the browser build's `MELEE_DISC_LOWERING`), where `DISC_STRUCT` is empty and the disc structs read byte-swapped. So the Mac half checks itself: a Mac build that compiles the file at all compiled it with GCC. It also turns the same silent misread on Android and iOS, whose `melee` target is clang too, into a build error; the fork builds neither.
- **The compile command** (`build/compile_commands.json`, before and after): the same compiler, optimisation, code generation flags, include directories and definitions of the game and aurora, in the same order, but the object path. Gone are what belongs to other libraries and features, which no header the file includes reads: `-DA11Y_HAVE_PRISM`, `-DA11Y_VOICE_BACKEND=...`, `-DMA_NO_DEVICE_IO`, `-DMA_NO_ENCODING`, `-DMELEE_STATE_SECTIONS=1`, and the monocypher, Prism and miniaudio include directories; `-std=gnu11` moved one place. The object built is byte for byte the one built before (`cmp`), so the difference is only on paper. Mirroring `melee`'s flags exactly through `$<TARGET_PROPERTY:melee,...>` was weighed and left: it would bring the miniaudio directory as `-I` instead of `-isystem`, and on a Mac hand clang's options of `melee` to GCC.
- **Rollback.** Nothing changes: the snapshot functions (`cmake/WindowsSnapshot.cmake`, `cmake/AppleSnapshot.cmake`) wrap `melee_game`'s launcher alone, so this target's objects keep the ordinary sections, as `melee`'s did; the ELF linker script matches `libmelee_game.a`, not `liba11y_melee_game.a`. `game_access.c`'s object has no data or bss of its own anyway. The link still ends with `Rollback sections: game globals included, engine/audio globals excluded`.
- **Verified** on Windows: build, `check_style.py`, the unit tests (37 of 38, `launcher_data`, the known base port failure, alone failing), a bounded `title` run, and the menus level with the match cue scripts and the match announcements' set, cues on (`MELEE_A11Y_CUES=1 run_level.py menus match_position match_opponent match_gap match_gap_battlefield match_ledge match_ledge_battlefield match_port_menu match_damage match_thresholds match_stocks match_time match_stock_steal`): 43 of 46 passed side by side; `sss_steps`, `css_coins` and `results_cpu4` failed on lost presses (two with "no pad change seen") and passed on an unchanged rerun. The gap and ledge scripts' counts held.
- **Docs.** `docs/a11y/gameplay-state.md`, "Reading safely", has a line on disc structs needing GCC and where `game_access.c`'s routing lives. The `verify` skill is unchanged: no step, line or failure it names changed.

For the issues after this one:

- **05** edits `a11y.cmake`'s Prism branch; this target's block sits between `melee`'s source list and `if (A11Y_PRISM_ASSET)` and takes nothing from Prism.
- **06** checks the Mac half. Ninja's usual progress line (`Building C object CMakeFiles/a11y_melee_game.dir/src/pc/a11y/game_access.c.o`) names no compiler, and even a verbose build (`cmake --build build -v`) shows the launcher in front of CMake's compiler (`tools/gcc_launcher.py /usr/bin/cc ...`), not the GCC it runs. The build passing is the proof, since the `#error` stops a clang compile of the file; for a line in the log, the job can show the launcher in the verbose command for that one file (`cmake --build build -v 2>&1 | grep game_access`) and that `gcc-NN` exists (`which gcc-15` or the like), or ask the object (`strings` on it for GCC's `GCC: (Homebrew GCC ...)` ident, if Mach-O GCC objects carry it). Then tick this issue's last Done-when item here.

### 2026-11-12, Mac half checked (agent, issue 06)

The fork's CI built the Mac packages on both arches (https://github.com/ohylli/non-visual-melee/actions/runs/43226714268, arm64 on its second attempt), so the last Done-when item is met: `game_access.c` was compiled by Homebrew GCC on the Mac.

- **The proof** is the build passing at all: the file's `#error` stops a clang compile.
- **In the log**, the macOS job's step "Show game_access.c compiled by GCC": `ninja -t commands a11y_melee_game` shows the compile as `tools/gcc_launcher.py /usr/bin/clang ...`; the launcher's search finds `/opt/homebrew/bin/gcc-16`, `gcc-16 (Homebrew GCC 16.2.0) 16.2.0`; and the object's debug info names its compiler, `DW_AT_producer ("GNU C11 16.2.0 -mmacosx-version-min=13.3 -mcpu=apple-m1 ... -fexec-charset=CP932 ...")` on arm64 and the same with `-mtune=core2` on Intel. `-fexec-charset=CP932` is the launcher's own GCC flag, so the line could not come from clang. GCC writes no ident string into a Mach-O object; the debug info is where it shows.
- **Rollback**: the link still ends with `Apple rollback sections: game globals included, engine/audio globals excluded`.
