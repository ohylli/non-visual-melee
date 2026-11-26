# Cues

## Mental model

A **cue** is a sound the fork plays to say something about the game without speech. Cues play through the **cue engine** (`src/pc/a11y/cue_engine.cpp`, decision in ADR-0005), which has an output of its own:

- **Two streams.** The game plays its audio through its own SDL audio stream (`src/pc/audio.c`). The cue engine opens a second SDL stream on the same default playback device (`cue_output.cpp`), and SDL mixes the two. Nothing the cue engine does touches the game's voices, its mixer, its lock or its netplay sound handles.
- **Voices with targets.** A cue owns one or more **voices**. A cue's voice is created once with a **sound** (a mono buffer of samples at 48 kHz), or for a pulsed voice or a one-shot its copies at many pitches in place of one (below), and a **mode**: looped, playing the sound over and over without a seam; pulsed, playing it once every interval; or one-shot, playing it once at each trigger (below, "The one-shot voice"). A fourth mode, once, has no sound of its own: it is the speech voices' (below, "The once voice"). Each tick the game thread writes the voice's **targets**: pan, pitch (the playback rate, or with copies the copy nearest it, below), gain (up to 1, a cue's boost being its cue level's; a once voice's carries its own boost) and, for a pulsed voice, the interval. The audio thread glides each voice towards its targets over a few milliseconds, so a step once a tick is heard as a smooth change, not a zipper noise.
- **Pulses on the audio clock.** A pulsed voice is restarted by the audio thread itself, each pulse one interval after the last, at the interval current then. The game thread never starts single pulses, so a rhythm stays even, and a faster interval is heard from the next pulse.
- **The volume chain.** A voice's loudness is master volume × mix level (cue volume and the all-cues switch) × its cue's level (switch, volume, boost) × its own gain target. The mix then has fixed headroom and a soft limit, so five voices at full gain together still stay under full scale. A voice made with `ignores_switches`, a preview's, takes the volumes and boost of both levels and leaves out their switches. A once voice, speech's, has a chain of its own (below, "The once voice").

```
game thread                       audio thread (SDL's)
-----------                       --------------------
start / stop / set_targets  --->  voices glide to their targets,
trigger (a one-shot)        --->  a one-shot's play from the top,
master, mix and cue levels  --->  pulses retriggered on the audio clock
                                  --> headroom, soft limit --> SDL stream --> device
                                                           \-> dump file
```

## The engine's rules

