# 02 Settings view

Status: ready-for-agent
Type: task
Blocked by: 01

The second slice of `.scratch/launcher-accessibility/spec.md`: the Settings view, on the machinery from issue 01.

## Speech in this slice

Wording is in the spec. In short:

- **Entering Settings.** The reader notices `#preferences` becoming visible and speaks "Settings." followed by the tab announcement. Back to launcher lands on "Settings, button".
- **Tabs.**
  - "Graphics, tab, selected, 1 of 5. Left and right to switch tabs."
  - Then the text of every row with no control on the selected page: the Controls page's two sections; the Online page's connect code (`#net-code`) and discovery note.
  - Left and Right on the tab strip keep focus on the tab and change the page. Each new tab is announced the same way.
- **Cycling settings.** Name from the row's heading, including a badge such as "RESTART". Then the value, "Enter to change", and the help sentence.
- **Sliders.**
  - Name, "slider", the value from the row's `-val` span, "Left and right to adjust", then the help sentence.
  - Steps are spoken as value changes. Read the span after the frame, not from the `change` event.
- **Text fields.** "Player name, edit, <value>." then the help sentence. The stored value is spoken when focus leaves the field.
- **Action rows.** "Performance preset: Apply" and "Check for updates now: Check now" read as buttons named by their row heading. Their results arrive through the watched regions.
- **Watched regions.** Add `#settings-status` (never speaking "Saved automatically.") and `#check-status`, with the settle rule.

If a row's shape defeats the generic reader, add the case to the fork override table rather than special code, and note it in the primer's "Where the words come from".

## Done when

- The headless test covers:
  - entering Settings;
  - a tab change with its control-less rows (Online's connect code);
  - a cycling setting and its value change;
  - a slider step;
  - a text field leaving focus;
  - a `#settings-status` message.
- The whole launcher's rows are walked once in the test, asserting each has a non-empty name. That guards against a base merge adding a row the reader cannot name.
- Build, unit tests, bounded run and `check_style.py` pass as in issue 01.
- CLAUDE.md's launcher status line now covers Settings.

## Play-test (maintainer, by ear, NVDA)

1. Open Settings. Walk every tab with Left and Right, and each page with Down and Up.
2. Change a cycling setting with Enter. Is hearing only the new value enough?
3. Step Master volume with Left and Right.
4. Controls tab: how does the keyboard layout text sound? Does it need rewording?
5. Online tab: is the connect code heard? Type a player name, then leave the field. Then type an invalid name (a symbol) and leave. Is hearing the old value back a clear enough signal?
6. Performance preset: Apply. Is the long explanation useful or too long?
7. Escape and Back to launcher both land on "Settings, button".

## Comments
