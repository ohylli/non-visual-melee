# Launcher accessibility

Status: agreed 2026-09-25 (grilling session with the maintainer)

## Goal

The first user-facing accessibility feature: a blind player can use the whole launcher by ear. They can choose and verify a disc, start the game, handle an update notice and change every setting, hearing each control as they move to it and each result as it happens.

Vocabulary is the glossary in `CONTEXT.md`: launcher, port menu, setting, announcement, hook, speech. The mechanism is explained in the primer `docs/a11y/rmlui-screens.md`; why it reads the page instead of hooking each transition is ADR-0002.

## What the launcher contains

One RmlUi document, `resources/launcher.rml`, driven by `src/pc/launcher.cpp`, with two views:

- **Home.**
  - Six buttons: Play Melee, Choose disc, Verify disc, Settings, Discord, Quit.
  - A disc card that focus never reaches: the disc's name, its description and its path.
  - The main status line, which also stays visible in Settings.
  - An update banner with three buttons, shown when an update is found. While it is shown, its buttons come first in the arrow order.
- **Settings.**
  - A strip of five tabs, one page per tab:
    - Graphics: 11 rows.
    - Audio & interface: 10 rows.
    - Cheats: 4 rows.
    - Controls: text only.
    - Online: two text fields, a read-only connect code and one choice.
  - Its own status line.
  - A Back to launcher button.

There are no popups: every result goes to a status line. The file chooser is the standard Windows dialog, which the screen reader already handles.

The launcher's keys:

- Up and Down move focus in the launcher's own order, skipping disabled controls.
- Left and Right step sliders and switch tabs.
- Enter activates a button or cycles a setting.
- Escape leaves Settings or cancels a verification.
- The gamepad does the same through raw SDL events. Whether pads are detected before the game runs is unverified.

## Decisions

### Scope

- The whole launcher, built in two slices: Home and status first (issue 01), then Settings (issue 02).
- The port menu stays silent until its own feature. The reader handles `launcher.rml` only, even though the port menu shares the RmlUi context and the row layout.
- The Online text fields get basic support: label and value on focus, and the stored value when focus leaves. Typed characters are not echoed and cursor movement is not read yet.

### How the fork attaches

- An `Rml::Plugin`, registered from `pc_a11y_init`, starts and stops the reader when `launcher.rml` loads and unloads, and adds the fork's key listener to that document.
- One new hook, `pc_a11y_launcher_frame()`, called once per turn of `Launcher::run`'s loop, is where the reader compares the page with the previous frame. Together with the include line, it is the fork's whole footprint in `launcher.cpp`.
- The reader does not rely on RmlUi focus or change events for what it speaks. It compares frames, which also covers gamepad input, text the launcher rewrites and several focus moves inside one frame. See the primer for why.
- Words come from the page:
  - a button's text;
  - a setting row's heading and help sentence;
  - a slider's value span;
  - the selected tab's class.
- A small fork override table fixes wording that reads badly.
- Visual separators become commas: " / " in "Ready to play / Disc not verified.", and " · ".

### Focus announcements

One announcement, interrupting, whenever the focused element changes:

| Control | Announcement |
| --- | --- |
| Action button | "Choose disc, button". When disabled: "..., button, unavailable". |
| Play Melee | "Play Melee, button. Disc: Melee.iso, Super Smash Bros. Melee, USA, Revision 2 (1.02)". This is the only way to hear the disc card. |
| Cycling setting | "Vertical sync, On, Enter to change. Synchronize frames with the display." No role word, then the help sentence last. |
| Slider | "Master volume, slider, 80%. Left and right to adjust." Then the help sentence, if the row has one. |
| Tab | "Graphics, tab, selected, 1 of 5. Left and right to switch tabs." Then the text of every row on that page with no control. |
| Text field | "Player name, edit, Otto." Then the help sentence. |

- The help sentence is read on every focus, last, so any key press cuts it off, and a braille display holds the whole line.
- Position ("1 of 5") is spoken on tabs only.
- A setting's badge, such as "RESTART", is part of its heading text and is spoken with the name.

### Value changes

- A new value on the element that already has focus is spoken alone, interrupting: "Off", "85%".
- This covers Enter, the gamepad, slider steps, and a button whose own label changes: "Verify disc" becoming "Cancel verification", or "Update Now" becoming "Downloading..." and then "Restart to apply".
- When a text field loses focus, its stored value is spoken. This is how the player notices a rejected entry: the launcher silently keeps the old value.

