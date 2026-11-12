# 09 Docs for Mac players

Status: ready-for-agent
Type: task
Blocked by: 08

Spec: `.scratch/macos-port/spec.md`. Read issue 08's recorded answers first.

## What to build

- **`ACCESSIBILITY.md`**: macOS among the requirements, with its version floor; where to download the Mac build and how to open it the first time with VoiceOver; the voices on a Mac (System Settings > Accessibility > Spoken Content for more voices) and the defaults; anything the play test found that a Mac player needs to know (ducking, a permission dialog, interrupting). Leave out what the play test found not to work, or say plainly that it does not.
- **CLAUDE.md, "Accessibility status"**: the Speech and Voices lines name macOS where it was heard working, and a line for the port itself (the issues of this directory, what still waits). "Build and run (Windows)" says Windows is the agent's platform and the Mac build comes from CI.
- **`docs/a11y/speech.md`**: the backend chosen on a Mac, the names recorded in issue 08, the render time and the main-thread hop, as its Windows measurements are written.
- **The Mac's default voices**, moved here from issue 07: the AVSpeech row of `kVoiceBackends` (`screen_reader_bridge.cpp`) gets the two voices issue 08's check 9 picked, each by name and language (its `VoiceDefault`), in place of the empty defaults, and its comment says so; `docs/a11y/speech.md` ("The voice backend", "Choosing a voice") names them beside Windows' Zira and Mark. A name that may not be installed on every Mac falls back to the default voice as on Windows; prefer voices every macOS install has.
- **The spec's status** and its open questions answered.

## Done when

The maintainer has read `ACCESSIBILITY.md`'s Mac parts by heading with a screen reader and is content with them.

The Mac's default voices are set, the voices' unit tests and the quick level pass on Windows, and the fork's CI builds the Mac packages with them.
