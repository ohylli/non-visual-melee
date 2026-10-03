# Non-Visual Melee

An accessibility fork of [melee-pc](https://github.com/999sian/melee-pc), the PC port of Super Smash Bros. Melee. Goal: make the game playable by blind players through screen reader speech (native menus, port menu, launcher) and audio cues in combat. A hobby project: decent code quality, no production ceremony.

Use the vocabulary in `CONTEXT.md` (base port, decomp layer, port layer, hook, announcement, cue, string table, native menu / port menu / launcher). The base port is young and changes fast; every rule below exists to keep base merges cheap.

## Remotes and branches

- `origin` is this fork (`ohylli/non-visual-melee`). `upstream` is the base port, fetch-only: its push URL is disabled on purpose.
- `master` is the fork's own branch. Topic branches hold uncertain work. A `dev` branch gets added once a first alpha is released from `master`.
- A base merge is `git fetch upstream` then `git merge upstream/master` on `master`, conflicts resolved locally; the `/base-merge` skill runs it and writes the digest of upstream changes. `upstream/master` is the base port's only live branch; `upstream/main` and `upstream/testing` are stale.
- All GitHub activity targets this fork. Pull requests, issues and pushes toward the base port happen only when the maintainer asks for that one explicitly. GitHub's "Sync fork" button stays unused: it can discard the fork's commits.
- Fixes unrelated to accessibility are welcome on `master`, kept in their own commits.

## The two layers

- Decomp layer: `src/melee/`, `src/sysdolphin/`, `src/Runtime`, `src/sdk_include`, `src/thp`. Copied from doldecomp/melee, pinned by `src/UPSTREAM_COMMIT`, refreshed by large decomp syncs. Leave its formatting and structure exactly as found; many functions have address-style names (`lbAudioAx_800237A8`) because they are not yet understood.
- Port layer: `src/pc/` (C11 and C++20), `extern/aurora` (vendored graphics, input and UI compatibility library; UI toolkit is RmlUi), `resources/`, `tools/`, `cmake/`.
- The decomp layer is unlicensed Nintendo/HAL code; the port layer is GPL-3.0-or-later, and so is the fork's code. The game needs the player's own disc image, which stays out of git.

## The isolation rule

Base merges conflict wherever the fork edits a base port file, so the fork's footprint there is one-line hooks and nothing else.

- All fork code lives in `src/pc/a11y/`: C++20 internals, built through the fork-owned `src/pc/a11y/a11y.cmake`, which the root `CMakeLists.txt` includes with a single line. Third-party wiring (the Prism screen reader library) goes in that file too.
- A hook is one call to a `pc_a11y_*` function, placed at the semantic transition ("cursor moved", "match started"), the same idiom the base port uses for `pc_is_unlock_all_enabled()`. Fork code decides what to say. Every hook function is declared in the single C-compatible header `src/pc/a11y/a11y_hooks.h`, so that header plus `grep -rn a11y src CMakeLists.txt --exclude-dir=a11y` (the hook calls, the header include and the cmake include) is the full inventory of the fork's footprint. Beyond hooks, the footprint holds screen-reader-only fixes to base port files, so far only the launcher's event pumping while the file dialog is open (`src/pc/launcher.cpp`): the maintainer keeps such fixes in the fork and takes the merge conflicts rather than open upstream pull requests, and marks each with an `a11y` comment so the grep finds it. A fix that helps every player is not footprint: it stays unmarked in its own commit, and `git log --no-merges upstream/master..master -- src ':!src/pc/a11y'` lists it among the fork's other commits to base port files.
- Prefer a hook at the transition over polling game state each frame; poll only where a screen would otherwise need many hooks.
- Native menus draw most labels as images, so spoken text comes from fork-owned string tables keyed by the game's own identifiers.
- Compose each announcement as one text (setting name and value together).
- Accessibility is always compiled in and switched at runtime, default on. Hooks stay free of `#ifdef`.
- Fork settings live in fork-owned storage, read and written by fork code. They are meant to appear in the port menu eventually; revisit storage when that feature is specced.
- Cues use the game's own audio path where possible (`lbAudioAx_800237A8(sfx_id, volume, pan)` plays any game sound with pan; the software mixer is `src/pc/audio.c`).

## Online compatibility

Fork builds are meant to play online against base port builds of the same release. On Windows over loopback, a fork build plays against another fork build and against a base port build of its last base merge, through character select and into the match with steering, at input delays 0 to 10 frames and over a simulated slow link (`tools/a11y/drive_net.py`, primer `docs/a11y/netplay.md`). On one machine, a fork build also meets either kind in the LAN lobby and goes on into character select with it (`tools/a11y/lan_lobby.drive`).

- Two copies pair when their protocol version, app version string and disc image id match. The fork keeps the base port's version string (`MELEE_APP_VERSION`, `src/pc/version.cpp`), including in fork releases.
- Rollback (online netcode that re-runs recent frames when a late input arrives) needs both machines to simulate identical frames, so fork code reads game state and leaves the simulation untouched.
- The one place the fork writes is the pad hook, `pc_a11y_pad` in `publish_locked` (`src/pc/keyboard.c`): steering adds its stick to port 1's virtual pad there, before the game and the netplay code sample it, so the peer receives it as a player's input (ADR-0004). Nothing else the fork does may change game state or input.
- Speech is gated in fork code: check `pc_net_resim()` in `hooks.cpp` and stay silent while it is true, so a re-run frame never repeats an announcement.
- Cues are gated like speech and started as audio-private sounds. In a netplay session every sound start returns a handle built from the frame and its place in the frame's call order (`src/pc/net_sfx.h`), and the game keeps those handles as state, so a cue counted in that order would give every later game sound a different handle than on a base port peer. Wrap each cue in `net_sfx_private(true)` / `net_sfx_private(false)`, which leaves it out of the count. A private start plays on every re-run, so a cue also stays silent while `pc_net_resim()` is true.

## Build and run (Windows)

Windows is the only supported target for fork work; keep code portable in principle by confining platform specifics to the screen reader bridge (see `docs/a11y/speech.md`).

- Toolchain: MSYS2 UCRT64 (GCC, CMake, Ninja) with its `bin` directory on `PATH`. GCC is required by the decomp layer.
- Configure once with `cmake -B build -G Ninja -DAURORA_SDL3_PROVIDER=vendor`, then `cmake --build build`. The flag builds SDL (the library under windows, input and audio) from source at the commit aurora pins, the same SDL the base port's release packages ship; without it aurora uses an older prebuilt SDL on Windows. The setting stays in the build directory's cache, so only a fresh build directory needs it again. The result is `build/melee.exe`, with its DLLs and `resources/` copied beside it. Every decomp file compiles through a Python wrapper that marks rollback-snapshot sections, so a full rebuild takes a while; the link ends with a "Rollback sections" verification line.
- On Windows the game writes `melee-pc.log` in the working directory, not beside `melee.exe`; `MELEE_LOG_FILE` overrides the path.
- The first configure downloads the pinned Prism release into `build/prism-<tag>/` and SDL's source, and needs network access; later configures reuse them until a base merge moves SDL's pinned commit.
- The base port documents itself in `docs/building.md`, `docs/testing.md`, `docs/debugging.md`, `docs/architecture.md` and `docs/porting-notes.md`. Read the relevant one before exploring the code.

## Driving the game

`tools/a11y/drive.py` is the agent's way around the game: it runs the game on a script of controller presses (the base port's `MELEE_KEY_FIFO` input driver, fed over a named pipe, so the window never needs focus), waits for output lines, saves screenshots of the window, and prints the `[a11y]` lines between the steps. Its `--help` lists the script steps, and inline steps (`-c`) suit a quick look. `tools/a11y/main_menu.drive` gets from the title screen to the main menu, the start of most scripts.

