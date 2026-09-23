# macOS port of speech

Status: parked (2026-09-23). Needs a Mac with VoiceOver and a session with its owner; a friend of the maintainer has one. Not before the speech feature has shipped on Windows.

## Why this should be cheap

The speech feature (`.scratch/screen-reader-speech/`) was laid out so a second platform is a cmake branch, not a code change:

- The screen reader bridge is the only fork file that includes `prism.h`, and its real body is gated on the cmake-defined macro `A11Y_HAVE_PRISM`, not on `_WIN32`. Prism's C API is the same on every platform it ships for, so the bridge should compile unchanged on macOS.
- `a11y.cmake` keeps the platform choice in one variable (`A11Y_PRISM_ASSET`, the release zip name). Prism v0.18.2 publishes `prism-macos-universal.zip` (arm64 and x86_64 in one dylib; macOS 11 or later) next to the Windows zip, under the same tag and hash-pinnable the same way. Linux has `prism-linux-x64.zip` and `prism-linux-arm64.zip`.
- The base port already builds and packages macOS (`tools/package_macos.sh`, Xcode clang for C++ plus Homebrew GCC for the decomp C; see `docs/building.md`), so the game itself is not the porting job.

## What the port consists of

1. **Cmake branch** in `src/pc/a11y/a11y.cmake`: on `APPLE`, set the asset to the macOS zip and its hash, download and extract like Windows, link the dylib from the zip's dynamic release variant, define `A11Y_HAVE_PRISM`. Verify the zip layout on the day; it may not mirror the Windows one. For a plain build directory, copy the dylib beside `melee` and make sure the executable finds it (`install_name_tool -add_rpath @executable_path` or a `BUILD_RPATH`). The Windows import-library question does not exist on macOS: clang links the dylib directly.
2. **Bundling**: nothing new. `tools/package_macos.sh` walks the executable's dependencies with `otool`, copies every non-system dylib into `Melee.app/Contents/Frameworks` and rewrites load paths to `@rpath`. Prism is picked up by that walk once the game links against it. Check `ls Frameworks` in the script output shows the Prism dylib.
3. **Code signing**: nothing new either. The script ad-hoc signs the whole bundle with `codesign --force --deep --sign -`, which re-signs every dylib inside, Prism included, and then verifies it. Background: Apple Silicon refuses to run unsigned native code, so every binary needs at least an ad-hoc seal, and everything inside a bundle must be signed consistently; a foreign dylib carrying its own signature would fail verification, which is why the `--deep` re-sign matters. The base port has no Apple Developer ID and does no notarization, so a downloaded `Melee.app` needs right-click > Open on first launch (Gatekeeper); the fork inherits that. If the base port ever adopts a Developer ID, the same `--deep` pass carries Prism along.
4. **Speech log and switches**: unchanged. `MELEE_A11Y`, `MELEE_A11Y_LOG`, the `[a11y]` lines and the bounded-run check all work the same; the log file lands in the working directory, which for a launched `.app` is not the bundle. Set `MELEE_LOG_FILE` explicitly.

## Play-test on the Mac (by ear, VoiceOver)

Prism's macOS backends are Apple's own APIs (VoiceOver through the accessibility announcement path, and Apple's speech synthesizer as the fallback), which behave differently from NVDA's controller client. Verify, do not assume:

1. Launch with VoiceOver running: hear "Non-Visual Melee ready". Log shows which backend Prism chose; record the exact name, it goes into the primer.
2. Launch with VoiceOver off: hear the fallback voice; record that backend name too.
3. Interrupt semantics: does an interrupting announcement actually cut VoiceOver off mid-sentence, or does VoiceOver queue it anyway? Needs a consumer that can speak twice in a row, so this waits for menu narration or a temporary key.
4. Braille: if a braille display is at hand, does `output` reach it.
5. Focus: VoiceOver may read the game window's own accessibility tree (title, "window") on top of the announcements. Note anything it says that the player did not ask for.
6. Missing dylib: remove the Prism dylib from `Frameworks` and launch. Expected: the app fails to start with a dyld error, which Finder shows as a generic "cannot be opened" dialog. Confirm VoiceOver reads it; this is the macOS counterpart of the Windows "prism.dll was not found" dialog the spec relies on.

## Open questions, answered on the day

- Zip layout of `prism-macos-universal.zip` (header location, dylib name, whether it is a `.dylib` or a `.framework`).
- Does the base port's macOS CI job (GitHub Mac runners) mind one more configure-time download. It already fetches SDL and the rest from Homebrew.
- Minimum macOS of the friend's machine versus Prism's macOS 11 floor.

## Not planned

- Building Prism from source on macOS. Possible there (no MIDL, no delay loading), but the prebuilt zip keeps both platforms on one mechanism and one pin. ADR-0001 stands.
- Linux. Same mechanism would apply (Speech Dispatcher and Orca backends), but nobody is asking for it.
