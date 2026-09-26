# 04 Editing the Online text fields

Status: needs-triage
Type: grilling

Raised by the play-test of issue 02. Needs further thought before it can be built.

## The problem

The Online tab's two text fields, Player name and Friend's connect code, work as the spec says, but editing them is a rough experience. What the reader gives today:

- On focus: the name, "edit", the value (or "blank") and the help sentence.
- While typing: nothing from the fork. Typed characters are not echoed, and moving the cursor with Left and Right or deleting with Backspace is not read.
- On leaving after typing: the field's name and the stored value. A rejected entry is only noticed by hearing the old value come back.

The spec listed character echo, reading the cursor and announcing rejected input as out of scope for the first version.

## What the launcher does

- RmlUi sends a `change` event on every keystroke. `change_online` in `src/pc/launcher.cpp` checks each one: at most 8 characters for the name, 17 for the connect code, letters and digits only (and `#` in the connect code), lowercase turned into uppercase. It stores the entry only if it passes and silently ignores it otherwise.
- The field kept showing whatever was typed. Issue 02 added a base port fix: on leaving a field, the launcher writes the stored value back into it.
- An empty player name is never stored; an empty connect code is (it means "host your own code").

## Questions to settle

- What does the screen reader already do while typing into the game window? Does NVDA's own typed-character echo speak there? This decides whether the fork should echo characters.
- Should Left, Right, Home, End and Backspace be read (the character under the cursor, the deleted character)?
- When should a rejected entry be announced, and how: at the keystroke ("# not allowed in the player name", "8 characters at most"), or when leaving the field? The fork only sees the page, so it would need the launcher's rules or a hook.
- Should the uppercase conversion be heard?
- Should Enter confirm the entry, and should it be spoken ("Player name saved, OTTO")?
- The port menu has the same fields. Whatever is chosen should be reusable there.

## Comments