- Reach for it whenever you need to know what a screen does: exploring a menu before planning its speech, screenshotting a screen that is not yet accessible to read its layout and labels, reproducing a bug, and verification.
- It sets `MELEE_A11Y_STEER=1`, which turns steering on with speech off; a run of the game started any other way needs it too for the D-pad to steer.
- Every agent-started game run stays silent (`drive.py` and `drive_net.py` see to it) and leaves the window focus and the real memory card alone: `SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0 SDL_AUDIO_DRIVER=dummy` and `--no-card`, plus `MELEE_A11Y=0` unless the run checks speech itself. Focus taken mid-sentence cuts off the maintainer's screen reader.
- `tools/a11y/drive_net.py` does the same for two instances in one netplay session over loopback (primer `docs/a11y/netplay.md`).

## Verification

The players cannot see the screen and the agent cannot hear the game. The speech log bridges that: every announcement and cue is also written into the game's log with an `[a11y]` prefix, interleaved with game events, and the agent checks a change by reading those lines.

- Verify every change with the `verify` skill. It has four levels (quick, menus, online, base merge), from about a minute to about ten; choose the level the change calls for by your own judgement, unless the maintainer names one.
- "Works" is reserved for what a blind tester has confirmed by ear, on any screen. The port menu and the launcher are out of the drivers' reach and depend on the maintainer play-testing.
- A change that adds or changes a verification tool, script, switch, expected log line or known failure updates the `verify` skill in the same change, so the skill stays the one place that says how to verify.

