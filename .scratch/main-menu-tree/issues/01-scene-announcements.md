# 01 Scene announcements

Status: resolved (2026-09-27)
Type: task

The first slice of `.scratch/main-menu-tree/spec.md`: the player hears which scene they have arrived in. It is small on purpose. It proves a hook in a decomp layer file, and it gives issue 02 the signal that a scene was entered. Read the spec and the primer `docs/a11y/native-menus.md` (section "Scenes") first.

## The hook

- `pc_a11y_scene_entered(int mode_kind, int scene_kind)`, declared in `a11y_hooks.h`.
- Called in `gm_801A4014` (`src/melee/gm/gm_1A3F.c`), right after the scene's enter function returns, so the announcement lines up with the first frame the player can act on. The enter function loads the scene and blocks while it does.
- `mode_kind` is the kind of the mode being run (`mode->kind`), which during a memory card interruption differs from `gm_GetCurrentGameMode()`. It is passed for later features and unused now.
- The header include is the second line of footprint in that file. Check with `grep -rn a11y src CMakeLists.txt --exclude-dir=a11y`.
- Silent while `pc_net_resim()` is true. CLAUDE.md asks for this gate in `hooks.cpp`, and it does not exist yet: add it in a form the later hooks can share.

## The table

The spec's section "Scene announcements" has the three groups and the wording: scenes to operate, things to watch, and the silent ones.

- One table keyed by `GameSceneKind`, in fork code.
- One announcement per scene entered, interrupting.
- A kind missing from the table is silent and logged once per kind.
- Entering the same kind again is announced again: character select after results is a new arrival.

## Done when

- `cmake --build build` passes, and the unit tests pass with a new test of the table: a named scene, a silent scene, an unknown kind, the same kind twice.
- A bounded `title` run with `MELEE_A11Y=0` logs the title screen's announcement after "Non-Visual Melee ready" and exits cleanly.
- A bounded `vs` run logs no announcement for the match itself.
- A drive from the title screen into VS. Mode, Melee logs "Character select. No speech yet." Going back to the menu then logs no scene announcement, since the menu scene is silent at this level. Character select goes back when B is held; find the hold time with the driver's press length.
- A run without `MELEE_BOOT_SCENE` would show the opening movie's announcement, but the driver always sets a boot scene. If no cheap way exists to check it, say so and leave it to the play test.
- `python tools/check_style.py` passes.
- The primer's "Scenes" section is corrected: the opening movie comes before the title on a normal launch, and `GS_CAMERA_VS` is a notice screen, not the snapshot match.
- CLAUDE.md: one "Accessibility status" line, and the `title` run's expected `[a11y]` lines in "Verification" updated.

## Comments

### 2026-09-27, implementation (agent)

Implemented; hearing it is issue 04.

- The table is `src/pc/a11y/scene_speech.cpp`, keyed by `SceneKind`, a copy of `GameSceneKind` in `scene_kinds.h`, since fork C++ does not include decomp headers. `game_access.c` checks every number of the copy against the decomp at compile time, and the table has a compile-time check that it lists every kind once, silent ones included. A base merge that renumbers scenes fails the build; a kind appended at the end is "not in the scene table" and silent, logged once.
- The rollback gate is `game_hook_may_speak()` in `hooks.cpp`, for every later hook from game code.
- A drive into VS. Mode, Melee logged "Character select. No speech yet.", and holding B went back to the menu with no scene announcement. B needs a hold of 800 ms; 400 ms is not enough.
- The opening movie turned out cheap to check: `drive.py --scene ""` gives a plain boot. With `--no-card` it first stops on the memory card prompt ("There is no Memory Card in Slot A", then "Continue without saving or loading Game Data?"), silent as planned; two A presses later the log has "Opening movie. Press Start to skip." On the maintainer's machine with a readable save that prompt should pass unseen, but a blind player without a card meets a silent prompt at every launch until its reader lands.
- The first unlock notice after the title screen now says "Unlock notice. No speech yet." once for the whole run of notices, since they are one scene.
