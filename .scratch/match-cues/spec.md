# Match cues: the cue engine and the first four cues

Status: agreed 2026-10-13 (design conversation with the maintainer)

The maintainer chose the first four cues (your position, the opponent's position, the gap warning, the ledge warning), sounds made by code with files possible later, a volume chain of master, cue volume and each cue's own volume, a switch per cue, and a tuning file re-read while the game runs, shaped so the port menu can take over from it later. Every mapping and default below is a first proposal for the play test to judge; the tuning file exists so they can change without a rebuild.

## Goal

A blind player in a match hears, without speech, where they are on the stage, where the opponent is and how near, when nothing is below them, and when they are close to an edge they would fall from to their death. The cues play beside the game's own sounds and the screen reader, follow the master volume, can each be turned down or off, and leave online play compatible with base port builds.

How the cues play is ADR-0005 (the cue engine). What the game knows during a match and how to read it safely is `docs/a11y/gameplay-state.md`. Vocabulary is `CONTEXT.md`: cue, cue engine. Prior art is the Star Fox 64 fork's aim cue and obstacle cues (`C:\Users\ottoh\koodi\Starship`, `docs/accessibility-cues-tuning.md`).

## New terms

- **Position cue**: a pulsed sound whose pan and pitch say where a fighter is on the stage. Two of them: **your position cue** and **the opponent's position cue**.
- **Gap warning**: a sound that plays while nothing is below you.
- **Ledge warning**: a sound on the left or right that plays while you are near an end of the floor below you and nothing lies past that end.
- **Cue settings**: the values that switch, set the loudness of and tune the cues, as one model with a file behind it now and the port menu later.

Each becomes a `CONTEXT.md` entry when its slice lands.

## Scope

In:

- **The cue engine**: ADR-0005's second SDL audio stream with miniaudio, built for voices that keep playing and change every tick.
- **Cue settings** and the tuning file.
- **The four cues**, in every match: VS, Training, the single-player modes and online.
- **One opponent**: the nearest one. The design leaves room for more.

Out, noted for later:

- **A recovery cue**: how far you are from the ledge while off the stage and trying to get back. Its own design questions (grab range, which ledge counts by facing) wait until the first four have been heard.
- **Sound files** for the cues. Cues take their sound as a buffer, so a file can replace a generated sound later.
- **Preview** of a cue from the port menu, and the port menu's settings screen. The design keeps both cheap: a cue can be played from fixed targets with no match, and cue settings are a model the menu can render.
- **The opponent panned relative to you**, to try by ear after the play test.
- **More than one opponent**, and teammates.
- **Odd stages** where "main stage" needs more than the floors' extent: Poké Floats, Icicle Mountain, Big Blue, the scrolling Adventure stages.

## What the game gives us

From `docs/a11y/gameplay-state.md`; read it first. The parts this spec relies on:

- **The tick hook point.** `gm_RunSimTick` (`src/melee/gm/gmscene.c`) runs every GObj's update in `HSD_GObj_RunProcs()`, and the base port's replay recorder sits right after it (`pc_slp_tick_end`) with a bitmask of the GObj lists held still that tick. Code there sees the tick's finished state, outside the fighters' collision step. When the fighters' bit is set the fighters did not move: paused, frozen on GAME!, or a skipped tick of a slow-motion mode. The match scene is shared by VS, the single-player modes and Training, so one hook covers them all.
- **Positions**: `cur_pos` at the fighter's feet; `ground_or_air`; `motion_id`, where 0 to 11 are knocked out or between death and respawn.
- **The floor below**: `mpCheckFloor` cast straight down from just above a point finds the nearest floor, platforms included. Called between ticks it leaves no trace.
- **A floor's ends**: `mpFloorGetLeft` and `mpFloorGetRight` from a floor line follow the joined lines to the surface's outer ends. On the ground the line is `coll_data.floor.index`.
- **The stage's frame**: the floors' extent (`mpLib_80458868[1]`), the blast zones (`Stage_GetBlastZone*Offset`) and the camera box (`Stage_GetCamBounds*Offset`), all read fresh each tick because some stages move them.
- **Sizes**: Final Destination's main floor ends at about x = ±85.6, Battlefield's at ±68.4; a dash crosses Final Destination in over two seconds.

## Decisions

### The cue engine

ADR-0005 as accepted, shaped by what these cues need: every one is a voice that keeps sounding while its targets change every tick.

- **Voices.** A cue owns one or more voices. A voice is given a sound buffer and a way of playing it: looped, or pulsed with an interval. Each tick fork code sets its targets: pan, pitch, gain and, for a pulsed voice, the interval. Starting and stopping go through the fork's short gain ramp (ADR-0005).
- **Smoothing on the audio thread.** Targets arrive 60 times a second; applied as steps, a held tone would make a stepping, zipper-like noise. The audio thread glides each voice toward its targets over a few milliseconds.
- **Pulses on the audio clock.** A pulsed voice is retriggered by the audio thread itself, the next pulse placed one interval after the last, with the interval current at that moment (the Star Fox 64 fork's geiger counter). The game thread never starts single pulses, so a rhythm stays even and its speed can change smoothly. This is ADR-0005's "scheduling on the audio clock", done inside the voice rather than per sound.
- **Pan** is the fork's constant-power pan, so a sound keeps its loudness as it moves across. **Pitch** is playback-rate resampling, so a pulse's attack is not smeared (the Star Fox 64 fork pinned the aim click to it for that reason).
- **Sounds are buffers.** Each cue's sound is made by code at engine start, as mono samples. A file decoded later fills the same kind of buffer, so changing a cue to a file is a change to its definition, not to the engine. A pulsed sound must fit inside the fastest interval its cue allows, or every pulse would be cut by the next; the generator checks this.
- **Headroom.** Up to five voices can sound at once (your position, the opponent's, the gap, the ledge on each side). The cue mix has fixed headroom and a soft limit at its output, so several cues together never clip.
- **Distinct sounds.** The four cues differ in timbre and register, so two at once never blend into one.
- **Startup and failure.** The engine opens in `pc_a11y_init`, as ADR-0005 decided. If it cannot open, the log says why, speech says "Audio cues could not start.", and the game goes on without cues.
- **Threads.** The game thread only writes targets and commands; the audio thread only reads them. No lock is shared with the game's own audio.

### Cue settings

One model of named values, with the tuning file as its first front end. The port menu becomes a second front end over the same model, so nothing in the cues changes when it arrives.

- **Each value** has a key, a default, a range, a unit and a one-line description: enough for a menu to draw a slider or a switch for it, and for the file to explain itself.
- **Values**:
  - All cues: on or off; cue volume, 0 to 100%.
  - Each cue: on or off; its own volume, 0 to 100%; a loudness boost applied under the sliders (a short click sounds quieter than a held tone at the same level, as in the Star Fox 64 fork); and its mapping values, listed under each cue below.
- **The file** is plain text, one `key = value` a line, grouped by cue, with `#` comments. It lives in the folder where the base port keeps its own settings (`SDL_GetPrefPath`), and the environment variable `MELEE_A11Y_CUE_SETTINGS` names another path. Drivers point it at a scratch file, so agent runs never read or write the maintainer's.
- **A missing file** is written out with every value at its default and its description as a comment, so there is something to edit.
- **Live reload.** The file is checked about twice a second; changed values apply on the next tick. A line that cannot be read is logged and spoken ("Cue settings, line 12 not understood."); the other lines still apply and the bad one keeps its last value. A value out of range is clamped.
- **The runtime switch.** Cues are on by default. `MELEE_A11Y=0` turns them off with speech, and `MELEE_A11Y_CUES=1` turns them back on, the way `MELEE_A11Y_STEER` works for steering, so a verification run can have cues without speech.

### The volume chain

Each voice's loudness is master volume × cue volume × the cue's own volume × the cue's boost × the voice's own gain target.

- **Master volume and mute** come from a new hook, one line in `pc_audio_set_volume` (`src/pc/audio.c`), which every launcher change already goes through with mute applied. The hook keeps the last value even before the engine opens.
- **The game's sound effects and music sliders** do not affect cues; the cue volume is their separate control.

### When cues sound

- **In a match only.** Cues start on the first tick after the fighters can move, and stop with a ramp when the scene changes (`pc_a11y_scene_entered`).
- **Pause and GAME!** When the fighters' list has been held still for more than a few ticks, every voice fades out; it fades back in when the fighters move again. The grace keeps a slow-motion mode's skipped ticks from cutting the cues. If a direct pause flag turns up that tells pause from slow motion, the implementation uses it instead.
- **Knocked out.** While a fighter is knocked out or between death and respawn (`motion_id` 0 to 11), its position cue is silent, and so are the gap and ledge warnings if it is you. On the respawn platform the cues sound again.
- **Online.** The tick hook stays silent while `pc_net_resim()` is true, so voices simply keep their last targets through a rollback and pick up from the next real tick. Cues react to predicted ticks at once rather than waiting for confirmation: a misprediction corrects itself a tick or two later, which a continuous sound hides. Cues are on the fork's own stream, so `net_sfx_private` does not apply.

### Who is you, and who is the opponent

- **You** are the player character select follows: port 1 offline, `pc_net_local_player()` online. The fighter is the slot's main fighter, so Nana does not count.
- **The opponent** is the nearest other player's main fighter in play, not on your team in a team match, measured by straight-line distance. The choice is made each tick, so it changes when another fighter becomes nearer. With nobody in play (all knocked out, or a mode with no other fighter) there is no opponent.
- The design keeps "the opponent" a list of one, so more opponents can be added later.

### The stage frame

Pan and pitch need a frame for "where on the stage".

- **The main stage** is the floors' extent: its left and right edges and its top floor height. It is read each tick. On Battlefield and Final Destination it is the main floor's ends exactly, since no platform reaches further out; the other tournament stages are checked when the first slice is built.
- **The height reference** is the main floor's y: the lowest floor line across the stage's middle. For now, the floors' extent's bottom edge. Odd stages get a closer look later.

### Your position cue

A pulse whose pan says where you are across the stage, whose pitch says how high, and whose speed says how near the opponent is.

- **Pan**, piecewise linear in x: the main stage's centre is the centre; its edges sit at ±edge pan (default 0.7); the side blast zones are fully left and right. Off the stage the pan keeps moving, so you can still hear how far out you are.
- **Pitch**, piecewise linear in octaves of y: the main floor is the base pitch; the top of the camera box is up by the up range (default 1 octave); the bottom blast zone is down by the down range (default 1 octave).
- **Interval**: distance to the opponent, mapped on a log scale from the slow interval (default 0.8 s) at the far distance (default 120 units) and beyond, down to the fast interval (default 0.08 s) at the near distance (default 10 units). A log scale gives equal-sounding tempo steps per unit of distance closed, as the Star Fox 64 fork's obstacle cue found. With no opponent: the slow interval.
- **Sound**: a short tick, a damped sine of about 20 ms after about 12 ms of lead-in silence, so the start ramp does not eat its attack (the Star Fox 64 fork's aim click).
- **Settings**: edge pan, up range, down range, slow and fast intervals, far and near distances.

### The opponent's position cue

The same pan and pitch mapping applied to the opponent's position, with its own sound, at a steady pulse.

- **Interval**: fixed (default 0.4 s). Distance is already in your cue's speed.
- **Sound**: a different timbre and register from your tick, for example a lower, rounder pulse.
- **Silent** with no opponent.
- **Settings**: its interval, plus the same mapping values as your cue, set separately.

### The gap warning

A sound that plays while you are in the air and nothing is below you: falling straight down now would take you past the bottom blast zone.

- **The test**: each tick you are in the air, cast `mpCheckFloor` from just above your feet down to the bottom blast zone. No floor found, platforms included, means a gap.
- **Sound**: a held, centred tone, low and distinct from both position cues, at constant pitch and loudness. It starts and stops with the ramp; a short hold (a few ticks) on stopping keeps it from flickering at a platform's edge.
- **The game's own warning**, sound 96 near the bottom blast zone, stays as the game plays it.
- **Settings**: on or off, volume, boost, and the hold in ticks.

### The ledge warning

A sound on the left or right while you are near an end of the floor below you, and nothing lies past that end. It works on the ground and in the air, so you hear an edge coming when you jump or are knocked across the stage.

- **The floor below**: on the ground, the floor under your feet; in the air, the first floor straight below you, by the same downward cast as the gap warning. In the air over a gap there is none, and the gap warning has taken over.
- **Its ends**: `mpFloorGetLeft` and `mpFloorGetRight` from that floor line.
- **Deadly or not**: an end is deadly when a downward cast just past it, a few units outward, finds no floor. A platform end over the main stage is not deadly and stays silent.
- **Closeness**: the horizontal distance from you to a deadly end. Within the warning distance (default 30 units), a voice sounds on that side, two if both ends of a small floor are close.
- **Mapping**, as the Star Fox 64 fork's beside chord: pan is fully to that side at the warning distance and moves toward the centre as you get closer, stopping at the pan floor (default 0.3) at the edge, so the side stays clear; loudness rises from the level floor (default 30%) at the warning distance to full at the edge.
- **Sound**: a pulsed tone at a fixed rate (about 5 Hz), distinct from the other cues; a pulse gives the ear an onset to place, which a held tone panned by level alone does not.
- **Handover**: past a deadly edge the floor below changes or vanishes, so the ledge warning fades as the gap warning starts.
- **Settings**: on or off, volume, boost, warning distance, pan floor, level floor.

### How the fork attaches

- **One tick hook**, `pc_a11y_match_tick(held_mask)`, beside `pc_slp_tick_end` in `gm_RunSimTick`, declared in `a11y_hooks.h`. It polls (the idiom of ADR-0003), so the four cues need no other hook in the match.
- **The master volume hook** in `pc_audio_set_volume`.
- **A match snapshot** filled in C (`game_access.c`, compiled with the game's bitfield layout): you and the opponent (position, ground or air, knocked out or not, floor line on the ground), the stage frame, and the casts' results (floor below, its ends, deadly or not). The casts run in the hook, between ticks, which settles the primer's open question on raycasts for these cues: the fork calls the game's own cast there rather than walking the line arrays itself.
- **Drivers in C++**: one per cue, a pure function from the snapshot and the cue settings to the cue's voice targets.
- **The engine** takes those targets.

### Shared code

- **Cue settings** are fork-wide, not cue-specific: steering's values and other fork settings can join the model later.
- **The engine** is the one cue engine of ADR-0005; menu cues, if any come, use it too.

### The log, the dump and the trace

- **Log lines.** Each voice's start and stop is logged with an `[a11y] cue` prefix and its targets at that moment ("[a11y] cue ledge right start pan 0.92 gain 0.31"), plus settings reloads and their errors. Per-tick targets are not logged.
- **The trace.** `MELEE_A11Y_CUE_TRACE=<file>` writes every voice's targets every tick, for checking a mapping by numbers.
- **The dump.** `MELEE_A11Y_CUE_DUMP=<file>` writes the cue mix as raw 32-bit float stereo at 48 kHz, as the prototype did and as `MELEE_AUDIO_DUMP` is for the game's mix.
- **Analysis.** A script under `tools/a11y/` reads a dump and reports, per window, level, left-right balance, pulse onsets and intervals, and dominant pitch, so the agent can check a recording against the trace.

### Tests

- **Drivers**, against snapshots: pan and pitch at the stage's centre, edges and blast zones; interval at, between and beyond the near and far distances; no opponent; knocked out; the gap test; deadly and safe ends, one or both sides, ground and air. Prior art is `test_css_targets.cpp` and `test_sss_targets.cpp`.
- **Cue settings**: defaults written for a missing file, parsing, clamping, an unreadable line keeping its last value, a reload applying only what changed.
- **The engine**, rendered without a device into a buffer: constant-power pan keeps loudness; starts and stops have no sample-to-sample jump above a threshold; a pulsed voice's onsets are its interval apart, and an interval change takes effect from the next pulse; the volume chain multiplies; the soft limit holds five full voices under full scale; a pulse that does not fit its fastest interval is refused.
- **In the game**: a drive script into a Training match on Final Destination that walks to each edge, jumps off and comes back, checked through the log lines, the trace and the dump analysis. The `verify` skill gains this run, the new switches and the expected lines.
- **By ear**: each slice ends with a play test by the maintainer. Only that counts as "works".

### Docs

- A primer, `docs/a11y/cues.md`: the cue engine's mental model (two streams, voices with targets, pulses on the audio clock, the volume chain), cue settings and the file, and how to verify a cue by its dump.
- `CONTEXT.md`: position cue, gap warning, ledge warning, cue settings.
- `CLAUDE.md`'s accessibility status: one line per cue as it lands.
- ADR-0005 stays as accepted. Pulses inside the voice are its audio-clock scheduling done one way.

## Open questions

- **Pause versus slow motion.** Whether a direct pause flag exists that the tick hook can read; otherwise the held-still grace stands.
- **The height reference on stages without one main floor.** For the tournament stages the floors' extent is enough.
- **Whether the opponent's steady pulse should drift against yours.** Two pulse trains at related intervals can line up and sound like one rhythm; the play test will tell.

## Issues

Four slices, each ending with a play test by ear before the next starts. The first slice is three issues.

1. `issues/01-cue-engine.md`: miniaudio's build, the second stream, voices, ramps, pulses, the volume chain and master volume hook, the switch, the log, dump, trace and analysis script.
2. `issues/02-cue-settings.md`: the settings model, the tuning file and its live reload, the drivers' scratch settings. Blocked by 01.
3. `issues/03-your-position-cue.md`: the tick hook, the match snapshot, pause, and your position cue. Blocked by 01 and 02.
4. `issues/04-play-test-your-position.md`: the maintainer, by ear. Blocked by 03.
5. `issues/05-opponent-position-cue.md`: the opponent's position cue. Blocked by 04.
6. `issues/06-play-test-opponent.md`: by ear. Blocked by 05.
7. `issues/07-gap-warning.md`: the gap warning. Blocked by 06.
8. `issues/08-play-test-gap.md`: by ear. Blocked by 07.
9. `issues/09-ledge-warning.md`: the ledge warning. Blocked by 08.
10. `issues/10-play-test-ledge.md`: by ear, the four cues together. Blocked by 09.
11. `issues/11-play-test-online.md`: by ear, the four cues online, with a friend or a second device. Blocked by 10. Added after issue 06's play test: the play tests before it stay offline.

Docs are part of the issue they belong to.