## Style and lint

Fork code follows `CODING_STYLE.md` at the repo root. Before committing, run `python tools/check_style.py` (clang-format, banned bare `long`, whitespace; covers `src/pc/` only). It needs `clang-format` on `PATH`.

## Docs

The code is the source of truth for what the fork does. A doc records what stays true as features land and what the code does not reveal without real digging: invariants, reasons, constraints from outside systems (Prism, rollback, the decomp), gotchas.

- Primers that explain a subsystem in plain terms ("how native menus work", "the audio path") go in `docs/a11y/`, written as the knowledge is gained, each opening with a short mental model. They name files and functions and leave the details to the code: no line numbers, current call sites, log line formats or test lists. A short illustrative snippet is fine.
- Native menu work starts from `docs/a11y/native-menus.md`: how the game's menus run, where their words come from, a screen inventory and candidate hook points.
- A reason or procedure tied to one spot in the code is a comment at that spot.
- A decision with its rejected alternatives is an ADR in `docs/adr/`.
- Specs and issues under `.scratch/` are snapshots of the plan when written. Later work supersedes them, and the code wins where they disagree.
- The maintainer navigates docs by heading with a screen reader, so a clear heading structure matters more than length.

`docs/*` is ignored by the base port; only `docs/agents/`, `docs/adr/` and `docs/a11y/` are excepted for the fork.

## Accessibility status

Add one line per feature as it lands: what it does, where it lives.

