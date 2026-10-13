# Cues

## Mental model

A **cue** is a sound the fork plays to say something about the game without speech. Cues play through the **cue engine** (`src/pc/a11y/cue_engine.cpp`, decision in ADR-0005), which has an output of its own:

- **Two streams.** The game plays its audio through its own SDL audio stream (`src/pc/audio.c`). The cue engine opens a second SDL stream on the same default playback device (`cue_output.cpp`), and SDL mixes the two. Nothing the cue engine does touches the game's voices, its mixer, its lock or its netplay sound handles.
- **Voices with targets.** A cue owns one or more **voices**. A voice is created once with a **sound** (a mono buffer of samples at 48 kHz) and a **mode**: looped, playing the sound over and over without a seam, or pulsed, playing it once every interval. Each tick the game thread writes the voice's **targets**: pan, pitch (playback rate), gain and, for a pulsed voice, the interval. The audio thread glides each voice towards its targets over a few milliseconds, so a step once a tick is heard as a smooth change, not a zipper noise.
- **Pulses on the audio clock.** A pulsed voice is restarted by the audio thread itself, each pulse one interval after the last, at the interval current then. The game thread never starts single pulses, so a rhythm stays even, and a faster interval is heard from the next pulse.
- **The volume chain.** A voice's loudness is master volume × mix level (cue volume and the all-cues switch) × its cue's level (switch, volume, boost) × its own gain target. The mix then has fixed headroom and a soft limit, so five voices at full gain together still stay under full scale.

```
game thread                       audio thread (SDL's)
-----------                       --------------------
start / stop / set_targets  --->  voices glide to their targets,
master, mix and cue levels  --->  pulses retriggered on the audio clock
                                  --> headroom, soft limit --> SDL stream --> device
                                                           \-> dump file
```

## The engine's rules

