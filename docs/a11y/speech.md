# Speech

## Mental model

An **announcement** is one piece of text plus a mode: **interrupt** (cut off whatever the screen reader is saying) or **queue** (say it after). A feature that has something to tell the player builds one announcement and hands it to **speech**, the fork's subsystem in `src/pc/a11y/speech.cpp`.

Speech writes the announcement to the **speech log** and passes it to the **screen reader bridge** (`screen_reader_bridge.cpp`). The bridge is the only fork file that knows about Prism, the screen reader library. Prism passes the text to whatever screen reader is running (NVDA, JAWS and others), which speaks it and shows it on a braille display. If none is running, Prism uses a Windows voice (OneCore or SAPI) instead.

```
feature --announce--> speech --log line--> melee-pc.log
                         |
                         +--> screen reader bridge --> Prism --> NVDA / JAWS / Windows voice
```

Beside the screen reader, speech has two **voices** of the fork's own (below, "Voices"): **your voice** for what happens to you and **the opponents' voice** for what happens to everyone else. A feature hands a voice a line with its **subject**, whom it is about. Speech logs it and passes it to the voices (`voices.cpp`), whose worker thread has the bridge render it with a Windows voice to samples, and the cue engine plays them (ADR-0008):

```
feature --announce_voice--> speech --log line--> melee-pc.log
                               |
                               +--> voices: queue --> worker --> bridge --> Prism (OneCore) --> samples
                                                                                               |
                                    cue engine's once voice <--- evened out, mono, 48 kHz <-----+
```

## Speech never filters

Speech says exactly what it is given, every time, and never drops a repeat. A feature that checks game state every frame keeps its own "last said" state. That keeps the speech log an exact record of what each hook asked for.

## The speech log

Every announcement, and every speech event worth knowing about (startup, the backend Prism picked, failures, shutdown), becomes one `[a11y]` line in the game's log. The log is how the fork is verified without hearing it: an agent's bounded run checks for the expected lines, and after a play-test the log is read against what the tester heard.

With speech off, by its switch or Accessibility's (below), announcements are still logged, marked `(off)`, and the speech log goes on recording. A run can check a feature's announcements without talking over the maintainer's screen reader: `MELEE_A11Y=0` turns Accessibility off for the run. Prism is started the first time speech is on, so a run that never turns it on never starts Prism.