- Speech (`src/pc/a11y/`, primer `docs/a11y/speech.md`): the subsystem every feature hands announcements to, speaking through Prism, with the proof-of-life announcement "Non-Visual Melee ready" at startup. Switches `MELEE_A11Y` and `MELEE_A11Y_LOG`. Play-tested by ear with NVDA and the Windows voice fallback, including speech resuming after an NVDA restart.
- Launcher (`src/pc/a11y/launcher_speech.cpp` and `rmlui_reader.cpp`, primer `docs/a11y/rmlui-screens.md`): an RmlUi plugin and the `pc_a11y_launcher_frame` hook read the launcher page aloud: the opening, Home and update banner buttons, the disc on Play, button label changes, the status line with settle and progress rules, and Tab as Down. Settings reads tabs with their control-less rows, settings with name, value and help, sliders, text fields and the Settings status lines. Play-tested by ear with NVDA. Open: reading page text line by line (issue 03) and text field editing (issue 04).
- Scene announcements (`src/pc/a11y/scene_speech.cpp`, the `pc_a11y_scene_entered` hook in `gm_801A4014`): names the scene the player arrives in, once it has loaded, from a table keyed by `GameSceneKind` ("Stage select. No speech yet. Press Start for a random stage.", "Title screen. Press Start."); matches and the memory card scene are silent, and the main menu tree and character select speak their own opening. Play-tested by ear with NVDA.
- Main menu tree (`src/pc/a11y/menu_speech.cpp` and `menu_names.cpp`, the `pc_a11y_menu_frame` hook in `mnMain_Scene_OnFrame`, ADR-0003): polls the tree's state once a frame; a tree screen opening says its title, hovered entry and description, a cursor move the entry and description, a leaf screen its name and "No speech yet." Sound, Screen display and Multi-Man Melee have readers: row or choice with value, key hint and description on opening, then each changed row, choice or value, with the `pc_a11y_menu_center_text` hook in `Menu_InitCenterText` naming the row or choice (`tools/a11y/leaf_screens.drive`). Names from a fork string table, descriptions decoded from the game's text, Online's words from the base port. Play-tested by ear with NVDA.
- Character select steering (`src/pc/a11y/css_reader.cpp`, `steering.cpp` and `css_targets.cpp`, the `pc_a11y_pad` hook in `publish_locked` and `pc_a11y_css_coin` in the coin's update, ADR-0004): the D-pad steps the local hand in VS modes over rows of targets, the top bar, the unlocked portraits and the player slots' buttons and knobs, along a row or to the target nearest in x in the next, without wrapping; a step names its destination at the press, and a glide takes the hand there by generating stick input while what it crosses goes unsaid. A free hand stepping onto a coin it may pick up hears whose it is and is taken to where A picks it up; a coin carried down says it goes back, and the top bar says it waits while a coin is held. A held slider steps one value a press. A glide that fails says "Could not reach", and the player's own stick takes over at once. Glides are logged with their frames (`tools/a11y/css_steps.drive`, `tools/a11y/css_setup.drive`). Single-player modes add the level and stock arrows as two rows below the portraits and steer only when controller 1 started the mode (`tools/a11y/css_classic.drive`, `tools/a11y/css_training.drive`). Play-tested by ear with NVDA; open findings in `.scratch/character-select/issues/` 10 to 15.
- Character select speech (`src/pc/a11y/css_speech.cpp`, `css_names.cpp` and `css_targets.cpp`, the `pc_a11y_css_frame` hook in `mnCharSel_Scene_OnFrame` and `pc_a11y_css_hand` in `mnCharSel_CursorThink`, spec `.scratch/character-select/spec.md`): polls the screen once a frame and follows the local player's hand, port 1's offline and `pc_net_local_player()`'s online. The opening names the player and their character; a carried coin names each portrait it enters; picking the coin up, clearing the character and B putting the coin back are said, and so is the costume; a choice is left to the game's announcer. A free hand names each button and slider knob it reaches with its value (decided by `css_targets.cpp` from the game's own tests); a player slot's kind, a team, the Teams rule and a held slider's value are said as the hand changes them, and Ready to Fight as it appears. The rules screen and name entry say their name and "No speech yet.", and returning from them says the opening. Online, other players' slot changes and choices are queued (`tools/a11y/css_portraits.drive`, `tools/a11y/css_slots.drive`). Single-player modes open with the mode's name ("Classic character select"), Training's CPU and its coin included, and "Steering needs controller 1." when another controller started the mode; a free hand on an arrow says the row's value and way ("Level: Normal, lower", "Stock: 3, more"), and A the new value, or the same at the end of the range (`tools/a11y/css_classic.drive`, `tools/a11y/css_training.drive`). Play-tested by ear with NVDA, VS and single-player modes.

## Agent skills

### Issue tracker

Issues and specs live as local markdown files under `.scratch/<feature-slug>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

The five default triage roles (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`), recorded as a `Status:` line in each issue file. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
