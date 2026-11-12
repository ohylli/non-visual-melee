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

The two voices and the screen reader are independent: either voice may speak over the other and over the screen reader, and nothing in one waits for or cuts another. Within one voice a line about the same subject as one playing or waiting replaces it, so a stale figure is never said after a fresh one. The voices sit under the Speech switch, not the Cues switch, and their loudness is the master volume × the voice's own Volume.

The player picks each voice from the voices installed in Windows, with its rate and volume, in the Accessibility category of the launcher's Settings and the port menu; each change plays a sample in that voice. A voice not installed falls back to the Windows default voice, and with no voice to be had at all the voices' lines go to the screen reader; either way the screen reader says so once.

## Speech never filters

Speech says exactly what it is given, every time, and never drops a repeat. A feature that checks game state every frame keeps its own "last said" state. That keeps the speech log an exact record of what each hook asked for.

## The speech log

Every announcement, and every speech event worth knowing about (startup, the backend Prism picked, failures, shutdown), becomes one `[a11y]` line in the game's log. The log is how the fork is verified without hearing it: an agent's bounded run checks for the expected lines, and after a play-test the log is read against what the tester heard.

With speech off, by its switch or Accessibility's (below), announcements are still logged, marked `(off)`, and the speech log goes on recording. A run can check a feature's announcements without talking over the maintainer's screen reader: `MELEE_A11Y=0` turns Accessibility off for the run. Prism is started the first time speech is on, so a run that never turns it on never starts Prism.

A feature that takes keys for speech leaves them alone with speech off (the launcher's line reading is one), so a run that checks such a feature needs speech on without the screen reader: `MELEE_A11Y_SCREEN_READER=0` keeps accessibility on, never starts the screen reader, and logs each announcement marked as muted. The voices still play (below, "Voices").

Each voice's line is one line too, naming its voice, marked `(off)` as the screen reader's are; the voices add what became of it: when it plays, with its render time and its wait, and when it is cut off or dropped, and why. The `verify` skill has the lines.

The port's logger cuts each line at 512 bytes, so a very long announcement is truncated in the log. What gets spoken is not affected.

## The switches

Four switches are settings like any other, rows of the accessibility settings in `a11y.ini` (`docs/a11y/cues.md`, "Accessibility settings"), on the Accessibility category's top level: Accessibility (`accessibility.on`), Speech (`speech.on`), Steering (`steering.on`) and Cues (`cues.on`, the cue settings' own). `src/pc/a11y/switches.cpp` works out what they leave on after every change to the settings, and the hooks hand it on. Every switch is live: no restart.

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

The voices speak the match announcements about players, so the player hears from the first syllable whether a line is about them; everything else stays on the screen reader. ADR-0008 records why the fork renders them itself rather than letting Prism play them. Each is a Prism instance of its own, kept for the run and never shared, since the maintainer's other mods lost lines to sharing one or tearing one down while another spoke.

### The voice backend

The Prism backend the voices speak with is chosen per platform: OneCore on Windows, AVSpeech on macOS (`.scratch/macos-port/`). Each platform's branch in `src/pc/a11y/a11y.cmake` names its backend by Prism id (`A11Y_VOICE_BACKEND`), beside its Prism asset. What a backend brings is its row in `kVoiceBackends`, in `screen_reader_bridge.cpp`: the id, its name in the log (`voices: OneCore, ...`) and each voice's default voice. `voice_backend()` gives the build's row, which the voices and their rows take at startup, before the voices open; a build with Prism but no backend named has no voices, as a build without Prism. Adding a backend is a row there and a line in its platform's branch, and the build stops if a branch names a backend the table lacks. The table is compiled on every platform, so its rows must use only ids Prism's header defines everywhere, as it does every backend's. A row's empty default is the backend's own default voice: AVSpeech's two are empty, so both voices speak with the Mac's default voice until two are chosen by ear (`.scratch/macos-port/`, issues 08 and 09); the file then holds an empty value, which reads back as that default. AVSpeech's default, in Prism v0.18.2's source, is the first voice Apple lists whose language is the system's, not the voice chosen in System Settings' Spoken Content, so which voice it is shows only on the Mac.

### What speaks through them