- **Starts and stops ramp.** Every start and stop goes through the engine's own 5 ms gain ramp, so a sound cut at full level never clicks. A voice stopped and started again before its ramp ends ramps back up from where it was, without restarting.
- **Constant-power pan.** Left and right gains are the cosine and sine of the pan angle, so a sound keeps its loudness as it moves across. miniaudio's own pan is louder at the sides, which is why the engine pans by itself.
- **Pitch by playback rate**, with 4-point interpolation, so a pulse's attack stays sharp. A pitch shifter would smear it (the Star Fox 64 fork pinned its aim click to playback rate for that reason).
- **A pulse must fit.** A pulsed voice's sound must end inside its fastest interval even at its lowest pitch, where it plays longest, or every pulse would be cut by the next. The engine refuses such a voice when it is created, with a log line, and holds every interval and pitch target to those limits.
- **Leave room for the ramp.** A short click should start with about 12 ms of silence, so the start ramp does not eat its attack (`lead_in` in `cue_sounds.hpp`, the Star Fox 64 fork's finding).
- **Threads.** The game thread writes, the audio thread reads; every shared value is an atomic and nothing is locked. Voices are added and never removed while the engine runs, so the audio thread can walk them without a lock.

## Sounds

Each cue's sound is made by code when the engine starts, as a `Sound` buffer: `cue_sounds.cpp` reads one from any miniaudio data source (a waveform, noise, later a decoded file) and shapes its edges. A sound from a file fills the same kind of buffer, so changing a cue to a file changes its definition, not the engine.

miniaudio 0.11.25 is downloaded at configure and compiled once into its own library (`a11y_miniaudio` in `a11y.cmake`), without its device code: SDL plays the mix. The engine renders the voices itself, since the per-sample glides and pulse timing need it; miniaudio supplies the generators and, later, file decoding. Its version must stay 0.11.25 or later: 0.11.21 crackled on every pitch change (ADR-0005).

## Lifecycle and switches

- The engine opens in `pc_a11y_init`, as the game starts: opening the stream takes part of a frame, too much for first use mid-game. It closes first thing in `pc_a11y_shutdown`.
- Opening early moves the process's heap layout. That once made a base port bug much likelier: the file cache took heap buffers below 0x01000000 for ARAM offsets and cached garbled archives (fixed in `file_cache.cpp`, see `store_host`). If a crash rate rises only with cues on, suspect another place where a low host address is taken for ARAM.
- Every voice stops, with its ramp, when a scene is entered (`pc_a11y_scene_entered`): cues belong to the screen they play on.
- `MELEE_A11Y_CUES`: unset follows `MELEE_A11Y`; `1` opens the engine with speech off, for agent runs; `0` keeps it closed.
- If the engine cannot open, the log says why ("cue engine could not start: ..."), speech says "Audio cues could not start." and the game goes on without cues.
- Master volume comes from the `pc_a11y_master_volume` hook in `pc_audio_set_volume`, the launcher's volume with mute applied, kept even before the engine opens. The second stream does not see the game's gain, so the engine applies it itself. The game's sound effect and music sliders do not affect cues.
- Online, the engine needs no `net_sfx_private`: it never reaches the game's sounds. The hooks that drive cues stay silent while `pc_net_resim()` is true, so voices keep their last targets through a rollback.

## Cue settings

### The model

Cue settings are one model of named values (`settings.hpp`), with a text file as its first front end and the port menu meant as the second. Each value has a key (`cues.volume`), a short label for a menu row, a one-line description, a unit, a default and a range; a switch is a value of 0 or 1. That is enough to draw a switch or a slider for every value, and for the file to explain itself. Values sit in groups, one for all cues and one per cue, and a group's title heads it in the file.

The model knows nothing about any cue. A feature registers its own values and keeps the typed handles it gets back (`Switch`, `Number`), which read the current value without looking up a key, so reading them every tick costs nothing. The model is fork-wide in principle: steering's values could join it later.

### The cues' values

`cue_settings.hpp` registers the values for all cues (a switch and the cue volume, 0 to 100%) and gives every cue the same three of its own through `add_cue`: a switch, its volume and its boost (a factor of 1 to 8 under the volume, for a short click that reads quieter than a held tone). `add_cue` also hands out the cue's number in the engine. The cue then adds its mapping values to the group it got back:

```cpp
a11y::CueGroup cue = cue_settings.add_cue("your_position", "Your position cue", 1.5f);
a11y::Number edge_pan = cue.group.add_number({"edge_pan", "Edge pan",
    "Pan at the main stage's edges.", "", 0.7f, 0.0f, 1.0f, 0.05f});
```

Every cue registers in `open_cue_settings` (`hooks.cpp`), before the file is read. These values feed the volume chain: the all-cues switch and cue volume become the mix level, each cue's switch, volume and boost its cue level. A switch turned off is a level turned off: the engine glides that cue to silence while its voices keep running underneath, and turning it back on brings them back where they are.

### The file

- **Where.** `a11y-cues.ini` in `SDL_GetPrefPath(NULL, "melee-pc")`, the folder where the base port keeps its own settings (`%APPDATA%\melee-pc` on Windows). `MELEE_A11Y_CUE_SETTINGS=<path>` names another file. `drive.py` and `drive_net.py` set it to a file of each run's own, removed before the run, so no agent run reads or writes the maintainer's file.
- **What it looks like.** Plain text, one `key = value` a line, each value under its group's `#` heading and right below its description, which ends with the range and the default. A switch is `on` or `off` (also yes, no, true, false, 1, 0); a number may carry its unit (`80%`) and a decimal comma. `#` starts a comment anywhere on a line.
- **A missing file** is written with every value at its default, so there is something to edit. A value the code has and the file lacks, as after an update that adds a cue, takes its default and is appended to the end of the file under its heading, leaving everything the player wrote as it was. A key the file has and the code does not is logged once and left in the file; the model keeps it too, for a front end that rewrites the whole file.
- **Errors.** A line that cannot be read keeps its value's last setting, is logged and is spoken as one announcement ("Cue settings, lines 3 and 7 not understood."); the other lines apply. A value out of range is clamped and logged, not spoken.

### Live reload

The file is read once as the engine opens, then `poll` looks at it about twice a second from scene entry and, in a match, from the match tick. Only a file whose time or size changed is read, only a text that changed is parsed, and only values that changed are applied and logged. Deleting the file brings every value back to its default and writes the file afresh. A line removed from the file goes back to its default too.

The spoken error is queued, and speech right at startup or at a scene change can cut it off: the launcher and the screens that speak their opening on their first frame interrupt. `SettingsFile::problem` keeps the last read's error so a later moment, such as a match starting, can say it again.

## The match cues

The cues of a match follow one idea: once a tick, read the match into a snapshot, hand the snapshot to each cue's driver, and give the voices the targets the drivers return. Nothing in a match calls the cues; they poll, as the menu readers do (ADR-0003).

```
gm_RunSimTick --pc_a11y_match_tick--> a11y_game_match_state --> A11yMatchState
                                                                   |
     cue settings' values -------------------> drivers (pure) <---+
                                                  |
                                                  v
                                 MatchCues: start, set targets or stop each voice
```

### The tick hook

`pc_a11y_match_tick(held_mask)` runs right after each simulation tick, beside the replay recorder (`gameplay-state.md`, "The tick hook"), in every scene that ticks. While a rollback re-runs ticks it does nothing, so the voices keep their last targets and pick up from the next tick played for the first time. In a match it also polls the cue settings file, and on the first tick of a match it says the settings file's last error again, if there is one: speech at a scene change may have cut it off, and a match is where the player listens.

### The match snapshot

`A11yMatchState` (`game_access.h`), filled in C by `a11y_game_match_state` because the fighter's struct has bitfields laid out as the game lays them out. It only reads.

- **Running**: a match is on with the local player's fighter on the live fighter list. Outside one, nothing else is filled and nothing sounds.
- **You**: the player character select follows. Offline the human slot on controller 1, else the first human slot, as when controller 2 started a single-player mode; online the human slot on `pc_net_local_player()`'s port. The slot's main fighter: Nana and the sleeping Zelda or Sheik never count.
- **Each fighter**: position at the feet, ground or air, out (knocked out or between death and respawn, motion states 0 to 11), whether its input is on (off before GO), its slot and team.
- **The opponent**: the nearest other slot's main fighter in play, not on your team in a team match, by straight-line distance, chosen afresh each tick. A list of one, so more can come.
- **Paused**: the game holds the match still on purpose: a pause, the GAME! freeze, a score tally. It comes from the game's pause word, not from the held mask, which a Training pause leaves clear and slow motion sets on its skipped ticks (`gameplay-state.md`, "Pause, freezes and slow motion"). The held mask is kept as `fighters_held`.
- **The stage frame**: the main floor's ends and height, the blast zones and the camera box, read each tick since some stages move them. The main floor is the lowest floor line across the middle of the blast zones, a solid one before a platform, followed to its surface's ends; the box around every floor would do on most tournament stages but not on Yoshi's Story (`gameplay-state.md`, "The tournament stages' frames").

### Drivers

A driver is a pure function from the snapshot and its cue's mapping values to its voice's targets, or to none when the cue is silent (`position_cue.hpp`). Tests feed it hand-written snapshots (`test_position_cue.cpp`). `MatchCues` (`match_cues.cpp`) owns the rest: it registers each cue's values and voices once, as the cue settings open, and each tick starts a voice that has targets (a start while playing only changes them), and stops one that has none.

So pausing stops a cue and unpausing starts it again: a stop is the engine's 5 ms ramp, and a pulsed voice started from silence pulses at once, which marks the match going on. The log shows both. Each tick of a running match is also a trace tick, numbered on from the first match's first tick, menu ticks left out.

### Your position cue

A tick whose pan says where you are across the stage, whose pitch says how high, and whose speed says how near the opponent is.

- **Pan**, piecewise linear in x: the main floor's centre 0, its ends ±`edge_pan` (0.7), the side blast zones ±1, held there beyond.
- **Pitch**, piecewise linear in octaves of y: the main floor 1, the camera box's top `up_range` octaves up (1), the bottom blast zone `down_range` octaves down (1), held beyond both.
- **Interval**: from `slow_interval` (0.8 s) at `far_distance` (120 units) and beyond to `fast_interval` (0.08 s) at `near_distance` (10 units) and closer, interpolated in the log of the interval, so each unit of distance closed speeds the tick by the same ratio. With no opponent, the slow interval.
- **Silent** outside a match, while paused, before GO and while you are out.
- **The sound**: the Star Fox 64 fork's aim click, a 1500 Hz sine decaying over 20 ms after 12 ms of lead-in. Its boost starts at 1.5.
- **Its limits go together.** The tick must end inside the fastest pulse even an octave down, where it plays twice as long: 32 ms there is 64 ms, so the fast interval goes no lower than 0.07 s and the down range no wider than 1 octave. A wider down range or a faster pulse needs a shorter tick first (`MatchCues::kFastestInterval`, `kWidestDownRange`).

## Checking a cue without hearing it

Three records, all off unless asked for:

- **Log lines.** Each voice's start and stop is an `[a11y] cue <name> start|stop` line with its targets at that moment, and the engine's opening is an `[a11y] cue engine:` line with the SDL driver, the device's rate and its buffer size.
- **The trace.** `MELEE_A11Y_CUE_TRACE=<file>` writes, each tick, one tab-separated line per voice: tick, voice, on or off, pan, pitch, gain and interval (`-` for a looped voice). It checks a mapping by numbers.
- **The dump.** `MELEE_A11Y_CUE_DUMP=<file>` writes the cue mix as raw 32-bit float stereo at 48 kHz, from the audio thread, from the moment the engine opens. SDL's dummy driver (`SDL_AUDIO_DRIVER=dummy`, used by every agent run) still pulls the mix in real time, so a silent run records a true dump.

`tools/a11y/cue_dump.py` reads a dump and reports, per window: level and peak in dBFS, the left-right balance as the pan that would give it, the pulses starting in the window with their intervals, and the dominant pitch:

```
python tools/a11y/cue_dump.py build/drive/cues.f32 --window 0.5 --onsets
   time   level    peak    pan  onsets  interval s (min-max)    pitch Hz
   0.00   -22.2    -8.6  +0.50       2  0.250 (0.250-0.250)      1000
```

Its measures are simple ones (onsets by a level threshold after 5 ms of quiet, pitch by zero crossings), so two cues sounding at once blur pan and pitch: check one cue at a time, or windows where only one sounds. `--start` and `--end` narrow a long dump; it runs on the standard library alone, about a second per ten seconds of dump.

`tools/a11y/match_position.drive` walks your position cue through a Training match on Final Destination; run it with the trace and the dump, as its header says, and the `verify` skill says what its log and dump show.

The engine's unit tests (`test_cue_engine.cpp`) render it into buffers with no device, and also write a dump of a known pulse train, which `tools/a11y/test_cue_dump.py` reads back through the script.
