# Launcher speech reads the RmlUi page through a plugin and one frame hook

Status: accepted (2026-09-25), amended (2026-10-17)

The launcher (`src/pc/launcher.cpp`, `resources/launcher.rml`) is an RmlUi page that the base port changes almost daily: 39 commits in its first twelve days of history. CLAUDE.md prefers a hook at each semantic transition, but here the transitions (status text, focus moves, tab switches, view switches, each setting's value) are spread over a dozen private functions in that file. So the fork reads the page instead of being told about it. An `Rml::Plugin` registered from the existing `pc_a11y_init` hook learns when the launcher document loads and unloads, and listens to keys on it. A single hook, `pc_a11y_launcher_frame()`, called once per iteration of the launcher's own loop, compares the page with the previous frame: the focused element, its value, and a few watched text regions. The spoken words come from the page's own text, not from a string table. That gives one line of footprint in the busiest base port file, and new settings rows are spoken without fork work.

## Considered options

- **A hook at each transition** (`status()`, `show_tab()`, `show_settings()`, the focus moves, each value change): about ten lines in a file that changes daily, and every new feature in the launcher needs another.
- **A hook in `text()`, the launcher's one text-writing helper**: one line, triggered only by a real change, but it misses text written by other means (the input-delay value, anything a later data binding writes).
- **Zero base edits**: the plugin inserts an invisible fork element whose per-frame `OnUpdate` gives a tick. It works, but it adds a fork element to the base port's page and leans on RmlUi internals that a reader would not expect.
- **A fork string table keyed by element id**: full control of wording, but it must follow every row the base port adds. String tables stay for native menus, whose labels are images.

## Consequences

- Label reading depends on the page's markup shape (a setting row is a heading and help text beside a control). A base merge that restructures the page can silently break it. A headless unit test that loads the real `launcher.rml` guards against that.
- Changes are seen at most one frame late, which nobody hears.
- The port menu shares the RmlUi context and the row shape, so its feature can reuse the reader. It needs its own frame hook in the game loop, and must honour the netplay silence rule there.

## Amendment (2026-10-17): a step hook for a page's lines

Reading a Settings page's text line by line (`docs/a11y/rmlui-screens.md`, "Reading the lines of a page") needs Up and Down to stop where focus never goes, before the launcher moves focus. A frame comparison sees only where focus landed, too late to keep it. So a second hook, `pc_a11y_launcher_step(direction)`, is the first line of the launcher's `focus_step`, where every way of moving ends; it returns true when the fork used the press, and the launcher then leaves focus alone. That is a second line of footprint in the launcher, not a hook at each transition: everything else is still read from the page.

- **Text rows in the launcher's focus order** (`focus_ids`): the base port's order and its focus highlight would change for every player, in the busiest file.
- **The fork's capture-phase key listener**, which already turns Tab into Down: no footprint, but it sees only keys. The D-pad and the stick reach the launcher from SDL and never pass through RmlUi.
