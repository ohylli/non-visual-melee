# 05 The shader line on Home

Status: needs-triage
Type: grilling

Raised by the v0.4.2 base merge. The base port added a line to the launcher's Home page that the fork's reader leaves silent. Decide whether a player should hear it, and when.

## What the base port added

- `#shader-status` in `resources/launcher.rml`, on the disc card below `#status`. Focus never reaches it, like the rest of the disc card.
- Its first text is "New shaders are cached as you play." (from the `.rml`). The launcher's loop in `Launcher::run` (`src/pc/launcher.cpp`) rewrites it whenever the number of shader pipelines still compiling changes: "Preparing cached shaders (N left)..." while any are pending, then "Cached shaders ready. New effects may compile during play." once none are. If none are pending when the launcher opens, the first text stays.
- The count can change on every frame while the shader cache is being compiled, which happens mostly on a fresh install or after a graphics driver update. With a warm cache the line usually never changes.

## What the reader does today

- `LauncherReader` in `src/pc/a11y/launcher_speech.cpp` watches `#status`, the Settings status lines and the update banner. It does not watch `#shader-status`, so the line is never spoken, not in the opening and not as it changes.
- Pressing Play while shaders are pending already speaks something: the launcher holds the game and writes "Compiling shaders (N left). Press Play again to start now." into `#status`, which the reader speaks under its settle rule. So a player learns about the wait only after pressing Play.

## A related change: a direct start goes through the launcher

Since v0.4.2, `melee.exe <disc>` (a shortcut, or a disc image dropped on the exe) no longer skips the launcher. The launcher opens, checks the disc, and presses Play by itself (`start_requested` in `pc_launcher_run`), so a direct start waits for shaders as Play does. With a warm cache the launcher closes before the reader's opening settles and nothing is spoken. With a cold cache it stays up through the wait, and the opening and then the falling count in `#status` may be spoken before the title screen. That has not been heard yet.

## Questions to settle

- Should the line be heard at all, given that `#status` already says when Play waits?
- If so, where: in the opening ("Melee launcher. Ready to play, Disc not verified. Preparing cached shaders, 120 left. Play, button."), added to Play's announcement as the disc is, or as a watched text spoken when it changes?
- A changing count needs a rule like the disc verification's progress steps (spoken every 20%), or only the start and the end ("Preparing cached shaders", "Cached shaders ready").
- Should the first text, "New shaders are cached as you play.", ever be spoken? It is a hint for a sighted player, not a state.
- On a direct start with a cold cache: is hearing the launcher and the shader count before the title screen welcome, or should the reader stay quiet when the launcher was opened only to wait?
- The port menu has no such line, so this concerns the launcher alone.
