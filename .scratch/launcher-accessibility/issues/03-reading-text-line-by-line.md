# 03 Reading page text line by line

Status: needs-triage
Type: grilling

Raised by the play-test of issue 02. Needs further thought before it can be built.

## The problem

Text that has no control is read in one go, as part of the tab announcement (issue 02, primer `docs/a11y/rmlui-screens.md`). On the Controls tab that is one long announcement: both sections, Keyboard and Controller, with the whole keyboard layout in between. It works as specified, but it is hard to follow. The player cannot stop at a line, hear it again or skip ahead. Reviewing the text line by line would be better.

The Online tab has the same shape on a smaller scale: the connect code and the discovery note come with the tab announcement.

## Why it is not simple

- Focus never reaches these rows. The launcher's arrow order (`focus_ids` in `src/pc/launcher.cpp`) holds only controls, and the reader follows focus.
- Letting Up and Down stop on text means either a base port change to the focus order, or a reading position the fork keeps itself, driven by its capture-phase key listener (the one that turns Tab into Down).
- The page does not say what a "line" is. The Controls text is two sections, the keyboard paragraph has a `<br/>`, and each key and action pair is separated by " · ".

## Questions to settle

- What is one line: a section, a paragraph, a `<br/>` line, or one key and action pair?
- How does the player move through the lines: Up and Down after the tab, a separate key, or something else? How does that fit with the launcher's own Up and Down order?
- What does the tab announcement say once the lines can be reviewed: all of the text as now, or only a hint that there is text to read?
- Should the Online tab's connect code and discovery note work the same way?
- Does the keyboard layout text need rewording as well ("WASD, arrows Move, X Attack")? The spec left that to play-testing.
- The port menu has the same pages. Whatever is chosen should be reusable there.

## Comments
