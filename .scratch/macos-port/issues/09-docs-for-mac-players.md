# 09 Docs for Mac players

Status: ready-for-agent
Type: task
Blocked by: 08

Spec: `.scratch/macos-port/spec.md`. Read issue 08's recorded answers first.

## What to build

- **`ACCESSIBILITY.md`**: macOS among the requirements, with its version floor; where to download the Mac build and how to open it the first time with VoiceOver; the voices on a Mac (System Settings > Accessibility > Spoken Content for more voices) and the defaults; anything the play test found that a Mac player needs to know (ducking, a permission dialog, interrupting). Leave out what the play test found not to work, or say plainly that it does not.
- **CLAUDE.md, "Accessibility status"**: the Speech and Voices lines name macOS where it was heard working, and a line for the port itself (the issues of this directory, what still waits). "Build and run (Windows)" says Windows is the agent's platform and the Mac build comes from CI.
- **`docs/a11y/speech.md`**: the backend chosen on a Mac, the names recorded in issue 08, the render time and the main-thread hop, as its Windows measurements are written.
- **The spec's status** and its open questions answered.

## Done when

The maintainer has read `ACCESSIBILITY.md`'s Mac parts by heading with a screen reader and is content with them.
