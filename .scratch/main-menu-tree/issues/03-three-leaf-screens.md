# 03 Three leaf screens: Sound, Screen display, Multi-Man Melee

Status: ready-for-agent
Type: task
Blocked by: 02

The slice of `.scratch/main-menu-tree/spec.md` that proves rows and values. Read the spec, ADR-0003 and the primer `docs/a11y/native-menus.md` first.

## Machinery

- **Centre text hook.** `pc_a11y_menu_center_text(int string_number)`, declared in `a11y_hooks.h`, called in `Menu_InitCenterText` (`src/melee/mn/inlines.h`) with the number the helper was given. That helper is a static inline in a decomp header, used by Sound, Screen display, Language and Multi-Man Melee, so the hook's header include goes into `inlines.h`. Check that it compiles in every file that includes it.
- **What the number tells.** Numbers are the NTSC-U ones, as the game's code uses them.
  - Sound: 187 is the Channel row, 188 the Volume row.
  - Multi-Man Melee: 171 to 176 are the six choices in order.
  - Screen display sets 189 once and Language 191 once; they carry no state.
- **Values.** Read by the poll through `game_access.c`:
  - Sound's channel: `lbAudioAx_80024BD0()`, true for Mono.
  - Sound's balance: `gmMainLib_8015ED74()`, -100 to +100. Check which sign leans toward music; Right moves toward music.
  - Screen display: `gmMainLib_8015F4E8()`. Check that the preference changes at the press and not only when the player leaves the screen. If it changes only on leaving, the value needs a hook at the A branch of `mnDeflicker_8024A168` instead, and the spec's hook table gets a row.
- **Extending the snapshot.** The menu reader's snapshot gains the centre text number and the three values. The reader stays pure.
- **Ordering.** A leaf screen's opening function sets `cur_menu` and the centre text in the same frame as the press. The opening announcement needs both, so compose it from the snapshot at the poll, not inside the centre text hook.

## Speech in this slice

Wording is in the spec, section "Leaf screens with a reader". In short:

- Opening: title, row and value, key hint, description.
- Row change on Sound: row, value, description.
- Value change: the value alone.
- Multi-Man Melee choice change: choice and description.
- Nothing for a press that changes nothing.
- These three screens lose their "No speech yet." notice.

## Done when

- `cmake --build build` passes, and the unit tests pass, with the menu reader's test extended:
  - each screen's opening announcement;
  - Sound: a row change both ways, Stereo to Mono and back, the balance at the centre, on each side and at each end, and no announcement for an unchanged value;
  - Screen display: On to Off and back;
  - Multi-Man Melee: a choice change, the wrap from Cruel Melee to 10-Man Melee, and an opening on a choice other than the first.
- A drive script under `tools/a11y/` opens each of the three screens, changes what can be changed, waits on each expected `[a11y]` line, and restores Sound and Screen display before leaving. It never presses A or Start on Multi-Man Melee. The exploratory scripts `.scratch/main-menu-tree/drives/options-leaves.drive` and `multi-man.drive` show the way in.
- The tree walk of issue 02 still passes, with the three notices gone from its expected lines.
- `python tools/check_style.py` passes.
- The primer's leaf screen notes are updated where the code turned out different, including what the centre text helper is good for.
- CLAUDE.md: the "Accessibility status" line of issue 02 extended, or a line added.

## Comments
