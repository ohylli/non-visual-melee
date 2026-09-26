# 01 Home view, status lines and the reader's foundation

Status: resolved (2026-09-26)
Type: task

The first slice of `.scratch/launcher-accessibility/spec.md`: everything outside the Settings view, plus the machinery that issue 02 builds on. Read the spec, ADR-0002 and the primer `docs/a11y/rmlui-screens.md` first.

## Machinery

- **Plugin.** An `Rml::Plugin` registered from `pc_a11y_init`. It must include `EVT_BASIC` in its event classes, because RmlUi's shutdown only unregisters plugins in that list, and it must be a static object. On load of the document whose source is `launcher.rml`, it starts the launcher reader and adds the capture-phase key listener. On unload, it stops both. Every other document is ignored.
- **Frame hook.** `pc_a11y_launcher_frame(void)` is declared in `a11y_hooks.h` and called once per turn of `Launcher::run`'s loop. The header include in `launcher.cpp` is the second base port line. Check the footprint with `grep -rn a11y src CMakeLists.txt --exclude-dir=a11y`.
- **Frame comparison** in the reader, which speaks through the one `Speech` owned by `hooks.cpp`:
  - The focused element (`Context::GetFocusElement`), limited to the launcher document.
  - The focused element's value.
  - The watched regions: `#status`, and the update banner's `#update-title` and `#update-desc`.
- **Settle and progress rules.** Settle time 250 ms, progress step 20%, both named constants in one place.
- **Text clean-up.** " / " and " · " become ", ".
- **Tab key.** Tab and Shift+Tab become Down and Up, as in the spec.
- **Headless test.** Prove it works first: an RmlUi context with a do-nothing render interface, the real `resources/launcher.rml`, `Context::Update` run so computed styles apply, and an element focused programmatically. If computed styles or focus do not work headless, stop and report back rather than work around it.

## Speech in this slice

Wording is in the spec. In short:

- **Opening.** Held until `#status` settles, then "Melee launcher. <status>. <focus announcement>", queued behind "Non-Visual Melee ready".
- **Home buttons.** "Choose disc, button". Play also carries the disc: "Disc: <#disc-name>, <#disc-info>".
- **Update banner.** Its three buttons are read the same way. Its appearance queues "Update available: <#update-tag>".
- **Changed labels.** A button whose label changes is spoken as a value change: Verify disc to Cancel verification, and the update action's labels.
- **Status texts.** `#status` changes are spoken with the settle rule. Verify progress and update download progress are spoken every 20%.

## Done when

- `cmake --build build` and the unit tests pass. The new headless test covers:
  - the opening announcement;
  - the Play announcement with a disc;
  - a status change that settles, and one that does not;
  - verify progress at 20% steps;
  - a button label change;
  - Tab becoming Down.
- A bounded `title` run (with `MELEE_A11Y=0`) exits cleanly, and the log shows no plugin errors at startup or shutdown.
- `python tools/check_style.py` passes.
- The primer is corrected wherever the code turned out different.
- CLAUDE.md has one "Accessibility status" line for the launcher (Home and status).

## Play-test (maintainer, by ear, NVDA)

1. Launch with no disc remembered (rename `launcher.cfg` aside). You should hear "Non-Visual Melee ready", then "Melee launcher. No disc selected. Choose disc, button".
2. Down and Up through the Home buttons, then Tab and Shift+Tab. Both keys should follow the same order.
3. Choose disc: the Windows dialog reads normally. Pick `melee.iso`. You should hear "Play Melee, button. Disc: ..." then "Ready to play, disc not verified".
4. Cancel the dialog once. Is what NVDA says on returning to the launcher enough?
5. Relaunch. The disc is remembered, and the opening should say "Ready to play" without "Checking disc image".
6. Verify disc: "Cancel verification", then progress every 20%, then the result. Is 20% right, and is 250 ms settle right?
7. Pick a wrong file, such as a text file renamed `.iso`. You should hear the error.

## Comments

### 2026-09-25, implementation (agent)

Implemented; the play-test above is what remains.

- The headless test (`src/pc/a11y/test_launcher_speech.cpp`, ctest `launcher_speech`) worked first time: computed styles, visibility and focus all behave without a window.
- It found a base port bug. `Element::Focus()` refuses a button whose computed `focus` is `none`, and RmlUi refreshes that only on update, so a button enabled and focused in the same frame stayed unfocused. After choosing a disc, focus stayed parked on the button column instead of reaching Play. The fix is one call in `Launcher::enabled` (`document->UpdateDocument()` after enabling), a base port edit to keep in its own commit.
- The opening waits for focus to reach a control as well as for the status line to settle, since the start-up disc check parks focus. If focus never reaches a control, the opening is spoken without it a second later.
- The update banner is followed by its own `display`, so leaving Settings does not announce it again.
- A launcher run with speech off (no disc argument, killed by `timeout` after 8 s) logged the expected opening: "Melee launcher. Ready to play, Disc not verified. Play Melee, button. Disc: melee.iso, Super Smash Bros. Melee, USA, Revision 2 (1.02)".
