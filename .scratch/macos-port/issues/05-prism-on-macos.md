# 05 Prism wired in on macOS

Status: resolved (2026-11-12)
Type: task
Blocked by: 01

Spec: `.scratch/macos-port/spec.md`, "Where things stand" and "Open questions". Read it first, then `src/pc/a11y/a11y.cmake` (the Windows branch and the download below it), ADR-0001, and `tools/package_macos.sh`.

## What to build

- **Read the zip first.** Download `prism-macos-universal.zip` for the pinned tag (`A11Y_PRISM_VERSION`) into its own empty directory in the scratchpad, list it, and record in a comment here: the header's place, the library's name and kind (`.dylib` or `.framework`), its variants (dynamic and static, release and debug), its architectures, and the licence files. Take its sha256 for the pin.
- **The Mac branch in `a11y.cmake`**, under `APPLE` and not iOS: the asset and its own sha256 (each platform needs its own hash: restructure the one `A11Y_PRISM_SHA256` into one per asset), the library to link and the runtime file, and the voice backend from issue 01 set to AVSpeech (`PRISM_BACKEND_AV_SPEECH`). Apple's clang links a dylib directly, so there is no import library.
- **Found at run time.** In a plain build directory the dylib is copied beside `melee` and the executable finds it (`BUILD_RPATH` `@executable_path`, or `install_name_tool -add_rpath`); check the dylib's own install name (`otool -D`) agrees with what `package_macos.sh` rewrites to `@rpath`.
- **The Mac backend's values for issue 01's table**: AVSpeech's id and log name. The default voices are issue 07's, chosen by ear; until then use Apple's default voice for both (an empty default, or whatever 01's table allows), and say so in a comment.

Nothing here can be built on Windows; issue 06's CI is where it compiles. Keep the Windows branch byte-for-byte as it is, apart from the hash restructuring.

## Done when

- The Windows configure and build are unchanged (verify at the quick level from a fresh configure, so the Prism download runs).
- The zip's layout is recorded here, and the spec's first open question is answered.
- Issue 06's Mac build links Prism and `ls Melee.app/Contents/Frameworks` in its log shows the Prism dylib; until 06 lands, say in a comment that this half is unchecked.

## Comments

### 2026-11-12, implementation (agent)

Implemented on Windows; the Mac half is unchecked until issue 06's CI job builds and packages it.

**The zip** (`prism-macos-universal.zip`, v0.18.2, sha256 `268cd7aa14e201f3281a35a0e39bd8b7f9085b9e044c2aa49e51ee1556f44062`, 16.9 MB), downloaded into a scratch folder, listed, and its Mach-O headers read with a small Python script (nothing in it run):

- **Layout** as the Windows zip's: `include/` (`prism.h`, `prism_version.h`, `tolk.h`), `dynamic/{release,debug}/`, `static/{release,debug}/`, `LICENSES/` and `NOTICE` at the top. So `a11y.cmake`'s extraction patterns (`include/* dynamic/release/* LICENSES/* NOTICE`) serve both platforms unchanged.
- **The library**: `dynamic/release/lib/libprism.dylib`, a plain `.dylib`, no `.framework`. Universal, x86_64 and arm64. Install name (`LC_ID_DYLIB`, what `otool -D` prints) `@rpath/libprism.dylib`, the same the zip's own `prism-targets-release.cmake` gives as `IMPORTED_SONAME`. Minimum macOS 11.0 (SDK 26.5) in both slices. It loads only the system's: Foundation, AVFoundation, AppKit, IOKit, CoreFoundation, `libc++`, `libSystem`, `libobjc`. No `LC_RPATH` of its own.
- **The other variants**: `dynamic/release/lib/` also holds `libtolk.dylib`, `libtolk.0.dylib` and `libtolk.0.18.2.dylib`, Prism's Tolk compatibility shim (install name `@rpath/libtolk.0.dylib`, loading `@rpath/libprism.dylib`; the zip stores the would-be symlinks as three copies), unused. `dynamic/debug/lib/` the same in debug, with `.dSYM` bundles. `static/release/lib/libprism.a` and `static/debug/lib/libprism.a` (fat archives), each beside a Tolk dylib. Each variant has its CMake package files under `share/prism/`.
- **Licences**: `LICENSES/<project>/` for prism (`mpl-2.0.txt`), simdutf, fmt, highway, concurrentqueue, dr_wav, djinni, moderncom, nvdaController and nvgt, and `NOTICE` at the top, the same set as the Windows zip; moderncom and nvdaController are Windows-only code, kept for attribution as `NOTICE` says.

**`a11y.cmake`.**

- **The hashes**: `A11Y_PRISM_SHA256` is now one per asset at the top, `A11Y_PRISM_SHA256_WINDOWS` and `A11Y_PRISM_SHA256_MACOS`, beside the one tag; each branch sets `A11Y_PRISM_SHA256` from its own, so the download below is unchanged. The Windows branch gained that one line and nothing else.
- **The Mac branch**, `elseif (APPLE AND NOT (IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))`, the test issue 04's target uses: the asset, its hash, `dynamic/release/lib/libprism.dylib` as both the library to link (Apple's linker takes the dylib itself) and the runtime file, `BUILD_RPATH` `@executable_path` on `melee`, and `A11Y_VOICE_BACKEND` `PRISM_BACKEND_AV_SPEECH`. No processor test: the one universal zip serves arm64 and Intel.
- **Found at run time.** The existing post-build step copies the dylib beside `melee`, where the rpath finds it in a build directory. `package_macos.sh` needs nothing: its `bundle_deps` sees `@rpath/libprism.dylib` in `melee`'s load commands, resolves it through the built `melee`'s rpaths (`@executable_path` becomes the build directory, where the copy is), copies it into `Contents/Frameworks`, sets its id to the `@rpath/libprism.dylib` it already has, and finds only system libraries below it; the `@executable_path/../Frameworks` rpath it adds then finds it in the bundle, and `codesign --force --deep` re-signs it with the rest. CMake may also put Prism's extraction directory in the build rpath, as it can for a library linked by full path; harmless, since either copy resolves.