- **Starts and stops ramp.** Every start and stop goes through the engine's own 5 ms gain ramp, so a sound cut at full level never clicks. A voice stopped and started again before its ramp ends ramps back up from where it was, without restarting.
- **Constant-power pan.** Left and right gains are the cosine and sine of the pan angle, so a sound keeps its loudness as it moves across. miniaudio's own pan is louder at the sides, which is why the engine pans by itself.
- **A pitch keeps a pulse's length** (ADR-0010). Played alone, a sound's pitch is its playback rate, with 4-point interpolation: an octave up plays it in half the time. A pulsed voice or a one-shot may instead carry, in place of its sound, copies of it made at start a semitone apart (`VoiceSpec::copies`, below in "Sounds"): each pulse or play takes the copy nearest its target pitch, the one a glide is heading for, keeps it to its end, and plays it at the rest of the pitch, so the sound is as long at every pitch within a few percent, and a glide during the pulse still slides. Every match cue that changes pitch has copies; a looped voice cannot. No pitch shifter runs as the cues play: one would delay them by about a tenth of a second and smear a click's attack (the Star Fox 64 fork pinned its aim click to playback rate for that reason).
- **A long pulse is cut with a fade.** A pulse still playing when the next is due, its sound longer than the interval at its pitch, fades out over the stop ramp, where it was, as the next starts from the top. A slow pace so plays a long sound whole, and a fast one its start. A sound that ends inside its fastest interval at its lowest pitch, or at any pitch for one with copies, is never cut and sounds as it did before cuts existed. The engine holds every interval and pitch target to the voice's fastest interval and lowest pitch.
- **Alternating voices.** Two pulsed voices of one interval can be paired when the second is created (`alternates_with`). One of the pair starting from silence while the other sounds waits for the midpoint between the other's pulses, so the two alternate instead of landing together; started on the same tick, the one created later usually waits half an interval. The audio thread places it, so it holds whatever block size the device asks for.
- **Leave room for the ramp.** A short click should start with about 12 ms of silence, so the start ramp does not eat its attack (`lead_in` and `kLeadInSeconds` in `cue_sounds.hpp`, the Star Fox 64 fork's finding). A sound from a file gets it as it loads.
- **Threads.** The game thread writes, the audio thread reads; every shared value is an atomic and nothing is locked. Voices are added and never removed while the engine runs, so the audio thread can walk them without a lock.

## The one-shot voice

A one-shot is a cue's voice for a moment rather than a state: an item appearing. It is created with its sound like any cue voice, and plays it once from the start each time the game thread calls `CueEngine::trigger` with its targets, ramping in, and ramping out over the sound's last frames, so it falls silent by itself. `start` does nothing to it: `trigger` is its start.

- **A cue's voice.** It takes its cue's level and the mix's like a looped or pulsed voice: Accessibility, Cues, Cue volume and its cue's switch, volume and boost reach it, its gain target stops at 1, and a preview's twin made with `ignores_switches` keeps the volumes alone. Pitch is a playback rate here too, unless it carries copies: the appearance cue's does, so it is as long at every pitch.
- **A trigger while it plays** restarts it from the top at the new targets, the old play fading out where it was over the stop ramp, as a cut pulse does. `stop` ramps the play out and drops a trigger the audio thread has not yet begun. `playing` is the audio thread's word, as for a once voice: true from the trigger until the play has ended or ramped out.
- **Logged** once per trigger, with its targets; its stops are not logged. The trace shows it on while it plays.
- **Not the once voice.** The once voice below is speech's: it takes a sound handed over at run time, ignores pitch, and stands outside the cue levels on purpose. A one-shot's sound is fixed at creation.

### The pool

A one-shot pool (`OneShotPool`, beside the engine) is a few one-shot voices of one sound, its size a constant, for a cue whose plays may overlap: several items appearing on one tick each sound. `add` makes its voices from one spec, named `<name> 1` and up in log and trace lines, and keeps those it got if the engine runs out of voices. `trigger` takes the first voice not playing, or, with every voice playing, the one triggered longest ago, whose play then fades out as the new one starts; it returns the voice taken. `stop` and `playing` cover them all. A driver keeps one pool for its cue and a second, made from the same spec with `ignores_switches`, as its preview twin, and calls `trigger` with each play's targets: there is no voice to choose.

## The once voice

Speech's two voices, your voice and the opponents' voice (`docs/a11y/speech.md`, "Voices"), play through the engine, one once voice each, added by `voices.cpp`. A once voice plays a sound handed to it at run time (`CueEngine::play`), from its start, once, centred at pitch 1, ramping in and, over its last frames, out. A sound handed over while another plays cuts that one off with the stop ramp, then begins; one handed over before the last was taken replaces it unheard. `stop` ramps it out. `playing` is the audio thread's word: true until the sound has played or ramped out.

- **Its chain** is the master volume × its gain target, the voice's Volume times its boost (a tuning value, for speech that reads quieter than the game), and nothing else: the cue volume, Cues, Accessibility and every cue's switch and level leave it alone. Speech's switches decide whether a voice speaks at all.
- **Handing over without a lock.** The game thread writes each sound into one of four slots and publishes its number; the audio thread takes it and marks the slot done when it has finished with it; the game thread frees done slots at its next `play`, so the audio thread never allocates or frees. One slot sounding, one waiting for it to ramp out, one handed over and one being written are all there can be.
- **Recorded** in the dump like any voice, so a run can check that a line was rendered and played (`MELEE_A11Y_VOICE_PREVIEW` with `MELEE_A11Y_CUE_DUMP`). It logs no start or stop lines: the voices log their own.

## Sounds

Each cue's sound is made when the engine starts, as a `Sound` buffer: `cue_sounds.cpp` reads one from any miniaudio data source (a waveform, noise) and shapes its edges, or loads one from a file. Either way it is the same kind of buffer, so changing a cue to a file changes its definition, not the engine.

A pulsed voice's or a one-shot's sound can be given as its copies at other pitches in place of it (`add_voice` refuses a voice given both), a semitone apart across the pitches it plays at, made by `pitched_copies` (`cue_sounds.hpp`) once, as the engine starts, and shared by the voices that play the sound and their previews' twins. A sound made by code is made again at each copy's pitch, which is exact: the position cues' tick and pulse take a pitch and multiply their tones by it. A file's sound is moved to each copy's pitch by `shift_pitch` (`pitch_shift.hpp`), Signalsmith Stretch's spectral shift run over the whole sound: its partials move, its length and its loudness over time stay, with no delay, since the whole sound is at hand. The shifter's analysis block is shorter than the library's default, chosen for sounds of about a tenth of a second (`pitch_shift.cpp`).

Signalsmith Stretch is downloaded at configure and compiled once into its own library (`a11y_pitch_shift` in `a11y.cmake`, which says why that release and why optimised even in a debug build).

miniaudio 0.11.25 is downloaded at configure and compiled once into its own library (`a11y_miniaudio` in `a11y.cmake`), without its device code: SDL plays the mix. The engine renders the voices itself, since the per-sample glides and pulse timing need it; miniaudio supplies the generators and the file decoding. Keep it at 0.11.25 or later should a cue ever play through miniaudio's own sounds: 0.11.21 crackled on every pitch change (ADR-0005).

### Sounds from files

The item cues play recorded sounds the maintainer supplies, free to use, rather than sounds made by code.

- **Prepared by a script.** `tools/a11y/prepare_sounds.py`, run with `uv run` (uv installs numpy and soundfile from the script's own metadata block), takes an original by path, WAV or FLAC at any rate and channel count, and writes `resources/a11y/<name>.wav`, mono, 48 kHz: silence trimmed at both ends, the end faded over a few milliseconds so the trim never clicks, the peak normalized to just under full scale. The balance between cues is set by ear through each cue's volume and boost, not in the file. Its report gives the length and the fastest pace at which a pulse plays whole at the cue's lowest pitch (`--lowest-pitch`, in octaves down; the default 0 for a cue whose sound has copies, as both item cues' do, which plays it as long at every pitch), so a pace that cuts the sound is known before it is heard. Its `--help` has the thresholds; `tools/a11y/test_prepare_sounds.py` is its test.
- **The originals stay outside git; the prepared files are committed**, so a cue dump compares with exactly what plays and the game does no processing at start.
- **No lead-in in the file.** `load_sound` puts the engine's lead-in (`kLeadInSeconds`) before the sound as it loads it, as the generated sounds get theirs, since a pulse and a one-shot ramp in at every play. The script counts the lead-in in its report: change the two together.
- **Loading.** `load_sound(path)` decodes through `ma_decoder`, asked for mono floats at 48 kHz, so a file at another rate or channel count is converted as it loads, though the script's files need neither. The file is read into memory through a filesystem path made from the UTF-8 text first: miniaudio's own file reading takes the Windows code page and would miss an install folder with letters outside ASCII. The folder is `cue_sound_folder()` (`cue_output.cpp`, the engine's SDL side): `resources/a11y/` in SDL's base path, beside `melee.exe`, as the base port finds its own resources. The base port's build copies all of `resources/` beside `melee.exe` and its packaging scripts copy it into the packages, so the files ship with no base port file touched.
- **A missing or unreadable file** gives an empty sound and a log line naming the file and why; the engine refuses a voice with no sound, so that cue stays silent and the rest of the cues play on.

## Lifecycle and switches

- `CueSystem` (`cue_system.cpp`) holds the engine, its output, the cues' part of the accessibility settings and the match cues, and opens and closes them together; the hooks in `hooks.cpp` only forward to it. The accessibility settings themselves, with their file, belong to `hooks.cpp`, since they outlive a closed engine.
- The engine opens in `pc_a11y_init`, as the game starts: opening the stream takes part of a frame, too much for first use mid-game. It closes first thing in `pc_a11y_shutdown`.
- Opening early moves the process's heap layout. That once made a base port bug much likelier: the file cache took heap buffers below 0x01000000 for ARAM offsets and cached garbled archives (fixed in `file_cache.cpp`, see `store_host`). If a crash rate rises only with cues on, suspect another place where a low host address is taken for ARAM.
- The match's voices stop, with their ramp, when a scene is entered (`pc_a11y_scene_entered`): cues belong to the screen they play on. A preview (below, "Preview") plays on.
- **Silent while the port menu is open.** The menu holds the game still: `VIWaitForRetrace` loops on the frame boundary until it closes, so no tick runs, and without a tick nothing would change the voices; they would sound on under the menu's speech as the last tick left them, a gap warning humming and the position cues pulsing. The port menu's frame hook (`pc_a11y_port_menu_frame`), which runs at every frame boundary whether the menu is open or not, calls `CueSystem::silence_for_port_menu` on each frame it is open, before the menu's speech: `MatchCues::silence` stops the match's voices with their ramp, leaving a preview's alone, and forgets what the drivers keep between ticks (the gap warning's hold). The first tick after the menu closes starts the cues again from its snapshot alone, as after any silence, so nothing is remembered across the menu. A cue played through the game's audio plays once and needs nothing.
- The engine opens whatever the switches say: Cues off (`cues.on`), or Accessibility off above it, mutes the mix through the volume chain, and the voices go on running underneath, so the switches are live (`docs/a11y/speech.md`, "The switches"). `MELEE_A11Y_CUES` overrides Cues for the run, `1` on and `0` off, and holds against Accessibility, so an agent run with speech off (`MELEE_A11Y=0`) hears the cues with `MELEE_A11Y_CUES=1`.
- If the output cannot open, the log says why, speech says "Audio cues could not start." after the proof-of-life announcement, and the cues run unheard: their values are still registered, so `a11y.ini` and the menus' rows hold them.
- Master volume comes from the `pc_a11y_master_volume` hook in `pc_audio_set_volume`, the launcher's volume with mute applied, kept even before the engine opens. The second stream does not see the game's gain, so the engine applies it itself. The game's sound effect and music sliders do not affect cues.
- Online, the engine needs no `net_sfx_private`: it never reaches the game's sounds. The hooks that drive cues stay silent while `pc_net_resim()` is true, so voices keep their last targets through a rollback.

## Accessibility settings

The cue values grew into the fork's whole model of settings, the accessibility settings (`CONTEXT.md`), kept in one file, `a11y.ini`. The cues were its first users and still hold most of its values, which is why it is described here.

### The model

The accessibility settings are one model of named values (`ValueModel` in `value_model.hpp`, each value a `NamedValue`), owned with its file by `AccessibilitySettings` (`accessibility_settings.hpp`), which `pc_a11y_init` creates before any feature registers. Each value has a key (`cues.volume`), a short label for a menu row, a one-line description, a unit, a default and a range; a switch is a value of 0 or 1. That is enough to draw a switch or a slider, and for the file to explain itself. The third kind, a choice, holds the text of one of a list of named options instead of a number, with no unit or range ("Choices" below). Values sit in groups, one for all cues, one per cue, and one for values several cues share (`CueSettings::add_shared`), and a group's title heads it in the file.

The model knows nothing about any cue. A feature registers its own values and keeps the typed handles it gets back (`Switch`, `Number`, `Choice`), which read the current value without looking up a key, so reading them every tick costs nothing.

### Choices

A choice (`ValueGroup::add_choice`, `ChoiceSpec`) is one of a list of named options, saved as the option's text rather than its place in the list, so a list that changes never moves a saved choice to another option. Two needs shaped it: the match announcements' damage rows have options fixed in code, and the Voice rows' options are the voices Windows has installed, known only at run time.

- **Where the options come from.** Fixed options are given at registration (`options`); a run-time list is a function (`list`) asked each time the options are needed, every frame of an open menu page among others, so it hands back a copy of a list kept elsewhere. Each option has the text the file saves ("Microsoft Mark") and, when the row should show more, a label ("Microsoft Mark, English (United States)").
- **The option in use** (`choice_index`, `Choice::option`) is the one whose text is the saved text, ignoring case, so a hand edit of "off" names "Off". A saved text that names no option listed now, as for a voice uninstalled, stays saved, in the model and in the file, until the player picks another option; meanwhile the option in use is the registration's fallback, a function given the saved text and the options (so a default of "Zira" can find "Microsoft Zira"), and without one the default's option, or the first.
- **No override.** The environment overrides only switches; a choice has none.

### Settings and tuning values

A value registered with `shown` set is a setting: a row of the launcher's and the port menu's Accessibility category. Every other value is a tuning value, changed only in the file. `ValueModel::shown` lists the rows in the order they were registered: the shown values, the actions, rows that do something rather than hold a value, and the rows that open a submenu. A front end walks that list and keeps none of its own: a row with a value is a switch, a slider or a choice by the value's kind, a row with an action is a button, and a row opening a submenu opens it ("Where a row goes"). The only actions so far are the Preview buttons, whose `group` is the cue's id or the voice's group (`your_voice`, `opponents_voice`).

To show a tuning value, set `shown` where it is registered; to move a row, move its registration, or the submenu it is registered in ("Where a row goes"). The menus have no group headings, so a shown value's label names what it is about ("Ledge warning distance", not "Distance").

### Where a row goes

The rows are in submenus (`CONTEXT.md`; ADR-0009): the category's top level holds the master switches, each followed by the row opening its submenu, Cue settings after Cues and Match announcement settings after Match announcements, with Voice settings first inside the latter. A submenu is a `NamedMenu` (key, label, description, the submenu it is in), never in the file.

- **A value names its submenu through its group handle.** `ValueGroup::in(menu)` gives the same group, its shown rows going into that submenu, and `ValueGroup::add_menu` registers a row opening a submenu where it is called, in the handle's own submenu, so a submenu nests by being added through a handle already inside one. A handle handed on keeps its submenu: `CueSettings` registers Cues on the top level, then Cue settings, and hands out every cue's group (`add_cue`) and every shared group (`add_shared`) already inside Cue settings, so a new cue, or a value a cue shows, lands there with no other change. `MatchAnnouncementSettings` registers its switch, Match announcement settings, then Voice settings as that submenu's first row, and gives the voices that submenu (`voice_menu()`); `VoiceSettings` takes it as the menu its rows go into.
- **Each level's rows are in registration order**, the order `ValueModel::shown` lists them in, each entry with the submenu it is in (`ShownRow::menu`) and, for an opening row, the submenu it opens (`ShownRow::opens`). The top level's order is the order in which `pc_a11y_init` creates the features, as its comment there says. The file follows registration order too, so a fresh `a11y.ini` lists the groups in that order; an existing file keeps its own, as the line edit keeps it.

### The cues' values

`cue_settings.hpp` registers the values for all cues (a switch on the top level and the cue volume, 0 to 100%, first in Cue settings, both shown) and gives every cue the same three of its own through `add_cue`: a switch, described by what the cue does, and its volume, both shown and followed by the cue's Preview action, and its boost (a factor of 1 to 8 under the volume, for a short click that reads quieter than a held tone), a tuning value. A cue registered with `add_cue` so appears in Cue settings with no other change. `add_cue` also hands out the cue's number in the engine. The cue then adds its mapping values to the group it got back, as tuning values unless one is worth a row:

```cpp
a11y::CueGroup cue = cue_settings.add_cue("your_position", "Your position cue",
    "A tick that pans with where you are across the stage.", 2.0f);
a11y::Number pitch_range = cue.group.add_number({"pitch_range", "Pitch range",
    "Octaves from the bottom blast zone to the camera's top.", "", 2.0f, 0.0f, 2.0f, 0.1f});
```

Every cue registers before the file is read, so a value the file lacks can be added to it. These values feed the volume chain: the all-cues switch and cue volume become the mix level, each cue's switch, volume and boost its cue level. A switch turned off is a level turned off: the engine glides that cue to silence while its voices keep running underneath, and turning it back on brings them back where they are.

Every change after the file's first read, from a hand edit or a menu, goes to the settings' one listener, which `hooks.cpp` points at `CueSystem::settings_changed`; it applies only the levels the changed values make up.

### The file

- **Where.** `a11y.ini` in the folder where the base port keeps its own settings (`pc_get_pref_path`: `%APPDATA%\melee-pc` on Windows, or the folder `MELEE_USER_DIR` names). `MELEE_A11Y_SETTINGS=<path>` names another file; set but empty, it means no file and the defaults, so a run that meant a scratch file and lost its path never falls back to the player's. An older `a11y-cues.ini` beside it is no longer read. `drive.py` and `drive_net.py` give each run a fresh settings folder of its own and name its `a11y.ini` in `MELEE_A11Y_SETTINGS`, so no agent run reads or writes the maintainer's file.
- **What it looks like.** Plain text, one `key = value` a line, each value under its group's `#` heading and right below its description, which ends with the range and the default. A switch is `on` or `off` (also yes, no, true, false, 1, 0); a number may carry its unit (`80%`) and a decimal comma; a choice is its option's text, whose description lists fixed options and leaves a run-time list unnamed ("One of the options the menus list"), so the file never depends on what was installed when it was written. `#` starts a comment anywhere on a line. Of two lines with the same key, the last counts.
- **A missing file** is written with every value at its default, so there is something to edit. A value the code has and the file lacks, as after an update that adds a cue, takes its default and is appended to the end of the file under its heading, leaving everything the player wrote as it was. A key the file has and the code does not is logged once and left in the file.
- **Errors.** A line that cannot be read, a choice's empty one included, keeps its value's last setting, is logged and is spoken as one announcement ("Accessibility settings, lines 3 and 7 not understood."); the other lines apply. A value out of range is clamped and logged, not spoken; so is a choice's text that names none of its fixed options, which is kept and logged with the option used instead.
- **Overrides.** An environment variable that overrides a switch for the run (`docs/a11y/speech.md`, "The environment overrides the file") holds the value the game uses with `ValueModel::override_value`, set before the file is first read. The file's own value waits beside it in the `NamedValue` (`saved`): reading the file updates it silently, and writing the file, whole or a line, writes it, so the override never reaches the file. A front end's change lifts the override.

### Saving a menu's change

A front end changes a value through `AccessibilitySettings::set`, or a choice through `AccessibilitySettings::choose` with the option's text, never by writing the model and the file itself. The game then edits only that value's line (`ValueModel::edit_text`): the value on the key's last line is replaced, keeping the key as written, its spacing, a comment after it and the line's ending; with no such line, the line and its description go at the end of its group, under its heading. Comments, order, the player's other lines and unknown keys stay as written. The text goes to a temporary file that is renamed over `a11y.ini`, so a crash halfway leaves the old file whole, as the base port saves `launcher.cfg`.

Two orderings matter. The file is read just before the edit, whatever the poll's interval, so a hand edit saved a moment before a menu change is applied and kept rather than lost under the save. And the save leaves the file's stamp and text as the poll last saw them, so the poll that follows the game's own write reads nothing and says nothing.

With no file (`MELEE_A11Y_SETTINGS` empty, or no settings folder) a change applies until the game closes.

### Live reload

The file is read once as the game starts, once every feature has registered, then `poll` looks at it about twice a second: at scene entry, in a match from the match tick, and every frame while the launcher or the port menu is open (`pc_a11y_launcher_frame`, `pc_a11y_port_menu_frame`), so an open menu shows a hand edit. Only a file whose time or size changed is read, only a text that changed is parsed, and only values that changed are applied and logged. Deleting the file brings every value back to its default and writes the file afresh. A line removed from the file goes back to its default too.

An error found as the game starts is spoken after the proof-of-life announcement, since speech waits for the switches. The spoken error is queued, and speech at a scene change can cut it off: the launcher and the screens that speak their opening on their first frame interrupt. `ValueFile::problem` keeps the last read's error so a later moment, such as a match starting, can say it again.

## The match cues

The cues of a match follow one idea: once a tick, read the match into a snapshot, hand the snapshot to each cue's driver, and give the voices the targets the drivers return. Nothing in a match calls the cues; they poll, as the menu readers do (ADR-0003).

```
gm_RunSimTick --pc_a11y_match_tick--> a11y_game_match_state --> A11yMatchState
                                                                   |
     the cues' values -----------------------> drivers (pure) <---+
                                                  |
                                                  v
                                 MatchCues: start, set targets or stop each voice
```

### The tick hook

`pc_a11y_match_tick(held_mask)` runs right after each simulation tick, beside the replay recorder (`gameplay-state.md`, "The tick hook"), in every scene that ticks. `CueSystem::tick` reads the snapshot there, handing it what the cues ask of it (the ledge warning's edge offset), and passes it to the cues; the snapshot's casts are safe only there (ADR-0006). While a rollback re-runs ticks it does nothing, so the voices keep their last targets and pick up from the next tick played for the first time. In a match it also polls the accessibility settings file, and on the first tick of a match it says the settings file's last error again, if there is one: speech at a scene change may have cut it off, and a match is where the player listens.

### The match snapshot

`A11yMatchState` (`game_access.h`), filled in C by `a11y_game_match_state` because the fighter's struct has bitfields laid out as the game lays them out. It only reads.

- **Running**: a match is on with the local player's fighter on the live fighter list. Outside one, nothing else is filled and nothing sounds.
- **You**: the player character select follows. Offline the human slot on controller 1, else the first human slot, as when controller 2 started a single-player mode; online the human slot on `pc_net_local_player()`'s port. The slot's main fighter: Nana and the sleeping Zelda or Sheik never count.
- **Each fighter**: position at the feet, ground or air, out (knocked out or between death and respawn, motion states 0 to 11), whether its input is on (off before GO), its slot and team.
- **The opponent**: the nearest other slot's main fighter in play, not on your team in a team match, by straight-line distance, chosen afresh each tick. A list of one, so more can come.
- **Decided**: GAME! or TIME! has come, from the match's scene state as the HUD snapshot reads it; Training never comes to it. The match's last ticks still run after it.
- **Paused**: the game holds the match still on purpose: a pause, the GAME! freeze, a score tally. It comes from the game's pause word, not from the held mask, which a Training pause leaves clear and slow motion sets on its skipped ticks (`gameplay-state.md`, "Pause, freezes and slow motion"). The held mask is kept as `fighters_held`.
- **The floor below you**: on the ground the floor line under your feet; in the air the first floor the game's own downward cast (`mpCheckFloor`) meets from just above the bottom of your collision box down to the bottom blast zone, platforms included, with its line and height. None in the air is a gap. Every line id is checked against the line count before it goes into the snapshot. The cast is safe between ticks and starts at the collision box rather than the feet, which in the air sink below a floor just before landing (`gameplay-state.md`, "Is there ground below?").
- **Its ends**: the floor's surface followed to its left and right ends (`mpFloorGetLeft`, `mpFloorGetRight`), each deadly or not. An end is deadly when no wall rises from it and the same downward cast, from just above the end's height and the edge offset (`ledge_warning.edge_offset`) outwards, meets no floor. The offset clears the unit a floor reaches past its end where another line joins it, and stays under the 10.8 units from a Battlefield side platform's end to the main floor's, so the platforms' ends stay safe. The wall test is for a floor that ends at a step up, as against Princess Peach's Castle's tower, where no walk leaves the floor and a cast past the end would start inside the stage (`gameplay-state.md`, "How far to the edge?").
- **The stage frame**: the blast zones and the camera box, read each tick since some stages move them.
- **The items**: every item in the fork's sense (`gameplay-state.md`, "Items in the fork's sense"), nearest to you first, each with its serial, kind, position, whether it is loose, whether it is within reach by the game's own test (called here between ticks, as the cast is, and not while you are out; asked as the press would ask it, A's pickup on the ground and Z's catch of a light item in the air, `gameplay-state.md`, "Within reach") and which slot holds it; a constant number are kept, and the count of all says when the list was cut short. Read in paused ticks too, since Training's item menu brings an item in while it holds the match. Who took what is the match HUD snapshot's (`A11yMatchHudPlayer.hands` and `taken`; `gameplay-state.md`, "Taking an item").

### Drivers

A driver is a pure function from the snapshot and its cue's mapping values to its voice's targets, or to none when the cue is silent (`position_cue.hpp`, `ledge_warning.hpp`); the gap warning's keeps one count beside, its hold (`gap_warning.hpp`), and the opponent's position cue, relative to you, the opponent's side (`OpponentPosition`). The cues about you share one test of whether you are in play, `you_in_play` (`match_state.hpp`): a match running, not paused, past GO, and you neither knocked out nor waiting to respawn. The appearance cue's keeps the items it has seen and those held through a pause (`item_appearance.hpp`), and the item's position cue's each tracked item's side (`ItemPosition`, `item_position.hpp`). Tests feed the drivers hand-written snapshots (`test_position_cue.cpp`, `test_gap_warning.cpp`, `test_ledge_warning.cpp`, `test_item_appearance.cpp`, `test_item_position.cpp`). `MatchCues` (`match_cues.cpp`) owns the rest: it registers each cue's values and voices once, as the cue system opens, and each tick starts a voice that has targets (a start while playing only changes them), and stops one that has none. Its own tests (`test_match_cues.cpp`) render the voices into a buffer.

The two position cues share the stage mapping (`StageMapping`, `stage_targets`): pan and pitch from a fighter's place, the same code for both, each cue with its own pitch range in its own group of the file and one pan frame for both (below).

The mapping is framed by the stage's own boxes alone, with no floor as a reference: finding the main floor is uncertain on stages without one clear floor, and the ledge warning says where the floor's ends are. The pan's frame is a choice between two, one switch for both cues (`position_cues.pan_to_camera`) so that their pans stay comparable. Framed by the camera box's sides, the default, the main floor's ends land about halfway out on Final Destination and Battlefield, but the pan is full for the stretch between the camera's view and the side blast zones, as wide as half of Final Destination. Framed by the blast zones, the floor's ends land about a third of the way out (`gameplay-state.md`, "The tournament stages' frames") and the pan goes on moving until a knockout, two thirds of it left for the space off the stage, where recoveries happen. Which matters more is the player's call. With the opponent's cue relative to you (below), the switch frames your cue alone.

So pausing stops a cue and unpausing starts it again: a stop is the engine's 5 ms ramp, and a pulsed voice started from silence pulses at once (one alternating with a voice still sounding at the midpoint, above), which marks the match going on. The log shows both. Each tick of a running match is also a trace tick, numbered on from the first match's first tick, menu ticks left out.

### Your position cue

A tick whose pan says where you are across the stage, whose pitch says how high, and whose speed says how near the opponent is.

- **Pan**, linear in x: the camera box's sides ±1, its middle 0, held at ±1 beyond. With `position_cues.pan_to_camera` off, the side blast zones instead.
- **Pitch**, linear in octaves of y: from the bottom blast zone, `pitch_range`/2 octaves down, to the camera box's top, as many up, held beyond both. The bottom is the blast zone, not the camera box's bottom, so a fall keeps sounding lower until the knockout. Standing on the main floor is not one fixed pitch: it sits near the middle, a little above it on Final Destination and below it on Battlefield.
- **Interval**: from `slow_interval` at `far_distance` and beyond to `fast_interval` at `near_distance` and closer, interpolated in the log of the interval, so each unit of distance closed speeds the tick by the same ratio. With no opponent, the slow interval.
- **Silent** outside a match, while paused, before GO and while you are out.
- **The sound**: the Star Fox 64 fork's aim click, a short decaying sine after 12 ms of lead-in.
- **Its limits.** The tick, 32 ms at every pitch through its copies, ends inside the fastest pulse the fast interval allows (`MatchCues::kFastestInterval`). The copies span the widest pitch range, 1 octave down to 1 octave up (`kWidestPitchRange`); a wider range needs only more copies.

### The opponent's position cue

A rounder, lower pulse placed where the opponent is, at a steady pace: from you by default, or on your cue's stage mapping.

- **Pan and pitch**, with relative to you off, as your cue's, from the opponent's position, in the same pan frame, with `opponent_position.pitch_range` of its own, starting at your cue's default.
- **Interval**: fixed, `opponent_position.interval`. How near the opponent is stays your cue's job.
- **Which opponent** is the snapshot's: the nearest one in play, chosen afresh each tick. When another fighter becomes the nearest, the voice moves there without restarting, so the pulse keeps its rhythm.
- **Silent** outside a match, while paused, before GO, with no opponent, and while the opponent is out; it sounds again on the respawn platform. While you are out it plays on.
- **The sound**: 12 ms of lead-in, then a 400 Hz sine with a quarter of its octave, rising over 5 ms and dying away over 70 ms. 82 ms at every pitch through its copies, made again at each pitch as your tick's are; the interval goes no lower than 0.2 s (`MatchCues::kOpponentFastestInterval`), which leaves silence between the pulses.
- **Relative to you**, the switch `opponent_position.relative`, on by default: the cue says where the opponent is from you rather than on the stage, so their side is heard from one cue instead of compared across two pans. Your cue stays on the stage mapping, the reference.
  - **Pan** from the horizontal distance: the opponent's side at `pan_floor` level with you, linear out to full at `side_distance` and held beyond. The floor keeps the side audible however near they are.
  - **Crossing**: the side holds until the opponent is more than `crossing_margin` past you the other way, then flips once, so fighters overlapping, or one standing over the other, do not flicker the pulse between the ears. The pan within the margin still grows with the distance, on the side kept. The side is the driver's only state: a new opponent, one back from a silence and every tick after the port menu's silence take it afresh from the distance's sign.
  - **Pitch** from the vertical distance, linear in octaves: the cue's own pitch level with you, `pitch_range`/2 octaves up at `height_distance` above you and held beyond, as many down as far below.
  - **Silent** also while you are out: your place is then where you were knocked out, which would make the pan nonsense.
  - **The preview** plays the opponent level with you, half its pulses on the left at the pan floor and the rest on the right, the side changing midway between two pulses.
- **The two rhythms.** Far apart, your cue's slow interval and the opponent's start together at GO; while one is a whole multiple of the other they stay in step, the opponent's pulses landing on your ticks. Whether that reads as one rhythm is the play test's question (`.scratch/match-cues/spec.md`, open questions).

### The gap warning

A held, centred hum while you are in the air with nothing below you, so that falling straight down would take you past the bottom blast zone.

- **The test** is the snapshot's floor below you: in the air, in play, and the cast met no floor. Platforms count as floor, so the warning stays off over Battlefield's platforms and anywhere above the main floor, and comes on past the main floor's ends (about a unit past them, where the cast stops meeting the floor) and under the stage.
- **The hold**: leaving a gap, the hum sounds on for `gap_warning.hold` ticks, so drifting back and forth over an edge does not stutter. A gap within the hold starts it afresh.
- **Silent** outside a match, while paused, before GO and while you are out, at once and without the hold; on the respawn platform the floor is below, so it stays off.
- **The sound**: 120 Hz with its third and fifth harmonics at a third and a fifth of its level, the start of a square wave, looped. It is the only held cue and the lowest: the opponent's pulse goes no lower than 200 Hz and your tick no lower than 750 Hz at the widest pitch range, and both pulse. The harmonics, at 360 and 600 Hz, let small speakers play it. Constant pitch and loudness, centred: the hum says only that there is a gap.
- **The game's own warning**, sound 96 as a fighter falls past the midpoint between the camera's bottom and the bottom blast zone, plays as before; the gap warning starts much earlier, as soon as nothing is below.
- **Hanging on a ledge** is in the air with nothing below (measured: Link hanging from Final Destination's left ledge at x = -88, y = -18.8), so the hum sounds through the hang. The play test kept it: you are still in danger there.

### The ledge warning

A pulsed chord on the left or right while you are near a deadly end of the floor below you, on the ground and in the air, so an edge is heard coming when you run, jump or are knocked across the stage.

- **Which ends**: the snapshot's floor below you and its ends (above). Only a deadly end sounds: a platform's end over the main floor stays silent, and so does a floor's end at a wall rising from it. With no floor below there is nothing to say; past the edge the gap warning has taken over.
- **Closeness**: the horizontal distance from you to a deadly end, held at 0 past it (in the air a unit past an end the cast still meets the floor). Within `ledge_warning.distance` a voice sounds on that side, `ledge left` or `ledge right`; both when both ends of a small floor are near. The two voices alternate (above, "Alternating voices"): with the same chord at the same rate, pulses landing together would blend into one sound in the middle.
- **Mapping**, the Star Fox 64 fork's beside chord: pan fully to the side at the warning distance, moving towards the centre as you close in and stopping at `pan_floor` at the edge, short of the centre; gain rising from `level_floor` at the warning distance to full at the edge. Both linear in the distance.
- **Handover**: walking off an edge, the floor below vanishes a unit past the end, so the ledge warning stops at its edge's pan and full gain on the same tick the gap warning starts. Coming back, the ledge warning starts again near the edge as soon as the floor is below, while the gap warning's hold runs out. Through a ledge hang nothing is below, so only the gap warning sounds.
- **Silent** outside a match, while paused, before GO, while you are out, and with no floor below. It has no hold: the distance changes smoothly, and the warning distance is the only place it starts or stops on the floor.
- **The sound**: 660 and 880 Hz together, a just perfect fourth as in the Star Fox 64 fork's beside chord, 90 ms rising over 5 ms and easing off after 12 ms of lead-in, pulsed every 0.2 s (`kLedgeInterval`), at a fixed pitch. A pulse gives the ear an onset to place, which a held tone panned by level alone does not; a chord sets it apart from the other cues' single tones, and its register sits between the opponent's 400 Hz pulse and your 1500 Hz tick.

### The item cues' frame

Where an item is, as pan and pitch, for both item cues (`item_frame.hpp`, `.scratch/items/spec.md`, "The item cues' frame"): a pure function, `item_targets`, of the snapshot, the item's place, a side and the frame's values, which the item's position cue shares with the appearance cue.

- **Items relative to you**, the switch `item_cues.relative`, on by default, a row after both item cues' rows: the opponent's position cue's relative mapping (`relative_pan`, `relative_pitch`), from the item cues' own copies of its distances (`item_cues.side_distance`, `pan_floor`, `height_distance`, tuning values starting at the opponent's defaults, kept in `RelativeMapping`). Silent while you are out.
- **On the stage**, with it off: your position cue's stage mapping (`stage_pan`, `stage_pitch`), in the pan frame `position_cues.pan_to_camera` sets for every cue on the stage.
- **The pitch range** is a cue's own, used in both frames: `item_cues.pitch_range` the appearance cue's, `item_position.pitch_range` the item's position cue's, each a tuning value starting at the same default.
- **The side.** A one-shot keeps no side: `item_side` takes the distance's sign, and an item exactly level with you, as Training's Ray Gun lies, is centred rather than on the right as a level opponent is. The crossing margin (`item_cues.crossing_margin`, a tuning value starting at the opponent's), which keeps a side through a crossing, is the item's position cue's, which keeps a side per tracked item.

### The appearance cue

A one-shot where each item appears (`item_appearance.hpp`, `.scratch/items/spec.md`, "The appearance cue"), its sound the maintainer's `item_appearance.wav` from `resources/a11y/`.

- **New items** (`NewItems`): an item not seen this match by its serial and kind, from GO to GAME! or TIME!. The kind is in the key for an online rollback, which can shift serials (`gameplay-state.md`, "Spawning"): a serial seen that comes back as another kind is new. What is listed before GO and on the GO tick itself is taken as seen, unsaid. From GAME! (the snapshot's `decided`) on nothing is new and nothing is marked seen: online a predicted tick can reach GAME! and a rollback take it back, and an item listed only then is new once the match goes on. The serials seen are forgotten at each scene entry (`MatchCues::scene_entered`), since each match is a scene and counts its serials from 0; the port menu's silence forgets nothing. A burst is the new items of one tick, in the snapshot's order, nearest to you first.
- **One play per item**, on its first tick on the list, where it came in, through the cue's one-shot pool, so a burst sounds on voices of its own. A broken container's contents come in where it broke, near the floor, and a Party Ball's several on one tick (`gameplay-state.md`, "Spawning").
- **Silent while paused**: a pause stops the match's voices, a one-shot still sounding among them. An item made while paused, as Training's item menu makes one while it holds the match, is held and played on the first tick that is not paused, where it lies by then, unless it has left the list meanwhile. So the cue sounds as the menu closes, while the announcement is said at the press. Skipping it instead would leave a Training spawn without a cue, and playing it under the open menu would break the rule that a pause silences the match.
- **Relative to you while you are out**, an item that appears is not played at all, nor held; the announcement is still said.
- **The items new on each tick** are also the appearance announcement's (`AppearanceAnnouncement`): `MatchCues` keeps them, `CueSystem::items_appeared` hands them to the hook, which has them said whatever the cues' switches, since muted cues still tick.
- **A missing sound** leaves the pool without voices: every trigger finds none and plays nothing, and the announcement goes on.

### The item's position cue

A pulse from the loose item within reach, or else the nearest loose item (`item_position.hpp`, `.scratch/items/spec.md`, "The item's position cue"), its sound the maintainer's `item_position.wav` from `resources/a11y/`.

- **The item followed** is the loose one within reach when there is one, even with another nearer your feet, so the quick pace always means the button takes the item heard; else the first loose one on the snapshot's list, which is nearest to you first; chosen afresh each tick. The game's reach test names the item nearest its pickup box in front of you, which can be ahead of you while the nearest lies just behind. The list itself stays in order of distance, as the appearances read it, and the driver puts the item within reach first. When the game's answer is an item the fork leaves out (a turnip, a Link bomb), or one not loose (a thrown item flying in front of you), no item the cue may follow is within reach and the nearest is followed at the steady pace, which is right: the button would take that other item, not the one heard. Another taking the voice, by coming nearer or into reach, only moves it, with `start` on a playing voice, so the pulse keeps its rhythm; a new item that is nearest is followed at once, beside its appearance cue. Held items and those flying after a throw are not loose and are skipped; holding an item yourself leaves the cue on, following the others. An item is followed only once it is loose: a light item the item switch drops from its first tick, as it falls, but a heavy one (a Crate, a Barrel, a Party Ball) only once it has landed (`gameplay-state.md`, "Held, thrown, loose").
- **Pace**: `item_position.steady_interval`, and `reach_interval` while the snapshot marks the item within reach, both tuning values. Only two speeds: distance is the pan's and the pitch's job. A change to the fast pace pulses at once when more than two fast intervals have passed since the last pulse (the engine starts the rhythm afresh), so turning to face an item is heard straight away. A sound longer than the fast interval, as long at every pitch through its copies, is cut there with a fade and plays whole at the steady pace. The voice's fastest interval is `MatchCues::kItemFastestInterval`, the reach interval's lowest setting.
- **Place**: the item cues' frame. Relative to you, each tracked item's side is kept by its serial until it is the crossing margin past you, so walking over an item does not flicker it between the ears; one not tracked on a tick loses its side and takes it afresh when tracked again, and a new item exactly level with you sounds centred until it moves.
- **Silent** outside a match, while paused, before GO, from GAME! or TIME! on, with no loose item, and relative to you while you are out. GO is latched, the first tick with your input on, until the next scene entry, so on the stage the cue sounds on through a knockout and respawn.
- **Reach in the log.** Its start line gives the pace it starts at; after that each change of reach while it sounds is logged, so a drive script can wait for it. The trace shows each tick's pan and interval.
- **Ready for several items.** `kTrackedItems`, the number followed, is 1, with no row: the driver returns that many voices' targets, the one within reach first and then the nearest, each keyed by its item's serial, and `MatchCues` adds that many voices (`item position`, then `item position 2` and up). Following several is then a setting and its voices; how they sound together is that feature's question.

## Positional sound effects

### Mental model

The game plays every fighter's sounds centred, at the volume the move asks for: a sighted player's eyes place them. Positional sound effects (`sfx_position.cpp`, driven from `CueSystem`) place them by ear instead: a fighter's sound is panned to where that fighter is, from you or across the stage, and played quieter the further it is from you. Your own fighter's sounds stay centred at their own volume. Unlike a cue, nothing new plays: the fork changes the volume and pan of the game's own voices in the game's own engine (`src/sysdolphin/baselib/axdriver.c`), which mixes in stereo; the game's Sound option must be Stereo, or the synth sums every voice to the centre.

### Knowing whose sound it is

The engine knows a sound only by its id, volume, pan and track, so the fork learns the fighter three ways, each through a hook:

- **By track.** Many fighter sounds go on tracks the game gives each fighter, in blocks of twelve, `base + slot * 2 + sub` (`ft_0881.c`, and `0x7E` for the hits a fighter takes in `ftcoll.c`). `pc_a11y_sfx_started`, called as every start takes its engine voice and before it first sounds, places such a start by rewriting its volume and pan in place.
- **Named before the start.** `ft_PlaySFX` (`ft_0877.c`), the path most fighter sounds take, starts on track 0. `pc_a11y_fighter_sfx`, just before its start, names the fighter, and the start is placed as on its track. A sound object (`lbAudioAx_800263E8` in `lbaudio_ax.c`: the landings and steps of a fighter's action script) is named the same way by `pc_a11y_sound_object_starting` from its owner, and since the game sets an object's pan and volume again each frame, `pc_a11y_sound_object_pan` and `pc_a11y_sound_object_volume` give the placed values in place of the game's there.
- **A hint before the start.** A hit's sound (`lbColl_80005BB0` in `lbcollision.c`) is asked for on its fighter's track, which `lbAudioAx_80024184` then drops for track 0. `pc_a11y_sfx_track_hint` passes the asked-for track on, and the next start is placed as if on it. An item's hit asks for none.

A name or a hint holds only for the start it belongs to and only until the next tick, so a stray one never places an unrelated sound.

### Every layer of a sound

A sound is a small script (`AXDriverExec` in `axdriver.c`) that may start several layers, a landing's thud and its scrape. The engine gives the voice's own pan (`pan`, with its flag) to the first layer only and clears the flag; each later layer takes the script's pan, `x1C`, which starts at the centre and which the script's own pan commands move. A start is therefore placed by setting both, which `pc_a11y_sfx_started` hands over, so every layer comes from the fighter. Placing a sound once it has started, through `HSD_AudioSFXSetPan`, reaches only the layer playing: a later layer then sounded centred, a second copy of a landing in the middle (heard in play). That is why fighters are named before their starts, and why the per-tick following moves only the layer playing.

### A sound's own script

A sound's script may also set an absolute pan of its own, a pan command near the centre (122 or 132 for a landing or a jump), which the engine applies in the same audio frame as the sound's start: a placed sound would open in the middle and only move back at the next tick, its sharp attack centred and its tail placed (heard in play as an extra copy in the middle). `pc_a11y_sfx_script_pan`, on that command, takes the script's pan as an offset from the voice's place instead (`sfx_script_pan.cpp`). The command runs on the engine's audio thread, so the places are in a lock-free table, one atomic word per engine voice slot holding the whole voice id and its pan, written by the game thread as it places or moves a voice; a later voice in the same slot never takes an earlier one's place.

### Following and letting go

Each tick (after the cues' snapshot, in `CueSystem::tick`) works out the place of every fighter present, the main fighter and the sub fighter (Nana) of each slot, and pans each followed voice again once its fighter's place has moved a little, so a long sound, a charge or a reflector, moves with the fighter. The engine's answer tells when a voice has ended, and it is let go. Volume is set at the start only: a later volume change is the game's own (a fade).

### Online and rollback

Only the sound engine is touched, never the simulation: the voice ids the fork uses are the engine's own, never the netplay handles the game keeps, so nothing is counted in a frame's call order (`src/pc/net_sfx.h`). Every hook is silent while `pc_net_resim()` is true. A start that a rollback leaves late, played when the re-run catches up, is not placed.

### Its settings

Its group, `positional_sfx`, sits in Cue settings: the switch, whether sounds are placed from you or across the stage (on the stage it pans as the position cues do, following their camera switch) and the far level are shown; the side, near and far distances are tuning values. It follows Accessibility, not the Cues switch, since it plays no cue: with Accessibility off the game's sounds are left as the game made them.

## Stage sounds

### Mental model

A switch, Stage sounds, mutes the sounds a stage makes of its own (Corneria's Arwings and Great Fox, Mute City's cars, Pokémon Stadium's crowd), so the fighters are easier to hear and to place. `stage_sounds.cpp`, driven from `CueSystem` like positional sound effects. It is not the stage's echo: that is the base port's Reverb setting, which stops every voice sending to the reverb busses (`pc_audio_set_reverb` in `src/pc/audio.c`), and Final Destination has echo but no stage sounds.

### Knowing a stage's sound

Every sound id falls in one bank, one of the game's `.ssm` files (`ssm_files` in `lbaudio_ax.c`), and each stage with sounds of its own has its own bank, `castle.ssm` to `pstadium.ssm`. `pc_a11y_sfx_started` now carries the sound id, and the game's own lookup, `lbAudioAx_80023130` (through `a11y_game_sfx_bank`), gives the bank. No hook in the stages' code is needed. A stage's sound that comes from a shared bank (`main.ssm`) is not caught, nor are the Adventure stages' (`1padv.ssm`).

### Holding it silent

A muted sound starts at volume 0. The stages' own code fades its long sounds in and out (`ground.c`), so each tick sets every muted voice back to 0 until it ends; switched back on, each one still playing gets its own starting volume back. Like positional sound effects it follows Accessibility, touches only the sound engine, and is gated on `pc_net_resim()`.

## Preview

A preview is a short sample of one cue, played on demand outside any match, so a player tuning a cue's volume in the Accessibility category hears what it sounds like there and then. The category's Preview rows (`.scratch/accessibility-settings/issues/04-accessibility-category.md`) are its callers; a row's action holds the cue's id, the one `add_cue` took.

- **A fixed moment.** Each cue supplies a typical moment of a match beside its driver, as a pure function of the seconds since the preview began that returns none once it has ended (`your_position_preview`, `opponent_position_preview`, `gap_warning_preview`, `ledge_warning_preview`, `item_position_preview`; shared lengths in `cue_preview.hpp`). A one-shot's preview is a list of plays at their moments instead (`appearance_preview`), which `MatchCues` triggers as each falls due and ends once the last has sounded. The position cues pulse a few times from the centre at middle height, at their slow pace; the gap warning's hum rises, holds and falls away; the ledge warning pulses a few times on the left, then on the right, at the pan and level its mapping gives halfway into the warning distance; the appearance cue plays three items appearing, to your left, to your right and just above you, a pause apart, in the frame Items relative to you sets, with you at the centre of Final Destination; the item's position cue pulses a few times at its steady pace from an item a little to your right, then a few at its within-reach pace, in the same frame and stage. Each uses the cue's current values, so a tuned interval or pan floor is heard as tuned.
- **Voices of its own.** `MatchCues` gives every match voice a preview twin, the same sound named `<voice> preview` in the log, made with `ignores_switches`. A preview so sounds with Accessibility, Cues and the cue's own switch off, which a player tuning a cue they have turned off needs, and still honours the master volume and mute, Cue volume, and the cue's volume and boost.
- **Who moves it.** `CueSystem::preview` starts one by id and `stop_preview` ends it with the usual ramp; both are game-thread calls, outside the simulation. `CueSystem::preview_frame` moves it along each frame, from the launcher's frame hook and from the port menu's, which runs at every frame boundary, open or not. One plays at a time: another cue's preview stops the one playing, and the same one again restarts it. Restarted voices first wait out `MatchCues::kPreviewRestart`, past the stop ramp and an audio block, so they start from silence with a pulse at once rather than carrying on the old rhythm, and the restart is heard as one.
- **Why the port menu silences only the match.** The port menu's per-frame silence exists because no tick runs under the menu to stop the match's voices. A preview is started from the menu itself, so a silence of every voice would cut it off on the next frame. `MatchCues::silence`, and so `scene_entered`, therefore stops the match's own voices and nothing else: a preview plays over a silent match, and the match's cues come back with the first tick after the menu closes, as before.
- **Online** a preview changes nothing a peer sees: it plays on the cue engine, never through the game's sounds, and its frame calls sit outside the simulation, so rollback never re-runs them.

## Checking a cue without hearing it

The agent cannot hear a cue, so the engine keeps three records, the last two off unless asked for:

- **Log lines**: each voice's start and stop with its targets at that moment, and the engine's opening. Per-tick changes are left out, so the log tells when a cue sounds, not how.
- **The trace** (`MELEE_A11Y_CUE_TRACE`): every voice's targets every tick, for checking a mapping by numbers against what the game did.
- **The dump** (`MELEE_A11Y_CUE_DUMP`): the cue mix itself, from the audio thread. SDL's dummy driver, which every agent run uses, still pulls the mix in real time, so a silent run records a true dump.

`MELEE_A11Y_CUE_PREVIEW=<id>,<id>...`, a developer switch, plays those cues' previews in turn from the run's first frame, a pause apart, whatever the switches say, which with the dump checks a preview without a menu.

`tools/a11y/cue_dump.py` turns a dump into what an ear would notice, window by window: level, the left-right balance as a pan, pulse onsets with their intervals, and the dominant pitch. Its measures are simple ones (onsets by a level threshold, pitch by zero crossings), so two cues sounding at once blur pan and pitch, and a pulse that starts while another cue sounds is no onset at all.

`--band LOW HIGH` keeps one cue's register before the analysis, a steep band-pass on both channels, so two cues at once can be checked one at a time: the opponent's pulse in 150 to 1000 Hz, your tick in 1100 to 12000 Hz, while both sit near the floor's pitch. A sharp attack such as the tick's still leaks into the other band as a short click; `--threshold 0.3` sets the onset level to 30% of the band's peak instead of 10% and leaves the leak out. A pitch far up or down moves a cue towards the other's band. The gap warning's hum shows as a dominant pitch of 120 Hz, centred, in the windows it sounds alone; `--band 90 150` keeps it alone beside the pulses. The ledge warning's chord shows as a pitch of 770 to 1030 Hz (zero crossings of two tones at once), one onset every 0.2 s (two, evenly spaced, while both sides sound), its pan the voice's; the opponent's pulse reaches into its band through its octave, so with both near the floor the clearest check is a run with the position cues switched off in a scratch settings file.

The `verify` skill has the commands, the expected lines and the match drive scripts.
