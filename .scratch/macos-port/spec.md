# macOS port

Status: agreed 2026-11-12 (conversation with the maintainer). The Windows half is ready to start; the Mac half needs a friend of the maintainer, who has a Mac with VoiceOver, for its checks and play test. First written 2026-09-23 for screen reader speech alone, before the voices existed; rewritten for the voices and split into issues.

The maintainer agreed:

- Everything that can be built and checked on Windows is done there first, so the Mac half is as small as it can be: the code only a Mac can run, and what only a Mac can tell.
- The voices on a Mac are Apple's system voices through Prism's AVSpeech backend, the same design as OneCore on Windows (ADR-0008).
- The chosen voice is saved by name and language. No code reads files written before that: the only one is the maintainer's own `a11y.ini`, edited by hand once.
- A CI job on GitHub's Mac runners builds and packages the Mac build, so the friend downloads it and never builds.

## Goal

A blind player on a Mac gets what a Windows player gets: the native menus, port menu and launcher spoken through VoiceOver (or Apple's voice when VoiceOver is off), the cues, steering, and the match announcements in two voices of their own choosing.

Vocabulary is `CONTEXT.md`. Background: `docs/a11y/speech.md` (the screen reader bridge and the voices), ADR-0001 (Prism prebuilt), ADR-0008 (voices rendered by Prism, played by the cue engine), the base port's `docs/building.md`, "macOS".

## Where things stand

- **The base port** builds macOS: Apple Silicon tested, Intel built by its CI but untested. Apple's clang builds the C++ and the link; the decomp's C (`melee_game`) goes through Homebrew GCC by `tools/gcc_launcher.py`, because only GCC implements `scalar_storage_order`, which reads the disc's big-endian structs. `tools/package_macos.sh` makes `Melee.app`, copies every non-system dylib into `Contents/Frameworks` with `@rpath` load paths, and ad-hoc signs the bundle with `codesign --force --deep --sign -`, which seals a foreign dylib like Prism's too. No Developer ID, no notarization: a downloaded app needs right-click > Open (VoiceOver: VO-Shift-M on it, then Open) or `xattr -d com.apple.quarantine Melee.app`. The base port's own Mac CI job is in `.github/workflows/build.yml`, disabled in the fork.
- **The fork's code** is mostly portable: the cue engine (SDL and miniaudio), the readers, the settings, steering. The screen reader bridge (`screen_reader_bridge.cpp`) has no platform code and is gated on `A11Y_HAVE_PRISM`, which `a11y.cmake` defines only for Windows today; without it speech still logs, silently.
- **The connection checker** (`melee-netcheck`) compiles and works on a Mac: the maintainer's friend has confirmed it.
- **The drive scripts** are Windows-only. On a Mac the checks are the friend's ears and the speech log.

## The voices on a Mac

Read in Prism v0.18.2's source, `source/backends/avspeech.cpp`; not yet run.

- **Everything the voices use is there**: rendering a line to memory (`writeUtterance:toBufferCallback:`, macOS 10.15 and later, below Prism's own macOS 11 floor), listing voices with name and language, choosing one, and the rate (Prism's 0 to 1 maps onto Apple's slowest to fastest, 0.5 Apple's default). Each instance keeps its own synthesizer, so one instance per voice holds as on Windows.
- **The voices are those in System Settings > Accessibility > Spoken Content**, the downloadable Enhanced and Premium voices among them. Apple keeps the Siri voices from other apps.
- **Every call runs on the main thread.** The backend sends each piece of work to the main thread with `dispatch_sync` and waits. The voices' worker therefore waits until the game's main thread next runs its event loop, which SDL does once a frame: a line starts up to a frame or two later, and waits through any long stall of the main thread, such as a scene loading.
- **So the main thread must never wait for the worker.** At quit it does today: `Voices::~Voices` joins the worker, and the worker, freeing its instances (or still rendering a line), waits for the main thread. Both wait forever. Issues 03 and 07 fix this.
- **Names repeat.** On a Mac a voice's name is not unique: some come in several languages under one name (Eddy, among Apple's Eloquence voices), and the list is sorted by language, so a saved "Eddy" could find the German one. Hence name and language, issue 02. Prism gives no stable id.
- **Personal Voice.** On macOS 14 and later, starting an instance asks for Personal Voice permission and waits for the answer. Whether that shows a dialog, which would take focus, is one of the play test's questions.

## A fork bug only a Mac would hit

`game_access.c` reads the stage's line count (`mpLib_8004D164()->line_count`, a field of `MapCollData`, a disc struct). It is built with the `melee` target, which on a Mac is Apple's clang, where `DISC_STRUCT` is empty: the count would read byte-swapped, the floor tests behind the gap and ledge warnings would loop over the wrong number of lines and could read past the end. It must go through GCC on a Mac as the decomp does, issue 04.

## The split

Windows half, built and verified here:

1. `01-voice-backend-per-platform.md`: the voice backend and its defaults chosen per platform by `a11y.cmake`, OneCore on Windows; platform-neutral wording.
2. `02-voice-saved-with-language.md`: a voice saved and matched by name and language.
3. `03-voices-close-without-blocking.md`: at quit, the main thread gives the bridge a turn while it waits for the worker, a no-op on Windows; a unit test with a fake bridge that needs those turns.
4. `04-game-access-through-gcc.md`: `game_access.c` compiled by GCC on Macs.
5. `05-prism-on-macos.md`: the Mac branch of `a11y.cmake`, Prism's dylib linked and found, the AVSpeech backend named.
6. `06-macos-ci.md`: the fork's CI builds and packages the Mac build, with `melee-netcheck` and Prism's licences, beside the Windows one.

Mac half, written here and compiled by 06's CI, checked on the friend's Mac:

7. `07-avspeech-voices.md`: the bridge's turn on a Mac, running the main thread's queue; the Mac default voices.
8. `08-play-test-mac.md`: the play test with VoiceOver and with it off.
9. `09-docs-for-mac-players.md`: `ACCESSIBILITY.md` and the status lines, once the play test has said what works.

## Out

- **Building Prism from source** on a Mac. ADR-0001 stands: one prebuilt release, one pin.
- **Linux.** The same mechanism would serve it (Speech Dispatcher and Orca backends), but nobody is asking.
- **Drive scripts on a Mac.** The named pipes and the window handling are Windows-only, and the agent cannot reach a Mac anyway.
- **Intel Macs as a supported target.** CI may build them if it costs nothing; nobody tests them.
- **Online between Mac and Windows** beyond what the base port promises: the fork leaves the simulation alone on every platform, so it adds nothing to check. The play test may try it if two machines are at hand.
- **Reading old `a11y.ini` voice lines.** See above.

## Open questions, answered on the day

- `prism-macos-universal.zip`'s layout (issue 05): header location, dylib name, `.dylib` or `.framework`. Answered 2026-11-12: as the Windows zip's, `include/prism.h`, `dynamic/release/lib/libprism.dylib` (a plain `.dylib`, universal arm64 and x86_64, install name `@rpath/libprism.dylib`, macOS 11 and later, system frameworks only), `LICENSES/` and `NOTICE` at the top; the issue's comment has the rest.
- The friend's macOS version against the zip's floor: macOS 15 on Apple Silicon, 14 on Intel, set by the Homebrew dylibs the app bundles (issue 06's comment); Prism's own is 11.
- How long a line takes to render on a Mac, main-thread hops included (issue 08).
- Whether VoiceOver's audio ducking lowers the game's sound, voices and cues with it, while it speaks (issue 08).