**The bridge.** `kVoiceBackends` gained `{PRISM_BACKEND_AV_SPEECH, {"AVSpeech", {{{"", ""}, {"", ""}}}}, nullptr}`, with a comment that both voices use Apple's default voice until issue 07 picks two by ear and gives the row its turn (07, as this issue and the spec have it; issue 01's note said 08). `PRISM_BACKEND_AV_SPEECH` is in Prism's one header on every platform, so the row compiles on Windows too, unguarded.

**A bug the empty defaults met, fixed here.** On a fresh settings file the empty defaults were written as `your_voice.voice = ` and, read back at once, refused: `accessibility settings: line 165 not understood: your_voice.voice =; your_voice.voice stays`, the same for the opponents' line, and the screen reader told "Accessibility settings, lines 165 and 179 not understood." on every start of a Mac build. The unit tests had missed it: their speech is off, and they checked only the value read back. The value model (`value_model.cpp`) now reads an empty value of a choice whose default is empty as that default; any other choice's empty line is still not understood. The file's comment for such a choice ends `default empty.` instead of `default .`. Tests first: `an_empty_default_choice_reads_back_empty` in `test_accessibility_settings.cpp` (written empty with its comment, read back with no bad line or note, a hand edit naming a voice and emptying it again), and `the_defaults_come_from_the_backend` in `test_voice_settings.cpp` now checks that the empty line logs nothing and leaves no problem; both failed without the fix.

**Verified** on Windows, at the quick level:

- **A fresh download**: `build/prism-v0.18.2/` and `build/prism.dll` removed, the existing `build/` reconfigured (`a11y: downloading Prism v0.18.2 (prism-windows-x64.zip)`, its hash checked) and built; `prism.dll` and `prism.lib` came back byte for byte (sha256). `build.ninja` and `compile_commands.json` configured from this change are identical to those from the commit before it, so the Windows configure is unchanged.
- **The row**: the bridge compiled by hand with `A11Y_VOICE_BACKEND=PRISM_BACKEND_AV_SPEECH` passes the `static_assert`, and with `PRISM_BACKEND_SAPI` (no row) stops on it. A build whose Windows branch named AVSpeech for one run, put back after: a `title` run of 400 frames logs `voices unavailable: no AVSpeech backend` (Prism's Windows build has none) and `"Voices unavailable, using the screen reader."`, and with the fix writes both voice lines empty under `default empty.` comments and reads them back with no line not understood.
- Build, `check_style.py`, the unit tests (38 of 38, `launcher_data` passing this time), a `title` run of 400 frames with `MELEE_A11Y_SCREEN_READER=0 MELEE_A11Y_VOICE_PREVIEW=1` (OneCore, Zira and Mark, both samples' `plays` lines, `speech shutdown`, exit 0), and `run_level.py launcher_a11y port_menu_a11y launcher_voices port_menu_voices`, 4 of 4.

**Docs.** `docs/a11y/speech.md`: "The voice backend" names AVSpeech, says the table's rows compile everywhere and what an empty default means; a new "macOS" section under "Other platforms" on the dylib, its install name, and how it is found and packaged. ADR-0001 names the hash per platform and the Mac's dylib; ADR-0008 drops "planned". The spec's first open question is answered. The `verify` skill: the fresh-download check, AVSpeech beside OneCore, and the new choice test.

**Unchecked until 06**: the Mac build linking Prism, and `ls Melee.app/Contents/Frameworks` showing `libprism.dylib`. Also unseen: that the `BUILD_RPATH` lands (`otool -l build/macos/melee | grep -A2 LC_RPATH` should show `@executable_path`), and whether Apple's linker warns about the dylib's macOS 11 floor (the base port sets no deployment target, so it is the runner's macOS, above 11, and there should be no warning).

For the issues after this one:

- **06**: after `package_macos.sh`, `ls Melee.app/Contents/Frameworks` should list `libprism.dylib` and no `libtolk`; `otool -L Melee.app/Contents/MacOS/melee | grep prism` should print `@rpath/libprism.dylib`, and `otool -D` on the bundled copy the same. The configure log shows `a11y: downloading Prism v0.18.2 (prism-macos-universal.zip)`. The licences are in `build/macos/prism-v0.18.2/LICENSES/` and `NOTICE` (extracted there by the configure, as on Windows), so the package step copies them from the build directory as the Windows job does. A launch, if the job tries one, would log `[a11y] speech backend: <name>` and, with speech on, `voices: AVSpeech, ...`; the voices' calls wait for the main thread, so they may not get far before issue 07.
- **07** fills the AVSpeech row's defaults, with their language as issue 02 says, and its `turn`.
- **08**: the friend's Mac must run macOS 11 or later for Prism (the spec's second open question).
