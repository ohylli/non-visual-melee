# 05 Prism wired in on macOS

Status: ready-for-agent
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