A feature that takes keys for speech leaves them alone with speech off (the launcher's line reading is one), so a run that checks such a feature needs speech on without the screen reader: `MELEE_A11Y_SCREEN_READER=0` keeps accessibility on, never starts the screen reader, and logs each announcement marked as muted. The voices still play (below, "Voices").

Each voice's line is one line too, naming its voice, marked `(off)` as the screen reader's are; the voices add what became of it: when it plays, with its render time and its wait, and when it is cut off or dropped, and why. The `verify` skill has the lines.

The port's logger cuts each line at 512 bytes, so a very long announcement is truncated in the log. What gets spoken is not affected.

## The switches

Four switches are settings like any other, rows of the accessibility settings in `a11y.ini` (`docs/a11y/cues.md`, "Accessibility settings"), the first rows of the menus: Accessibility (`accessibility.on`), Speech (`speech.on`), Steering (`steering.on`) and Cues (`cues.on`, the cue settings' own). `src/pc/a11y/switches.cpp` works out what they leave on after every change to the settings, and the hooks hand it on. Every switch is live: no restart.

- **The hierarchy.** Accessibility is above Speech, Steering and Cues; Cues is above each cue's own switch. A higher switch off silences the ones below it and leaves their saved values as they are, so turning it back on restores what the player had.
- **What off does.** Speech off stops speech and leaves the cues and steering. Steering off ends a glide under way where the hand or cursor is, puts the stick back to rest at the next pad the hook publishes, and leaves the D-pad to the game and to quick chat. Cues off mutes the cue engine, which stays open and keeps its voices running underneath, so the cues come back where the match is. Accessibility off does all three: the readers and scene speech go on following the screens, so their state stays current, and only their speech goes unheard.
- **The last announcement.** Turning Speech or Accessibility off says "Speech off" or "Accessibility off" while speech is still on, then speech goes quiet. The screen reader got that text before anything else could be sent, and nothing sent after reaches it, so nothing cuts it off. Turning either back on says "Speech on" or "Accessibility on" first.
- **The way back into a page.** A reader learns that speech is back by watching `Speech::enabled()`: the RmlUi readers (`PageReader::follow_focus`) announce the focused row again, queued behind the switch's words, on their next frame, when the page already shows the new value; the main menu tree says its screen afresh on its next frame. The other native screens' readers say nothing until something changes.
- **Startup.** Speech is silent until the settings are read, so a player who saved Accessibility off hears nothing, not even the proof-of-life announcement, and Prism is never started. A problem found as the file was read is said after the proof-of-life.

### The environment overrides the file

`MELEE_A11Y` (Accessibility), `MELEE_A11Y_STEER` (Steering) and `MELEE_A11Y_CUES` (Cues), when set, hold their switch for the run whatever `a11y.ini` says: `0` off, anything else on, empty as if unset. The file keeps its own value: an override is never written, and a hand edit of that line waits under it. A change from a menu lifts the override and is saved as usual.

- `MELEE_A11Y=1` forces Accessibility on, the way back for a player who saved it off and starts the game silent; editing `a11y.ini` is the other.
- An override of Steering or Cues holds against Accessibility as well, so an agent run with Accessibility off (`MELEE_A11Y=0`) still steers with `MELEE_A11Y_STEER=1` and hears cues with `MELEE_A11Y_CUES=1`. Without an override they sit under Accessibility like any switch.
- The developer switches stay environment-only, never settings: `MELEE_A11Y_LOG`, `MELEE_A11Y_SCREEN_READER`, the cue dump, trace and previews, the voices' preview (`MELEE_A11Y_VOICE_PREVIEW`), the event pipe and the text dump.

## Threading rule

Speech runs on the game thread only, the thread that started it. Prism's backends are not thread-safe, and neither is the port's logger. Every feature planned so far runs on the game thread anyway. The voices' worker is the one exception inside speech: it owns the voices' Prism instances and never logs, and hands its results to the game thread (below, "Voices").

A call from any other thread is a bug: speech logs it and drops it. If a feature on another thread ever needs to speak (the netplay timer thread is the likely one), a queue inside speech can hand announcements to the game thread without changing any feature.

Prism's output call is expected to hand the text over and return without waiting for speech to finish, so it should not hold up a frame. That is an assumption from Prism's API, not a measurement. If a hitch is ever noticed, time the Windows voice fallback (SAPI) first.

## Voices

The voices speak match announcements, so the player hears from the first syllable whether a line is about them; everything else stays on the screen reader. ADR-0008 records why the fork renders them itself rather than letting Prism play them. Each is a Prism OneCore instance of its own, kept for the run and never shared, since the maintainer's other mods lost lines to sharing one or tearing one down while another spoke.

### Rendering and playing

- **One worker thread** (`Voices::work`) starts the first time speech is on, opens both instances, lists the installed voices and renders a test line, then renders each line as it is asked for: `prism_backend_speak_to_memory`, which returns once the whole line is synthesized. The game thread hands it text and never waits; each frame it takes back what was rendered.
- **Evening out.** OneCore's loudness varies from word to word (grimdark's finding), so every line is raised or lowered to one peak (`kVoicePeak`, a tuning value) after it is mixed down to mono and resampled to the cue engine's 48 kHz with miniaudio's resampler. A voice's volume is applied after that, by the cue engine, never by Prism's own volume, which the evening out would undo.
- **Playing.** Each voice has a once voice in the cue engine (`docs/a11y/cues.md`), which plays a buffer handed to it at run time, centred, with the engine's ramps. Its chain is the master volume × the voice's volume, under neither the cue volume nor any cue switch.
- **Rate** is Prism's 0 to 1, 0.5 the voice's normal speed, set on the instance before the next line is rendered.

### Queueing

Every line carries its subject: a player's slot in a match, or a preview (`kPreviewSubject`). Within one voice:

- a new line about the subject whose line is playing cuts that line off with the engine's stop ramp as soon as the new one is rendered, ahead of the lines waiting;
- a new line about a subject whose line is waiting takes its place in the queue;
- any other waits behind the lines already there.

So a queue never holds two lines about one subject, and a stale figure is never said after a fresh one. Each line is rendered when it is queued, so the next one is ready when its turn comes. The two voices never wait for or cut each other, nor the screen reader.

### Switches, the port menu and the developer preview

- **Under the Speech switch**, and Accessibility above it: off, a voice's line is logged `(off)` and never rendered. Turning either off lets the line playing finish and drops the lines waiting; so does the port menu opening. A line said while the menu is open plays: the menu is where a voice's preview is asked for. In a VS pause nothing changes.
- **`MELEE_A11Y_SCREEN_READER=0` leaves the voices playing**, unmarked (open question 5 of `.scratch/voices/spec.md`): they play through the cue engine, never the screen reader, so a driven run, whose audio goes to SDL's dummy driver, checks them through the speech log and the cue dump without a sound. That starts Prism on the worker for the voices alone.
- **`MELEE_A11Y_VOICE_PREVIEW=1`**, a developer switch, has your voice say "Your voice" as soon as the voices are ready and the opponents' voice "Opponents' voice" a pause after it ends, through speech and its log, so the voices can be checked before any feature speaks with them.
- **Online** nothing changes: the callers are behind the hooks' rollback gate like all speech, the voices' frame runs at the frame boundary outside the simulation, and the cue engine never touches the game's sounds.

### Choosing a voice

A voice is known by its display name ("Microsoft Zira"): Prism has no stable id, and its indexes may change between runs. `find_voice` takes a setting's text and gives the voice of that name, ignoring case, else the first whose name holds the text ("Zira"), else the Windows default voice. The worker and the game thread look in the same list, sorted by language and then name, as the menus show it.

- **The rows** (`voice_settings.cpp`) come right after the Speech switch, which registers them through its `after_speech` hand-off, so the switches stay the first rows: for each voice, your voice's first, a Voice row, a choice of the installed voices saved by display name (default "Zira" and "Mark"); a Rate slider, 50 the voice's normal speed (Prism's 0.5); a Volume slider; and Preview. A row shows a voice by its name, with its language after it ("Microsoft Mark, English (United States)", from the fork's table in `voice_names.cpp`) only when the installed voices span more than one language. Until the voices are ready the Voice rows show "Starting" ("Off" while speech has never been on), and with OneCore unavailable "Unavailable", and a step does nothing.
- **Every change takes effect at once**, from a row in either host or a hand edit of `a11y.ini`, through the settings' one listener. A change to a voice's Voice, Rate or Volume plays a sample in it, "Your voice" or "Opponents' voice", a moment after the change (`speech.voice_sample_delay`, a tuning value), so the screen reader's first words come through; each change restarts the wait, so a quick run of steps plays one sample, at the value it settles on. Preview plays the same at once. A sample is about the preview subject, so a new one cuts one still playing.
- **A voice not installed.** A saved voice no longer installed, or a default with no match, speaks with the Windows default voice; the file keeps the saved name, so reinstalling the voice brings it back, and the row shows the voice in use. The screen reader says so once, "Zira not installed, using Microsoft David." (both at once in one sentence), and again only if a hand edit names another missing voice.
- **OneCore unavailable** (an instance does not start, no voice is installed, or the test line fails): each voice line goes to the screen reader, queued, those waiting first in the order they were said; the queueing rules do not hold there. The screen reader says once "Voices unavailable, using the screen reader."
- **When that news is said.** The voices are ready a moment after speech first starts. In the launcher, whose opening queues behind it, the news is said at once; in the game the title screen's announcement interrupts, so the news waits for the first scene's announcement and queues behind it (`VoiceSettings::scene_announced`).
- **Voices of another language** (open question 6): OneCore lists the voices of every language installed, here a Finnish one beside the English ones, and renders English lines with them; whether to list them is for the play test to decide by ear.

### What Prism v0.18.2 does

Measured on the maintainer's machine (Windows 10, seven OneCore voices, among them a Finnish one), from a worker thread, answering the spec's open questions 1 to 3:

- **Rendering to memory** is as in v0.14.4: the OneCore backend synthesizes the whole line, then calls back once, on the calling thread, with float samples (mono, 16 kHz for every voice installed here), silence trimmed. Rate applies to the rendered line: the same long line lasted 2.5 s at 0.5, 1.5 s at 0.8 and 6.5 s at 0.2. v0.18.2 also sets OneCore's pitch, which v0.14.4 did not.
- **The apartment.** `prism_init` still initialises COM as a single-threaded apartment on the thread that calls it, and the OneCore backend moves its blocking WinRT calls to the thread pool when called from one. So the worker calls `prism_init` itself, a context apart from the screen reader's, and creates its instances there; nothing else is needed. Two instances also rendered at once from two threads.
- **Shutting down.** `prism_shutdown` on the worker, called while the game exits (from its atexit handler, where `pc_a11y_shutdown` runs), never returned once a OneCore instance had rendered. The worker frees its instances and leaves the context to the process's end (`PrismVoiceBridge::close`).
- **Render time.** Opening both instances took about a third of a second, on the worker. A short line ("45") renders in about 5 ms, the first in each voice in 15 to 30 ms; a long one (2.5 s of speech) in 15 to 45 ms. In the game, a line plays one or two frame boundaries after it was said, about 33 ms, plus the audio device's buffer. Rendering is fast enough that a cache of rendered lines is not worth having: each line is rendered as it is queued, which is all the rendering ahead it needs. The speech log gives each line's render time and its wait, for a play test to read.

## Prism is a prebuilt, required DLL

The fork's GCC toolchain cannot build Prism, so the build downloads a pinned release and links its DLL; [ADR-0001](../adr/0001-prebuilt-prism-dll.md) records why, and why a missing `prism.dll` stops the game instead of silently turning speech off. Bumping the release is described where the pin lives, at the top of `src/pc/a11y/a11y.cmake`.

## Other platforms

Porting speech to another platform should be a build change, not a code change: Prism publishes macOS and Linux builds with the same C API, and where the build has not set Prism up, the bridge compiles as a silent stub while the log keeps working. The worked plan for macOS is `.scratch/macos-port/spec.md`.
