# 04 Editing the Online text fields

Status: resolved (2026-10-17)
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

### 2026-10-17: agreed, built and play-tested

Settled in a design conversation with the maintainer, built, and play-tested with NVDA (primer `docs/a11y/rmlui-screens.md`, "A focused text field"):

- NVDA's own character echo is left aside: few players have it on, and it does not confirm that the field took the character.
- Every edit speaks the whole value ("O", "OT", "OTT"), "blank" when empty. Left, Right, Home and End speak the character after the cursor, the one Delete would remove, or "blank" at the end. Selection is not read.
- A typed character that changes nothing says "Full, 8 characters at most" in a full field, otherwise "# not allowed" with the character as typed. The fork does not copy the launcher's rules: whatever is refused under the length limit was not allowed.
- A base port fix makes the refusal visible: the launcher's `type_online` drops a character the Online fields cannot store before the field takes it, and puts letters in as capitals. Before it, a refused character stayed in the field and every later keystroke went unsaved too. Pasted text is still checked only as the whole value.
- The name and value on leaving a field are gone, except when leaving rewrites the field (an emptied name gets the stored one back).
- Uppercase is not spoken as a change; Enter says nothing, as every keystroke already saves.
- Play-test finding, fixed: Up and Down move a field's cursor to an end, which was read while focus stayed on the field for the lines between the controls, cutting the line off.
- Reusable by the port menu: `FieldEcho` in `rmlui_reader.cpp` knows nothing about the launcher.
