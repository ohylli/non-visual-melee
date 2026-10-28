# Voices are rendered by Prism and played by the cue engine

Status: accepted (2026-10-28)

Match announcements are spoken by two voices of the fork's own beside the screen reader, so the player hears at once whether a line is about them or about someone else. Each voice is its own Prism OneCore instance, created once and kept for the whole run. A worker thread has it render each line to audio with `prism_backend_speak_to_memory`, evens out the line's loudness, and hands the samples to the cue engine, which plays them on the fork's own audio stream (ADR-0005). Prism never plays a voice's line itself. Rendering this way lets the fork decide how lines queue, overlap and replace each other, keeps synthesis off the game thread, and puts the voices under the port's master volume with the cues. The screen reader stays the fork's voice for everything else.

Two of the maintainer's other mods do the same: grimdark renders OneCore voices itself through WinRT and plays them through its own mixer, and wotr-access renders SAPI voices through its Loadstar library into its own mixer. Both found that sharing one backend instance between voices, or tearing one down when another speaks, cuts lines off; hence one instance per voice for the run.

## Considered options

- **Prism plays each line itself** (`prism_backend_speak` on a OneCore instance per voice): the least code. But Prism's OneCore backend has no queue (a new line replaces the one playing even without interrupt), it blocks the caller until the whole line is synthesized, and its playback goes to the system outside the port's volume, so neither the queueing rules nor the master volume could be honoured.
- **The fork's own WinRT code, as grimdark does**: full control of OneCore, but a second path to the speech engines beside Prism, C++/WinRT in a GCC build, and its own delay-load and apartment handling. Prism already wraps all of that.
- **Loadstar, as wotr-access does**: a streaming render with render-ahead and stable voice ids, but it offers no OneCore voices at the commit wotr pins, and it is a second native library to fetch and ship beside Prism.
- **SAPI voices**: Prism renders them the same way, so they can be added later as more choices on the same rows. OneCore came first because its voices are on every Windows 10 and 11 install and grimdark's defaults are OneCore voices.

## Consequences

- Rendering is synchronous and takes a few to some tens of milliseconds a line, so it runs on a worker thread; each Prism instance is used from that thread alone, as Prism's thread-safety rules ask. The game thread only queues text and reads back nothing but whether a line is still waiting.
- The cue engine gains a voice that plays a one-shot buffer handed to it at run time, beside its looped and pulsed voices whose sounds are fixed when they are added.
- The voices follow the master volume but not the cue volume or the Cues switch; they sit under the Speech switch.
- Prism identifies a voice only by display name and language, so the chosen voice is saved by its display name.
- Online nothing changes: the voices never touch the game's sounds, and like all speech they are silent while `pc_net_resim()` is true.
- The agent cannot hear the voices any more than the cues; the speech log names the voice of every announcement, and the cue engine's dump records their audio.
