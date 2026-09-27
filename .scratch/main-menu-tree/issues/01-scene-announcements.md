# 01 Scene announcements

Status: ready-for-agent
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