The match announcements (`match_announcer.cpp`, read from the tick hook; spec `.scratch/match-announcements/spec.md`): your damage and stocks in your voice, everyone else's in the opponents' voice, each line's subject the player's slot, so a new line about a player replaces an older one about the same player. The time left, also a match announcement, goes to the screen reader, queued. What the announcements read, and when the HUD shows it: `docs/a11y/gameplay-state.md`, "The HUD".

### Rendering and playing

- **One worker thread** (`Voices::work`) starts the first time speech is on, opens both instances, lists the installed voices and renders a test line, then renders each line as it is asked for: `prism_backend_speak_to_memory`, which returns once the whole line is synthesized. The game thread hands it text and never waits; each frame it takes back what was rendered.
- **Evening out.** OneCore's loudness varies from word to word (found in the maintainer's grimdark mod), so every line is raised or lowered to one peak (`kVoicePeak`, set by ear) after it is mixed down to mono and resampled to the cue engine's 48 kHz with miniaudio's resampler. A voice's volume and boost are applied after that, by the cue engine, never by Prism's own volume, which the evening out would undo.
- **Playing.** Each voice has a once voice in the cue engine (`docs/a11y/cues.md`), which plays a buffer handed to it at run time, centred, with the engine's ramps, in a volume chain of its own (`docs/a11y/cues.md`, "The once voice").
- **Rate** is Prism's 0 to 1, 0.5 the voice's normal speed, set on the instance before the next line is rendered.

### Queueing

Every line carries its subject: a player's slot in a match, or a preview (`kPreviewSubject`). Within one voice:

- a new line about the subject whose line is playing cuts that line off with the engine's stop ramp as soon as the new one is rendered, ahead of the lines waiting;
- a new line about a subject whose line is waiting takes its place in the queue;
- any other waits behind the lines already there.

So a queue never holds two lines about one subject. Each line is rendered when it is queued, so the next one is ready when its turn comes; a line replaced or dropped before the worker reaches it is never rendered.

### Switches, the port menu and the developer preview

- **Under the Speech switch**, and Accessibility above it: off, a voice's line is logged `(off)` and never rendered. Turning either off lets the line playing finish and drops the lines waiting; so does the port menu opening. A line said while the menu is open plays: the menu is where a voice's preview is asked for. In a VS pause nothing changes.
- **`MELEE_A11Y_SCREEN_READER=0` leaves the voices playing**, unmarked: they play through the cue engine, never the screen reader, so a driven run, whose audio goes to SDL's dummy driver, checks them through the speech log and the cue dump without a sound. That starts Prism on the worker for the voices alone.
- **`MELEE_A11Y_VOICE_PREVIEW=1`**, a developer switch, has your voice say "Your voice" as soon as the voices are ready and the opponents' voice "Opponents' voice" a pause after it ends, through speech and its log, so the voices can be checked before any feature speaks with them.
- **Online** nothing changes: the callers are behind the hooks' rollback gate like all speech, the voices' frame runs at the frame boundary outside the simulation, and the cue engine never touches the game's sounds.

### Choosing a voice

A voice is known by its display name and its language together: Prism has no stable id, its indexes may change between runs, and a name alone is not unique on every platform (on a Mac some voices come in several languages under one name, and since the list is sorted by language a saved "Eddy" could find the German one).