### Watched text

These regions are spoken queued, behind any focus announcement, whenever their text changes:

- **The main status line.**
- **The Settings view's own status line.** "Saved automatically." is never spoken.
- **The update-check result** under "Check for updates now".
- **The update banner's title and description.**

Rules:

- **Settle time.** A change is spoken once the text has stayed the same for the settle time, so passing messages are skipped. A message that lasts longer, such as "Verifying disc", is spoken.
- **Progress.** A text whose only change is its percentage is progress. It is spoken when it crosses the next reporting step, and at 100%, without waiting for the settle time.
- **Update found.** When the update banner appears, "Update available: <version>" is queued.

Both numbers are named constants in one place, to be tuned by ear:

| Constant | Value |
| --- | --- |
| Settle time | 250 ms |
| Progress reporting step | 20% |

### Opening and views

- **Opening.** When the launcher opens, the reader holds its first announcement until the status line has settled, so the start-up disc check has finished. It then speaks "Melee launcher. <status>. <focus announcement>" as one announcement.
- **Proof-of-life.** "Non-Visual Melee ready" stays in `pc_a11y_init`. It is the only speech on the command-line-disc path, where the launcher is skipped, and the evidence line for bounded runs. The opening announcement is therefore queued behind it rather than interrupting it.
- **Entering Settings.** Speaks "Settings." followed by the tab announcement. Back to launcher lands on "Settings, button".
- **Cancelling the file dialog.** Nothing is spoken, since RmlUi focus never left Choose disc. The screen reader announces the launcher window as it regains focus.
- **Disabled Play.** Play stays unreachable while disabled, as it is today. With no disc, the status line has already said why.

### Keys

- Tab acts as Down and Shift+Tab as Up. The fork's capture-phase key listener consumes the key and sends a Down or Up key event to the focused element, so the launcher's own navigation runs. This works on the launcher document only.
- There is no fork key to repeat the status line or the disc.

### Names and layout

These are suggestions; the implementer may reshape them.

| File | Role |
| --- | --- |
| `rmlui_reader.hpp/.cpp` | Generic page reading, meant for reuse by the port menu. It finds a control's name, role, value and help sentence from the row shape, and applies the separator clean-up. |
| `launcher_speech.hpp/.cpp` | The launcher's specifics: which document, the watched regions, the opening, the disc on Play, the frame comparison, and the settle and progress rules. |
| The plugin | Registers from `pc_a11y_init` and is unregistered before speech shuts down. It must tolerate RmlUi shutting down after speech has gone. |

- Speech is owned by `hooks.cpp`. The launcher module gets it by reference or through a small accessor, not a second instance.
- Hooks: `pc_a11y_launcher_frame(void)` in `a11y_hooks.h`, called in `Launcher::run`'s loop, plus the header include in `launcher.cpp`.

### Tests

- **Headless test.** A fork unit test loads the real `resources/launcher.rml` into an RmlUi context with a render interface that draws nothing. It then:
  1. Focuses elements and rewrites text the way `launcher.cpp` does.
  2. Calls the reader's frame step.
  3. Checks the announcements through a fake bridge, as `test_speech.cpp` does.
- Issue 01 proves that such a context works, including computed styles (visibility, focusability), before the rest relies on it.
- This test is the guard against base merges that reshape the page.
- **Bounded run.** The agent's check is still a bounded `title` run. It passes the disc on the command line, so it never shows the launcher, but it proves the plugin registers and unregisters cleanly and the game still boots.

### Docs

- ADR-0002: the plugin and frame hook instead of per-transition hooks.
- Primer `docs/a11y/rmlui-screens.md`, already written. Update it where the code turns out different.
- CLAUDE.md: one line under "Accessibility status" as each slice lands.

## Out of scope, noted for later

- The port menu: its own feature, reusing the reader, with a frame hook in the game loop and the netplay silence rule.
- Text field editing: typed-character echo, reading characters under the cursor, and announcing rejected input explicitly.
- A key to repeat the status line or the disc: declined for now.
- Rewording the Controls page's keyboard text ("WASD / arrows Move · X Attack"): only if it sounds bad in play-testing.
- Mouse hover: not announced; clicks move focus and are announced like any focus change.
