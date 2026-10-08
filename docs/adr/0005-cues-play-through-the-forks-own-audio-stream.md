# Cues play through the fork's own audio stream

Status: accepted (2026-10-08)

Cues need more than the game's sound path offers. The game can start any sound it has loaded with a volume and a pan, and change its pitch while it plays, but it has no filters, cannot synthesize a sound or load a file, and only plays sounds from the banks the current screen has loaded (Fox's sounds exist only in a match with Fox). So the fork has a cue engine of its own: an SDL audio stream opened by fork code on the default playback device beside the game's, which SDL mixes with the game's audio, filled by miniaudio used as a library without its own device code. miniaudio synthesizes tones and noise, decodes WAV, FLAC and MP3 files, and changes pan, pitch and filtering per sound. The fork adds what miniaudio does not do well: a constant-power pan, short gain ramps on every start and stop, and scheduling on the audio clock when a rhythm must stay even. The game's own sound path stays available for a cue that should be a game sound, where pan and pitch are enough.

A prototype on the local branch `prototype/audio-cues` answered the question, measured from a recording of the cue mix and play-tested by ear with NVDA over WASAPI: the two streams mix cleanly with the game's music and the screen reader, and nothing in the base port changes.

## Considered options

- **The game's sound path only** (`lbAudioAx_800237A8` and the base port's mixer in `src/pc/audio.c`): no new library, and the game's master volume and reverb apply. But it lacks filters, synthesis and files, is limited to loaded banks, takes voices from the game's pool of 64 in a busy match, and online every cue must be kept out of the netplay sound handle count.
- **A hook in the base port's mixer**, as the base port mixes its custom music (`pc_music_stream_mix` in `render_frame`): one output, with the game's volume and clipping applied for free. But the hook sits in a busy base port file, the mix runs at the game's 32 kHz, and cue rendering would share the game's audio lock.
- **miniaudio with its own device**, as the Star Fox 64 fork does: proven there, but a second audio backend in a process that already has SDL. The operating system mixes the two either way.
- **SDL3_mixer or SoLoud**: SDL3_mixer covers pan, pitch and files but has no filters; SoLoud has filters and a synthesizer but is barely maintained. Neither does more than miniaudio for this.
- **Writing the mixing and effects ourselves**: the effects themselves are small, but miniaudio brings file decoding, filters and oscillators already tested.

## Consequences

- miniaudio must be 0.11.25 or later. In 0.11.21 every pitch change made a small jump in the waveform, heard as crackle in a pitch sweep; 0.11.24 fixed it.
- Every start and stop goes through the fork's own short gain ramp. A sound cut at full level clicks, and so can a sound started at the beginning of its wave when it is reused with a pitch change, because the pitch shifter still holds the end of its previous play. miniaudio's own fades are not enough: a sound has one fade schedule, so a scheduled stop with fade replaces the fade-in, the fade-out can start a processing block late and be cut short by the stop, and the stop time it leaves behind ends a later start early unless it is cleared.
- Starts and stops from the game thread land at the next audio callback, about 10 ms apart on a 480-frame WASAPI buffer. A rhythm (a beep every quarter second) is scheduled on miniaudio's engine clock instead.
- The second stream does not see the game's master volume or mute, or a pause: the cue engine follows the launcher's volume and mute itself, and cues stop themselves when the game pauses.
- The engine starts with the game rather than on first use: opening the stream and decoding files took about 50 ms of a frame.
- Online, a cue on the fork's stream never touches the game's voices or netplay sound handles, so only the speech gate applies: silent while `pc_net_resim()` is true. A cue played through the game's path is still wrapped in `net_sfx_private`.
- The agent cannot hear either stream. The cue engine needs its own dump of its mix, as `MELEE_AUDIO_DUMP` is for the game's, for verification by analysing the recording.
- miniaudio is public domain or MIT No Attribution, so it is compatible with the fork's GPL.
