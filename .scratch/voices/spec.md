# Voices: two voices of the fork's own beside the screen reader

Status: agreed 2026-10-28 (design conversation with the maintainer)

The maintainer agreed:

- Two voices, your voice and the opponents' voice, for match announcements; everything else stays on the screen reader.
- Rendering through Prism's OneCore backend and playing through the cue engine (ADR-0008).
- How lines in a voice queue, overlap and replace each other.
- The voices under the Speech switch and the port's master volume, not under the Cues switch or cue volume.
- The settings rows for each voice, a sample on every change, and what happens when a voice is missing.

The wording is a first proposal for the play test to judge.

## Goal

A blind player in a match hears what happens to them in one voice and what happens to everyone else in another, so whom a line is about is clear from the first syllable, while the screen reader keeps talking about everything else. The player can pick each voice from the voices installed in Windows and set its rate and volume, in the launcher's Settings and in the port menu.

This feature builds the voices and their settings. What they say is the next feature, `.scratch/match-announcements/spec.md`; until it lands the voices speak only their previews.

Vocabulary is `CONTEXT.md`: voice, your voice, the opponents' voice, announcement, speech, screen reader bridge, cue engine, accessibility settings, setting, tuning value. Prior art: grimdark (`C:\Users\ottoh\koodi\grimdark`, `src/voice.cpp`, `src/tts_onecore.cpp`) and wotr-access (`C:\Users\ottoh\koodi\wotr-access`, `src/Speech/`).

## Scope

In:

- The two voices, rendered by Prism and played by the cue engine.
- Their queueing rules, their place under the switches and the volume chain, and the speech log lines.
- Each voice's rows in the Accessibility category: Voice, Rate, Volume, Preview.
- A choice kind of value in the accessibility settings, saved as text, which the Voice rows need first and the match announcements' damage rows need next.
- The fallbacks: a saved voice no longer installed, the default voices missing, OneCore unavailable.

Out:

- What the voices say in a match: the next feature.
- **Pan**: both voices are centred. Panning the opponents' voice toward the opponent may come after a play test.
- **SAPI voices**: Prism renders them the same way, so they can join the Voice rows' choices later.
- **Pitch**: Prism's OneCore backend does not set it (it advertises pitch but returns "not implemented" in the source read, v0.14.4).
- **The screen reader as a choice for a voice**, as wotr-access offers.

## What the code gives us

### Prism

The fork pins Prism v0.18.2 (`src/pc/a11y/a11y.cmake`); the behaviour below was read from the v0.14.4 source in `C:\Users\ottoh\koodi\prism`, whose backend functions have the same signatures in the v0.18.2 header.

- **Instances.** `prism_registry_create(ctx, PRISM_BACKEND_ONE_CORE)` returns a new, uninitialised instance (then `prism_backend_initialize`); every call makes an independent one, with its own `SpeechSynthesizer`. A single instance is not thread-safe; different instances may be used from different threads.
- **Voices.** `prism_backend_count_voices`, `prism_backend_get_voice_name` (OneCore's `DisplayName`, such as "Microsoft Zira"), `prism_backend_get_voice_language` ("en-US"), `prism_backend_set_voice` by index. There is no stable id: indexes may change between runs, so a voice is known by its display name.
- **Rate and volume.** `prism_backend_set_rate` takes 0 to 1, 0.5 the voice's normal speed, mapped on a log scale to OneCore's 0.5 to 6 times. `prism_backend_set_volume` is baked into the rendered audio.
- **Rendering.** `prism_backend_speak_to_memory(b, text, callback, userdata)` synthesizes the whole line and returns; the callback receives interleaved float samples with their channel count and rate, once, on the calling thread (the docs allow chunks or another thread, so do not rely on it). Leading and trailing silence is trimmed. The format can change with the voice, so read it per line.
- **Playing it itself.** `prism_backend_speak` on OneCore blocks until synthesis ends and has no queue: a new line replaces the one playing even without interrupt. ADR-0008 records why the fork does not use it.
- **COM.** `prism_init` initialises COM on its thread as a single-threaded apartment; OneCore moves its blocking calls to a thread pool when called from such an apartment.

### The cue engine

`src/pc/a11y/cue_engine.hpp`: a fixed set of voices, each added once with its sound, looped or pulsed, its targets written by the game thread and read by the audio thread without locks. The volume chain is master volume × cue volume × the cue's own volume and boost × the voice's gain (primer `docs/a11y/cues.md`). Nothing yet plays a sound handed to it at run time.

### Speech

`src/pc/a11y/speech.cpp`: `Speech::announce(text, mode)` logs the announcement and passes it to the screen reader bridge (`screen_reader_bridge.hpp`), which is the only fork file that knows Prism. Game thread only.

### The accessibility settings

`src/pc/a11y/value_model.hpp`: two kinds of value, toggle and number, each a float, read and written by `value_file.cpp` as numbers in `a11y.ini`. The rows are built by `settings_page.cpp` into the base port's markup, and read by `rmlui_reader.cpp` in both hosts. A setting that is a list of named choices does not exist yet.

## Decisions

### The voices

- **Your voice** speaks what happens to you; **the opponents' voice** speaks what happens to everyone else, teammates included. Everything else, in a match too (the time left), stays on the screen reader.
- **Defaults.** Your voice is the first installed OneCore voice whose display name contains "Zira", the opponents' voice the first containing "Mark", as in grimdark. Either missing, it is the Windows default voice (the next section).
- **Each voice is its own Prism OneCore instance**, created once speech is first on and kept for the run. Neither is ever shared with the other or with the screen reader, nor torn down while the other speaks: wotr-access lost lines to that.

### Rendering and playing

- **A worker thread** owns both instances and renders each line with `prism_backend_speak_to_memory`. The game thread hands it text and never waits.
- **Each rendered line is evened out** to a fixed peak level (a tuning value), because OneCore's loudness varies from word to word (grimdark's finding). The Volume row is therefore applied after that, by the cue engine, not by Prism's volume, which the evening out would undo.
- **The samples are converted** to the cue engine's rate and mono on the worker, and handed to the cue engine whole.
- **The cue engine plays them** on a new kind of voice: one per speaking voice, which plays a buffer handed to it at run time, once, with the engine's start and stop ramps. Lines are centred.
- **A cache of rendered lines**, keyed by voice, rate and text as grimdark keeps, is allowed if rendering proves slow enough to hear; measure first.

