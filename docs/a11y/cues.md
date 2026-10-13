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

The engine's unit tests (`test_cue_engine.cpp`) render it into buffers with no device, and also write a dump of a known pulse train, which `tools/a11y/test_cue_dump.py` reads back through the script.
