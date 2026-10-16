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
- **Alternating voices.** Two pulsed voices of one interval can be paired when the second is created (`alternates_with`). One of the pair starting from silence while the other sounds waits for the midpoint between the other's pulses, so the two alternate instead of landing together; started on the same tick, the one created later usually waits half an interval. The audio thread places it, so it holds whatever block size the device asks for.
- **Leave room for the ramp.** A short click should start with about 12 ms of silence, so the start ramp does not eat its attack (`lead_in` in `cue_sounds.hpp`, the Star Fox 64 fork's finding).
- **Threads.** The game thread writes, the audio thread reads; every shared value is an atomic and nothing is locked. Voices are added and never removed while the engine runs, so the audio thread can walk them without a lock.

## Sounds

Each cue's sound is made by code when the engine starts, as a `Sound` buffer: `cue_sounds.cpp` reads one from any miniaudio data source (a waveform, noise, later a decoded file) and shapes its edges. A sound from a file fills the same kind of buffer, so changing a cue to a file changes its definition, not the engine.

miniaudio 0.11.25 is downloaded at configure and compiled once into its own library (`a11y_miniaudio` in `a11y.cmake`), without its device code: SDL plays the mix. The engine renders the voices itself, since the per-sample glides and pulse timing need it; miniaudio supplies the generators and, later, file decoding. Keep it at 0.11.25 or later should a cue ever play through miniaudio's own sounds: 0.11.21 crackled on every pitch change (ADR-0005).

## Lifecycle and switches

- `CueSystem` (`cue_system.cpp`) holds the engine, its output, the cue settings with their file and the match cues, and opens and closes them together; the hooks in `hooks.cpp` only forward to it.
- The engine opens in `pc_a11y_init`, as the game starts: opening the stream takes part of a frame, too much for first use mid-game. It closes first thing in `pc_a11y_shutdown`.
- Opening early moves the process's heap layout. That once made a base port bug much likelier: the file cache took heap buffers below 0x01000000 for ARAM offsets and cached garbled archives (fixed in `file_cache.cpp`, see `store_host`). If a crash rate rises only with cues on, suspect another place where a low host address is taken for ARAM.
- Every voice stops, with its ramp, when a scene is entered (`pc_a11y_scene_entered`): cues belong to the screen they play on.
- `MELEE_A11Y_CUES`: unset follows `MELEE_A11Y`; `1` opens the engine with speech off, for agent runs; `0` keeps it closed.
- If the engine cannot open, the log says why, speech says "Audio cues could not start." and the game goes on without cues.
- Master volume comes from the `pc_a11y_master_volume` hook in `pc_audio_set_volume`, the launcher's volume with mute applied, kept even before the engine opens. The second stream does not see the game's gain, so the engine applies it itself. The game's sound effect and music sliders do not affect cues.
- Online, the engine needs no `net_sfx_private`: it never reaches the game's sounds. The hooks that drive cues stay silent while `pc_net_resim()` is true, so voices keep their last targets through a rollback.

## Cue settings

### The model

Cue settings are one model of named values (`ValueModel` in `value_model.hpp`, each value a `NamedValue`), with a text file as its first front end and the port menu meant as the second. Each value has a key (`cues.volume`), a short label for a menu row, a one-line description, a unit, a default and a range; a switch is a value of 0 or 1. That is enough to draw a switch or a slider for every value, and for the file to explain itself. Values sit in groups, one for all cues and one per cue, and a group's title heads it in the file. The code says value rather than setting throughout, since `CONTEXT.md` keeps "setting" for the port's own options.

The model knows nothing about any cue. A feature registers its own values and keeps the typed handles it gets back (`Switch`, `Number`), which read the current value without looking up a key, so reading them every tick costs nothing. The model is fork-wide in principle: steering's values could join it later.

### The cues' values

`cue_settings.hpp` registers the values for all cues (a switch and the cue volume, 0 to 100%) and gives every cue the same three of its own through `add_cue`: a switch, its volume and its boost (a factor of 1 to 8 under the volume, for a short click that reads quieter than a held tone). `add_cue` also hands out the cue's number in the engine. The cue then adds its mapping values to the group it got back:

```cpp
a11y::CueGroup cue = cue_settings.add_cue("your_position", "Your position cue", 2.0f);
a11y::Number edge_pan = cue.group.add_number({"edge_pan", "Edge pan",
    "Pan at the main stage's edges.", "", 0.6f, 0.0f, 1.0f, 0.05f});
```

Every cue registers before the file is read, so a value the file lacks can be added to it. These values feed the volume chain: the all-cues switch and cue volume become the mix level, each cue's switch, volume and boost its cue level. A switch turned off is a level turned off: the engine glides that cue to silence while its voices keep running underneath, and turning it back on brings them back where they are.

### The file

- **Where.** `a11y-cues.ini` in `SDL_GetPrefPath(NULL, "melee-pc")`, the folder where the base port keeps its own settings (`%APPDATA%\melee-pc` on Windows). `MELEE_A11Y_CUE_SETTINGS=<path>` names another file; set but empty, it means no file and the defaults, so a run that meant a scratch file and lost its path never falls back to the player's. `drive.py` and `drive_net.py` set it to a file of each run's own, removed before the run, so no agent run reads or writes the maintainer's file.
- **What it looks like.** Plain text, one `key = value` a line, each value under its group's `#` heading and right below its description, which ends with the range and the default. A switch is `on` or `off` (also yes, no, true, false, 1, 0); a number may carry its unit (`80%`) and a decimal comma. `#` starts a comment anywhere on a line.
- **A missing file** is written with every value at its default, so there is something to edit. A value the code has and the file lacks, as after an update that adds a cue, takes its default and is appended to the end of the file under its heading, leaving everything the player wrote as it was. A key the file has and the code does not is logged once and left in the file; the model keeps it too, for a front end that rewrites the whole file.
- **Errors.** A line that cannot be read keeps its value's last setting, is logged and is spoken as one announcement ("Cue settings, lines 3 and 7 not understood."); the other lines apply. A value out of range is clamped and logged, not spoken.

### Live reload

The file is read once as the engine opens, then `poll` looks at it about twice a second from scene entry and, in a match, from the match tick. Only a file whose time or size changed is read, only a text that changed is parsed, and only values that changed are applied and logged. Deleting the file brings every value back to its default and writes the file afresh. A line removed from the file goes back to its default too.

The spoken error is queued, and speech right at startup or at a scene change can cut it off: the launcher and the screens that speak their opening on their first frame interrupt. `ValueFile::problem` keeps the last read's error so a later moment, such as a match starting, can say it again.

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

`pc_a11y_match_tick(held_mask)` runs right after each simulation tick, beside the replay recorder (`gameplay-state.md`, "The tick hook"), in every scene that ticks. `CueSystem::tick` reads the snapshot there, handing it what the cues ask of it (the ledge warning's edge offset), and passes it to the cues; the snapshot's casts are safe only there (ADR-0006). While a rollback re-runs ticks it does nothing, so the voices keep their last targets and pick up from the next tick played for the first time. In a match it also polls the cue settings file, and on the first tick of a match it says the settings file's last error again, if there is one: speech at a scene change may have cut it off, and a match is where the player listens.

### The match snapshot

`A11yMatchState` (`game_access.h`), filled in C by `a11y_game_match_state` because the fighter's struct has bitfields laid out as the game lays them out. It only reads.

- **Running**: a match is on with the local player's fighter on the live fighter list. Outside one, nothing else is filled and nothing sounds.
- **You**: the player character select follows. Offline the human slot on controller 1, else the first human slot, as when controller 2 started a single-player mode; online the human slot on `pc_net_local_player()`'s port. The slot's main fighter: Nana and the sleeping Zelda or Sheik never count.
- **Each fighter**: position at the feet, ground or air, out (knocked out or between death and respawn, motion states 0 to 11), whether its input is on (off before GO), its slot and team.
- **The opponent**: the nearest other slot's main fighter in play, not on your team in a team match, by straight-line distance, chosen afresh each tick. A list of one, so more can come.
- **Paused**: the game holds the match still on purpose: a pause, the GAME! freeze, a score tally. It comes from the game's pause word, not from the held mask, which a Training pause leaves clear and slow motion sets on its skipped ticks (`gameplay-state.md`, "Pause, freezes and slow motion"). The held mask is kept as `fighters_held`.
- **The floor below you**: on the ground the floor line under your feet; in the air the first floor the game's own downward cast (`mpCheckFloor`) meets from just above the bottom of your collision box down to the bottom blast zone, platforms included, with its line and height. None in the air is a gap. Every line id is checked against the line count before it goes into the snapshot. The cast is safe between ticks and starts at the collision box rather than the feet, which in the air sink below a floor just before landing (`gameplay-state.md`, "Is there ground below?").
- **Its ends**: the floor's surface followed to its left and right ends (`mpFloorGetLeft`, `mpFloorGetRight`), each deadly or not. An end is deadly when no wall rises from it and the same downward cast, from just above the end's height and the edge offset (`ledge_warning.edge_offset`) outwards, meets no floor. The offset clears the unit a floor reaches past its end where another line joins it, and stays under the 10.8 units from a Battlefield side platform's end to the main floor's, so the platforms' ends stay safe. The wall test is for a floor that ends at a step up, as against Princess Peach's Castle's tower, where no walk leaves the floor and a cast past the end would start inside the stage (`gameplay-state.md`, "How far to the edge?").
- **The stage frame**: the main floor's ends and height, the blast zones and the camera box, read each tick since some stages move them. The main floor is the lowest floor line across the middle of the blast zones, a solid one before a platform, followed to its surface's ends; the box around every floor would do on most tournament stages but not on Yoshi's Story (`gameplay-state.md`, "The tournament stages' frames").

### Drivers

A driver is a pure function from the snapshot and its cue's mapping values to its voice's targets, or to none when the cue is silent (`position_cue.hpp`, `ledge_warning.hpp`); the gap warning's keeps one count beside, its hold (`gap_warning.hpp`). The cues about you share one test of whether you are in play, `you_in_play` (`match_state.hpp`): a match running, not paused, past GO, and you neither knocked out nor waiting to respawn. Tests feed the drivers hand-written snapshots (`test_position_cue.cpp`, `test_gap_warning.cpp`, `test_ledge_warning.cpp`). `MatchCues` (`match_cues.cpp`) owns the rest: it registers each cue's values and voices once, as the cue settings open, and each tick starts a voice that has targets (a start while playing only changes them), and stops one that has none. Its own tests (`test_match_cues.cpp`) render the voices into a buffer.

The two position cues share the stage mapping (`StageMapping`, `stage_targets`): pan and pitch from a fighter's place, the same code for both, each cue with its own copy of the values in its own group of the file.

So pausing stops a cue and unpausing starts it again: a stop is the engine's 5 ms ramp, and a pulsed voice started from silence pulses at once (one alternating with a voice still sounding at the midpoint, above), which marks the match going on. The log shows both. Each tick of a running match is also a trace tick, numbered on from the first match's first tick, menu ticks left out.

### Your position cue

A tick whose pan says where you are across the stage, whose pitch says how high, and whose speed says how near the opponent is.

- **Pan**, piecewise linear in x: the main floor's centre 0, its ends ±`edge_pan`, the side blast zones ±1, held there beyond.
- **Pitch**, piecewise linear in octaves of y: the main floor 1, the camera box's top `up_range` octaves up, the bottom blast zone `down_range` octaves down, held beyond both.
- **Interval**: from `slow_interval` at `far_distance` and beyond to `fast_interval` at `near_distance` and closer, interpolated in the log of the interval, so each unit of distance closed speeds the tick by the same ratio. With no opponent, the slow interval.
- **Silent** outside a match, while paused, before GO and while you are out.
- **The sound**: the Star Fox 64 fork's aim click, a short decaying sine after 12 ms of lead-in.
- **Its limits go together.** The tick must end inside the fastest pulse even an octave down, where it plays twice as long: 32 ms there is 64 ms, so the fast interval goes no lower than 0.07 s and the down range no wider than 1 octave. A wider down range or a faster pulse needs a shorter tick first (`MatchCues::kFastestInterval`, `kWidestDownRange`).

### The opponent's position cue

A rounder, lower pulse on the same stage mapping, placed where the opponent is, at a steady pace.

- **Pan and pitch** as your cue's, from the opponent's position, with `opponent_position.edge_pan`, `up_range` and `down_range` of its own, starting at your cue's defaults.
- **Interval**: fixed, `opponent_position.interval`. How near the opponent is stays your cue's job.
- **Which opponent** is the snapshot's: the nearest one in play, chosen afresh each tick. When another fighter becomes the nearest, the voice moves there without restarting, so the pulse keeps its rhythm.
- **Silent** outside a match, while paused, before GO, with no opponent, and while the opponent is out; it sounds again on the respawn platform. While you are out it plays on.
- **The sound**: 12 ms of lead-in, then a 400 Hz sine with a quarter of its octave, rising over 5 ms and dying away over 70 ms. It must end inside its fastest interval an octave down: 82 ms there is 164 ms, so the interval goes no lower than 0.2 s (`MatchCues::kOpponentFastestInterval`).
- **The two rhythms.** Far apart, your cue's slow interval and the opponent's start together at GO; while one is a whole multiple of the other they stay in step, the opponent's pulses landing on your ticks. Whether that reads as one rhythm is the play test's question (`.scratch/match-cues/spec.md`, open questions).

### The gap warning

A held, centred hum while you are in the air with nothing below you, so that falling straight down would take you past the bottom blast zone.

- **The test** is the snapshot's floor below you: in the air, in play, and the cast met no floor. Platforms count as floor, so the warning stays off over Battlefield's platforms and anywhere above the main floor, and comes on past the main floor's ends (about a unit past them, where the cast stops meeting the floor) and under the stage.
- **The hold**: leaving a gap, the hum sounds on for `gap_warning.hold` ticks, so drifting back and forth over an edge does not stutter. A gap within the hold starts it afresh.
- **Silent** outside a match, while paused, before GO and while you are out, at once and without the hold; on the respawn platform the floor is below, so it stays off.
- **The sound**: 120 Hz with its third and fifth harmonics at a third and a fifth of its level, the start of a square wave, looped. It is the only held cue and the lowest: the opponent's pulse goes no lower than 200 Hz and your tick no lower than 750 Hz at the widest down range, and both pulse. The harmonics, at 360 and 600 Hz, let small speakers play it. Constant pitch and loudness, centred: the hum says only that there is a gap.
- **The game's own warning**, sound 96 as a fighter falls past the midpoint between the camera's bottom and the bottom blast zone, plays as before; the gap warning starts much earlier, as soon as nothing is below.
- **Hanging on a ledge** is in the air with nothing below (measured: Link hanging from Final Destination's left ledge at x = -88, y = -18.8), so the hum sounds through the hang. The play test kept it: you are still in danger there.

### The ledge warning

A pulsed chord on the left or right while you are near a deadly end of the floor below you, on the ground and in the air, so an edge is heard coming when you run, jump or are knocked across the stage.

- **Which ends**: the snapshot's floor below you and its ends (above). Only a deadly end sounds: a platform's end over the main floor stays silent, and so does a floor's end at a wall rising from it. With no floor below there is nothing to say; past the edge the gap warning has taken over.
- **Closeness**: the horizontal distance from you to a deadly end, held at 0 past it (in the air a unit past an end the cast still meets the floor). Within `ledge_warning.distance` a voice sounds on that side, `ledge left` or `ledge right`; both when both ends of a small floor are near. The two voices alternate (above, "Alternating voices"): with the same chord at the same rate, pulses landing together would blend into one sound in the middle.
- **Mapping**, the Star Fox 64 fork's beside chord: pan fully to the side at the warning distance, moving towards the centre as you close in and stopping at `pan_floor` at the edge, short of the centre; gain rising from `level_floor` at the warning distance to full at the edge. Both linear in the distance.
- **Handover**: walking off an edge, the floor below vanishes a unit past the end, so the ledge warning stops at its edge's pan and full gain on the same tick the gap warning starts. Coming back, the ledge warning starts again near the edge as soon as the floor is below, while the gap warning's hold runs out. Through a ledge hang nothing is below, so only the gap warning sounds.
- **Silent** outside a match, while paused, before GO, while you are out, and with no floor below. It has no hold: the distance changes smoothly, and the warning distance is the only place it starts or stops on the floor.
- **The sound**: 660 and 880 Hz together, a just perfect fourth as in the Star Fox 64 fork's beside chord, 90 ms rising over 5 ms and easing off after 12 ms of lead-in, pulsed every 0.2 s (`kLedgeInterval`), at a fixed pitch. A pulse gives the ear an onset to place, which a held tone panned by level alone does not; a chord sets it apart from the other cues' single tones, and its register sits between the opponent's 400 Hz pulse and your 1500 Hz tick on the floor, where ledges are.

## Checking a cue without hearing it

The agent cannot hear a cue, so the engine keeps three records, the last two off unless asked for:

- **Log lines**: each voice's start and stop with its targets at that moment, and the engine's opening. Per-tick changes are left out, so the log tells when a cue sounds, not how.
- **The trace** (`MELEE_A11Y_CUE_TRACE`): every voice's targets every tick, for checking a mapping by numbers against what the game did.
- **The dump** (`MELEE_A11Y_CUE_DUMP`): the cue mix itself, from the audio thread. SDL's dummy driver, which every agent run uses, still pulls the mix in real time, so a silent run records a true dump.

`tools/a11y/cue_dump.py` turns a dump into what an ear would notice, window by window: level, the left-right balance as a pan, pulse onsets with their intervals, and the dominant pitch. Its measures are simple ones (onsets by a level threshold, pitch by zero crossings), so two cues sounding at once blur pan and pitch, and a pulse that starts while another cue sounds is no onset at all.

`--band LOW HIGH` keeps one cue's register before the analysis, a steep band-pass on both channels, so two cues at once can be checked one at a time: the opponent's pulse in 150 to 1000 Hz, your tick in 1100 to 12000 Hz, while both sit near the floor's pitch. A sharp attack such as the tick's still leaks into the other band as a short click; `--threshold 0.3` sets the onset level to 30% of the band's peak instead of 10% and leaves the leak out. A pitch far up or down moves a cue towards the other's band. The gap warning's hum shows as a dominant pitch of 120 Hz, centred, in the windows it sounds alone; `--band 90 150` keeps it alone beside the pulses. The ledge warning's chord shows as a pitch of 770 to 1030 Hz (zero crossings of two tones at once), one onset every 0.2 s (two, evenly spaced, while both sides sound), its pan the voice's; the opponent's pulse reaches into its band through its octave, so with both near the floor the clearest check is a run with the position cues switched off in a scratch settings file.

The `verify` skill has the commands, the expected lines and the match drive scripts.
