# 03 Drive scripts, the verify skill and the docs

Status: ready-for-agent
Type: task
Blocked by: 02

`.scratch/accessibility-submenus/spec.md`, "Tests" and "Docs".

## Machinery

- **Drive scripts.** `launcher_a11y`, `launcher_settings`, `launcher_voices`, `port_menu`, `port_menu_a11y`, `port_menu_voices` and `match_thresholds` reach their rows through the submenus, and one of the launcher's and one of the port menu's opens and leaves a submenu, the nested one included, checking the spec's words.
- **The `verify` skill.** The scripts' new paths and expected lines.
- **Docs.** `docs/a11y/rmlui-screens.md`, "The Accessibility category": the submenus, moving in each page, the Back hooks in the wiring list. `docs/a11y/cues.md`, "Accessibility settings", and `docs/a11y/speech.md`, "Choosing a voice": where the rows are and how a shown value names its submenu. `CLAUDE.md`'s Accessibility settings status line.

## Done when

- The `verify` skill's menus level passes, and its online level, since `match_thresholds` sets values through the category.
- The docs name no row counts or values (CLAUDE.md, "Docs").