- **The saved form** (`voice_text` and `parse_voice_text`, `voices.cpp`) is the display name with the language tag after it in brackets, `Microsoft Zira (en-US)`: readable, and editable by hand. It is read from its end: a language tag in brackets there (two or three letters, then any parts of letters and digits after a hyphen or an underscore, as `en-US`, `fi` or `zh-Hans-CN`) is the language, and the rest the name, so a name with brackets of its own keeps them (`Eddy (English (UK)) (en-GB)`). Text with no tag at its end is a name, or part of one, of any language: a hand edit of `Zira` still works, and `Ava (Enhanced)` is a name, since `Enhanced` is no tag. Such a name written without its tag, ending in a bracketed two or three letters, would be read as name and language; with its tag, as the rows save it, it reads right.
- **Matching.** `find_voice` takes a setting's text and gives, within its language (any, for text without one, compared ignoring case), the voice of that name, ignoring case, else the first whose name holds the name given ("Zira"), else the backend's default voice. A language not installed, or no voice of the name in it, is the default voice. The worker and the game thread look in the same list, sorted by language and then name, as the menus show it.
- **The defaults** are part of a name in a language, from the backend's row (`Zira` and `Mark`, both `en-US`, on Windows), saved as `Zira (en-US)`. An empty default is the backend's own default voice, whatever its language, saved empty.
- **The rows** (`voice_settings.cpp`) are in Voice settings (`docs/a11y/cues.md`, "Where a row goes"). For each voice, your voice's first, a Voice row, a choice of the installed voices saved in the saved form above; a Rate slider whose middle is the voice's normal speed (Prism's 0.5); a Volume slider; and Preview. The rows are choice rows and sliders of the base port's own markup, read by the shared RmlUi reader (`docs/a11y/rmlui-screens.md`, "The choice row"). A row shows a voice by its name, with its language after it ("Microsoft Mark, English (United States)", from the fork's table in `voice_names.cpp`) only when the installed voices span more than one language; the tag in the saved form is never shown. Until the voices are ready the Voice rows show "Starting" ("Off" while speech has never been on), and with the backend unavailable "Unavailable", and a step does nothing.
- **Every change takes effect at once**, from a row in either host or a hand edit of `a11y.ini`, through the settings' one listener. A change to a voice's Voice, Rate or Volume plays a sample in it, "Your voice" or "Opponents' voice", a moment after the change (`speech.voice_sample_delay`, a tuning value), so the screen reader's first words come through; each change restarts the wait, so a quick run of steps plays one sample, at the value it settles on. Preview plays the same at once. A sample is about the preview subject, so a new one cuts one still playing.
- **A voice not installed.** A saved voice no longer installed, or a default with no match, speaks with the backend's default voice; the file keeps the saved text, so reinstalling the voice brings it back, and the row shows the voice in use. The screen reader says so once, "Voice Zira not installed, using Microsoft David." (both at once in one sentence), and again only if a hand edit names another missing voice. Each voice is said as a row would show it, the missing one with its language when the installed voices, counting it, span more than one: "Voice Eddy, French (France) not installed, using Microsoft David, English (United States)."
- **Voices unavailable** (an instance of the backend does not start, no voice is installed, the test line fails, or the cue engine has no audio output): each voice line goes to the screen reader, queued, those waiting first in the order they were said; the queueing rules do not hold there. The screen reader says once "Voices unavailable, using the screen reader."
- **When that news is said.** The voices are ready a moment after speech first starts. In the launcher, whose opening queues behind it, the news is said at once; in the game the title screen's announcement interrupts, so the news waits for the first scene's announcement and queues behind it (`VoiceSettings::scene_announced`).
- **Voices of another language.** OneCore lists the voices of every language installed, here a Finnish one beside the English ones, and renders English lines with them.

### What Prism v0.18.2 does

Measured on the maintainer's machine (Windows 10, seven OneCore voices, among them a Finnish one), from a worker thread:

- **Rendering to memory** is as in v0.14.4: the OneCore backend synthesizes the whole line, then calls back once, on the calling thread, with float samples (mono, 16 kHz for every voice installed here), silence trimmed. Rate applies to the rendered line: the same long line lasted 2.5 s at 0.5, 1.5 s at 0.8 and 6.5 s at 0.2. v0.18.2 also sets OneCore's pitch, which v0.14.4 did not.
- **The apartment.** `prism_init` still initialises COM as a single-threaded apartment on the thread that calls it, and the OneCore backend moves its blocking WinRT calls to the thread pool when called from one. So the worker calls `prism_init` itself, a context apart from the screen reader's, and creates its instances there; nothing else is needed. Two instances also rendered at once from two threads.
- **Shutting down.** `prism_shutdown` on the worker, called while the game exits (from its atexit handler, where `pc_a11y_shutdown` runs), never returned once a OneCore instance had rendered. The worker frees its instances and leaves the context to the process's end (`PrismVoiceBridge::close`). Quit waits in turns: the game thread (the main thread) waits for the worker to free its instances, and for any line still rendering to finish; the lines waiting are never rendered. It never waits with a bare `join`: on a Mac, Prism's AVSpeech backend runs each of its calls on the main thread and the calling thread waits for it, so a main thread blocked in `join` and a worker waiting for the main thread would wait forever. Instead it waits a short step at a time, giving the bridge a turn before each step (`VoiceBridge::turn`, through `Voices::wait_in_turns`), in which a backend runs what the worker left for the main thread; it joins once the worker has said it is finished, since `std::thread` has no timed join. The turn touches no instance, as the worker is using them. OneCore's calls never wait for the main thread, so on Windows the turn does nothing; a backend's turn is a function in its row of `kVoiceBackends`. Any other wait of the game thread for the worker goes the same way: today only the unit tests' `wait_for_worker`. During play nothing more is needed, since SDL runs the main thread's event loop once a frame. A Mac's turn is below, "The main thread's turn on a Mac".
- **Render time.** Opening both instances took about a third of a second, on the worker. A short line ("45") renders in about 5 ms, the first in each voice in 15 to 30 ms; a long one (2.5 s of speech) in 15 to 45 ms. In the game, a line plays one or two frame boundaries after it was said, about 33 ms, plus the audio device's buffer. Rendering is fast enough that a cache of rendered lines is not worth having: each line is rendered as it is queued, which is all the rendering ahead it needs. The speech log gives each line's render time and its wait, for a play test to read.

## Prism is a prebuilt, required DLL

The fork's GCC toolchain cannot build Prism, so the build downloads a pinned release and links its DLL; [ADR-0001](../adr/0001-prebuilt-prism-dll.md) records why, and why a missing `prism.dll` stops the game instead of silently turning speech off. Bumping the release is described where the pin lives, at the top of `src/pc/a11y/a11y.cmake`: one tag, and the sha256 of each platform's zip for it.

## Other platforms

Porting speech to another platform should be a build change, not a code change: Prism publishes macOS and Linux builds with the same C API, and where the build has not set Prism up, the bridge compiles as a silent stub while the log keeps working. The worked plan for macOS is `.scratch/macos-port/spec.md`.

### macOS

`a11y.cmake`'s Mac branch links `libprism.dylib` from Prism's one universal zip (arm64 and x86_64, macOS 11 and later). Its install name is `@rpath/libprism.dylib` and it needs only the system's frameworks, so the build copies it beside `melee` and gives `melee` the rpath `@executable_path`, and the base port's `tools/package_macos.sh`, which copies every non-system dylib into `Melee.app`'s `Contents/Frameworks` and signs the bundle, takes it with the others unchanged. A missing dylib stops `melee` with dyld's "Library not loaded", as a missing DLL stops it on Windows. The voices speak through AVSpeech, whose calls wait for the main thread (above, "What Prism v0.18.2 does").

#### The main thread's turn on a Mac

Read in Prism v0.18.2's `source/backends/avspeech.cpp`; written before any run on a Mac, so the play test (`.scratch/macos-port/issues/08-play-test-mac.md`) is its check.

- **Where the voices wait.** AVSpeech sends the work of opening an instance, listing the voices, rendering and freeing to the main thread's dispatch queue (`dispatch_sync` to the main queue, its `sync_on_main`) and waits for it. A render then waits for the synthesizer's buffers, which may come back through the main thread too. Setting a voice or a rate waits for nothing. On the main thread itself the work runs at once, so the screen reader's instance, on the game thread, never waits.
- **Who runs that queue.** The main queue runs whenever the main thread's run loop does. In play, SDL's event pump runs it once a frame, so each hop of a line adds up to a frame, and a line waits through any stall of the main thread, such as a scene loading. The voices open on the worker the same way while the game goes on.
- **At quit** the game thread waits for the worker in turns (above, "Shutting down"), and AVSpeech's turn, `run_main_queue` in `main_queue_mac.cpp`, runs the run loop once without waiting: `CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true)`, CoreFoundation's C API, so no Objective-C. A time of 0 runs what is already queued and returns at once; the game thread's own short wait between turns is the pause. Freeing each instance stops its synthesizers through one hop, so quitting takes a step or two of the turn's wait, more while a line still renders. The file is built and CoreFoundation linked by `a11y.cmake`'s Mac branch alone, which names the function to the bridge with `A11Y_MAIN_QUEUE_TURN`, so `screen_reader_bridge.cpp` keeps no platform header.
- **Nothing else on the game thread waits for the worker**: the bridge's calls are made outside the voices' lock, and Prism's C API (`prism.cpp`) takes no lock the screen reader's context could hold.
- **Personal Voice.** On macOS 14 and later opening an instance asks for Personal Voice permission and waits up to two minutes for the answer, on the worker; whether that shows a dialog is a question for the play test.