### Queueing

- **The two voices are independent**: they may speak at once, and over the screen reader. Nothing in one voice waits for or cuts the other, or the screen reader.
- **Every line carries its subject**: whom it is about (a player's slot in a match, or a preview). Within one voice:
  - a new line about a subject that has a line playing cuts that line off, with the engine's stop ramp, and plays at once;
  - a new line about a subject that has a line waiting takes that line's place in the queue;
  - otherwise it queues behind the lines already there.
- So the queue never holds two lines about one subject, and a stale figure is never said after a fresh one.

### Switches and volume

- **Under the Speech switch**, and Accessibility above it. With either off, voice announcements are logged, marked `(off)`, as screen reader announcements are.
- **Not under the Cues switch, cue volume or any cue's switch.** The chain for a voice is master volume × the voice's Volume.
- **When the voices stop taking new lines.** When Speech or Accessibility is turned off, or the port menu opens, the line playing finishes and the lines waiting behind it are dropped. In a VS pause nothing changes: the queue plays out.
- **Online**: speech is already silent while `pc_net_resim()` is true, so a re-run frame never repeats a line. The voices never touch the game's sounds, so a base port peer is unaffected.

### Choosing a voice

- **The voice is saved by its display name** in `a11y.ini`, so it survives indexes changing.
- **The Voice row is a choice row**: Left and Right step through the installed OneCore voices, sorted by language and then name, as the other rows step. The screen reader reads "Voice, Microsoft Mark". The language follows the name ("Microsoft Mark, English (United States)") only when the installed voices span more than one language.
- **A sample on every change.** A step on a voice's Voice, Rate or Volume row plays a sample in that voice, at the new value, a moment after the step (a tuning value) so the screen reader's first words come through: "Your voice" or "Opponents' voice". A quick run of steps plays one sample, for the value the run settles on.
- **Preview** is a button that plays the same sample at once.

### The rows

After the Speech switch in the Accessibility category, each voice's four rows, your voice's first:

- **Your voice**: the voice to speak what happens to you.
- **Your voice rate**: a slider, the voice's normal speed by default.
- **Your voice volume**: a slider, full by default.
- **Preview your voice**: a button.

The opponents' voice has the same four, named "Opponents' voice", "Opponents' voice rate" and so on. The descriptions are the implementer's first proposal.

### A choice kind of value

- **A third kind**, beside toggle and number: one of a list of named options. The file saves the option's text, not its index, so a list that changes (voices installed or removed) never moves a saved choice to another option.
- **Options from code or from the run.** The match announcements' damage rows have fixed options; the Voice rows' options are the voices Prism lists, known only once the voices' instances exist.
- **A saved option not in the list** stays in the file, unchanged, until the player picks another; the value in use falls back as the setting says (for a voice, the next section).
- **Both hosts read it** as they read a slider: label and option.

### Missing voices

- **A saved voice no longer installed**, or a default with no match: the voice is the Windows default voice. Once per run, after the proof-of-life announcement, the screen reader says so: "Zira not installed, using Microsoft David." The file keeps the saved name, so reinstalling the voice brings it back; the Voice row shows the voice in use.
- **Both voices on the same voice** (both missing, or chosen so) is allowed; the play test may ask for something better.
- **OneCore unavailable** (the instance fails to start, or a render fails at startup): voice lines go to the screen reader, queued, and the screen reader says once: "Voices unavailable, using the screen reader." The queueing rules above do not hold there.

### How the fork attaches

- **No new hooks.** The voices are inside speech; the rows are built by `settings_page.cpp` like the others; the cue engine's new kind of voice is fork code.
- **The screen reader bridge** stays the only file that touches Prism. It gains the voices' instances: listing voices, setting voice and rate, and rendering a line to samples. Its fake in the unit tests renders known samples.
- **Speech** gains an announce call for a voice, with the line's subject, and owns the worker thread. The game-thread-only rule holds for callers; the worker calls only the bridge's voice functions, each instance from the worker alone.

### The speech log

Every voice announcement is one `[a11y]` line naming its voice, as every screen reader announcement is, with the same `(off)` and muted marks. A line cut off or dropped from a queue is logged too, so a play test's log shows what the player could have heard.

### Tests

- **A unit test of the voices** (fake bridge and clock): the queueing rules (cut a playing line about the same subject, replace a waiting one, queue a different subject), the two voices independent, the switches and the port menu dropping the queue and letting the playing line finish, the evening out, a missing voice and the fallback announcement, OneCore unavailable.
- **The cue engine's test** covers the new kind of voice: a buffer played once, ramps at both ends, cut off with a ramp, master volume and Volume applied and cue volume not.
- **The accessibility settings' test** covers the choice kind: saved as text, a saved option not in the list kept, a list that changes, the rows in both hosts.
- **A drive run** opens the port menu, steps the Voice rows and presses Preview, checking the readers' lines and the voice lines in the log.
- **The cue dump** (`MELEE_A11Y_CUE_DUMP`) records the voices' audio, so a run can check that a line was rendered and played.
- **The play test**, by ear: choosing voices, rate and volume, the samples, the fallbacks.

### Docs

- `docs/a11y/speech.md`: a "Voices" section (the two voices, rendering and playing, queueing, switches, missing voices); its mental model extended.
- `docs/a11y/cues.md`: the new kind of voice and the voices' place in the volume chain.
- `docs/a11y/rmlui-screens.md`: the choice row.
- `CLAUDE.md`: one "Accessibility status" line.
- The `verify` skill: the drive run and its expected lines.

## Open questions

For the implementer to find out:

1. **Prism v0.18.2 itself.** Whether its OneCore backend still renders to memory as v0.14.4 does (synchronously, one callback, silence trimmed), whether rate applies to rendered lines, and whether the threading of OneCore and SAPI changed (a 2026-01-27 Prism commit prepared a move to a multi-threaded apartment).
2. **COM on the worker.** Which apartment the worker must initialise before it creates the instances, and whether creating them on the worker instead of the thread that called `prism_init` matters.
3. **Render time.** How long a short line ("45") and a long one take to render on the maintainer's machine, from the request to the first sample playing, and whether a cache or rendering ahead is worth having.
4. **The choice row's markup.** Which of the base port's row markups a choice row uses (the base port's own choice rows step with Left and Right), and whether the readers need anything new to read it.
5. **The screen reader switch.** Whether `MELEE_A11Y_SCREEN_READER=0` should also keep the voices from rendering (logging them as muted), or let them render into the cue engine so a driven run, whose audio goes to SDL's dummy driver, can check them through the cue dump. The second needs Prism started for the voices alone.
6. **Voices of another language.** Whether OneCore lists voices of every installed language, and how a non-English voice reads English lines; the play test decides whether to filter.

## Issues

1. `issues/01-choice-values.md`: the choice kind of value in the accessibility settings: model, file, rows in both hosts, readers.
2. `issues/02-voice-engine.md`: the bridge's voice functions, the worker, the cue engine's new voice, queueing, evening out, switches, the speech log.
3. `issues/03-voice-rows.md`: Voice, Rate, Volume and Preview, samples on change, the missing-voice and OneCore fallbacks. Blocked by 01 and 02.
4. `issues/04-drive-scripts-and-docs.md`: the drive scripts, the `verify` skill, the docs and the status line. Blocked by 03.
5. `issues/05-play-test.md`: the maintainer, by ear. Blocked by 04.
