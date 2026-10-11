# Rules screens: speech

Status: agreed 2026-10-11 (design conversation with the maintainer)

The maintainer agreed the scope (both entry points, online by the same code, Tournament out), the value words, value descriptions with value changes, no "A to open" on rows that open a screen, the item frequency hint once per visit, the rules header's text on character select, and the return wording. The wording of announcements is a first proposal for the play test to judge.

## Goal

A blind player can read and change every rule the game offers before a match: the mode, time or stock, handicap, damage ratio, stage selection, the item switch and frequency, the additional rules and the random stage switch. They can do it from the main menu tree or from character select. On character select they can hear the current rules without opening the screens.

How the game's menus run is in `docs/a11y/native-menus.md`; the menu reader's rules are ADR-0003 and the main menu tree spec (`.scratch/main-menu-tree/spec.md`, section "Leaf screens with a reader"), which this one follows unless it says otherwise. Vocabulary is `CONTEXT.md`: rules screens, rules header, leaf screen, row, value, description.

## Scope

In:

- **The four rules screens**: Custom Rules, Item Switch, Additional Rules, Random Stage.
- **From the main menu tree** (VS. Mode > Custom Rules, scene `GS_MENU`).
- **From character select's rules header** (inside `GS_CSS`), in VS Melee and the Special Melee types that have the header.
- **Online**: the rules header works in a session, and either player drives the screens through the merged pads. The speech is the same code; no drive script covers it.
- **The rules header's text**, said when the hand reaches it.

Out:

- Tournament Melee's rules setup: another scene, where neither frame hook runs. Its rows differ (no Stage Selection, no Friendly Fire, no "none" time).
- Saying that a Handicap change resets every player's handicap slider to 9; the game shows nothing either.

## What the game gives us

From reading `mnmainrule.c`, `mnruleplus.c`, `mnitemsw.c`, `mnstagesw.c` and `mncharsel.c`, and runs on 2026-10-11. Address-style names are the decomp's.

### One set of screens, two hosts

- The same functions run in both scenes. Every screen keeps its cursor in `mn_804A04F0`: `cur_menu` 13 Custom Rules, 15 Additional Rules, 16 Item Switch, 17 Random Stage; `hovered_selection` the row; `confirmed_selection` the hovered row's value as the input function just set it.
- In `GS_MENU` the existing `pc_a11y_menu_frame` poll already sees them (today "Custom Rules. No speech yet." and so on).
- In `GS_CSS`, A on the rules header sets the pending exit `mnCharSel_804D6CF6` to 3; the next frame calls `mn_80231804(..., 1)` and sets it to 5. `mnCharSel_Scene_OnFrame`, and with it `pc_a11y_css_frame`, keeps running while the screens are open. Leaving rebuilds character select in the same scene (`mnCharSel_802640A0`), resetting the pending exit. `cur_menu` keeps a stale value after that, so the pending exit is the gate.
- Input is the merged pad slot (`mn_80229624(4)`): any controller, D-pad or stick, with auto-repeat. Input is ignored for 20 frames on opening, and on the two switch screens until their slide-in ends (B excepted).
- None of the four uses `Menu_InitCenterText`, so `pc_a11y_menu_center_text` never fires here; rows are keyed by `hovered_selection`.
- In `GS_CSS`, font slot 0 holds `SIS_SelCharData` (SdSlChr), not `SIS_MenuData`, yet the screens draw the same string numbers; a screenshot shows "Stages chosen randomly." correctly there. `menu_string()` in `game_access.c` accepts only `SIS_MenuData` today. PAL remaps strings per table (`pc_region_sis_index`; `region.c` has an SdSlChr entry for the rule descriptions).

### Custom Rules

Rows and values (`mn_803EC7DC` bounds); values wrap both ways, and Left and Right on rows 5 and 6 do nothing:

| Row | Label | Values | Read from |
|---|---|---|---|
| 0 | Rules | Time, Stock, Coin, Bonus | `GameRules.mode` |
| 1 | Time Limit (Stock in Stock mode) | Time: 0 none, 1 to 99 minutes. Stock: 1 to 99 | `time_limit`, `stock_count` |
| 2 | Handicap | Off, Auto, On | `handicap` |
| 3 | Damage Ratio | 5 to 20, drawn 0.5 to 2.0 | `damage_ratio` |
| 4 | Stage Selection | On, Random, Ordered, Turns, Loser | `stage_sel` |
| 5 | Item Switch | opens Item Switch | |
| 6 | Additional Rules | opens Additional Rules | |

- `fn_802309F0` copies the screen's values into `GameRules` on the frame a value changes, so `GameRules` is current by the next poll; the screen's own copy is `mn_804D6BD0`'s user data (`struct mn_802307F8_t`).
- Descriptions are game text by number (`mn_803EC818`): one per value on rows 0, 2 and 4, one per row elsewhere (stock has its own, 0x2B).
- A on row 5 opens Item Switch on Food; A on row 6 opens Additional Rules on its first row.
- B: in `GS_MENU` back to VS. Mode with Custom Rules hovered; in `GS_CSS` back to character select. Start: in `GS_MENU` straight to VS character select; in `GS_CSS` back to character select.

### Additional Rules

`MenuRulesPlusData` (`mn_804D6BE0`), bounds and descriptions in `mn_803ED2E8`. Up and Down wrap and skip rows `mnRulePlus_IsOptionVisible` hides.

| Row | Label | Values | Read from |
|---|---|---|---|
| 0 | Stock Match Time Limit | 0 none, 1 to 99 minutes | `GameRules.stock_time_limit` |
| 1 | Friendly Fire | Off, On | `friendly_fire` |
| 2 | Pause | Off, On | `pause` |
| 3 | Score Display | Off, On; shown only after an unlock | `score_display` |
| 4 | Self-Destructs | stored 0 = -1, 1 = 0, 2 = -2 | `GameRules.unk_xc` |
| 5 | Random Stage | opens Random Stage | |

- Every valued row has a description per value except the stock match time limit.
- B returns to Custom Rules on Additional Rules; Start as on Custom Rules.
- Score Display's visibility is unresolved: the code shows it when save bit `x186C & 1` is set, but a run with every unlock forced (`x186C` 0xFF, online) still did not show it, and Random Stage (bit 2) showed with the bits clear. The reader follows the game's own test, not a fixed row list.

### Item Switch

- `hovered_selection` 0 to 15 is the left column (Food to Flipper), 16 to 30 the right (Freezie to Party Ball), 31 and 32 the frequency strip at the top, entered from the left or the right column.
- Up and Down move within a column and wrap through the frequency strip. Left and Right on items switch columns at the same row (Flipper's row clamps to Party Ball; the outer edge does nothing). On the strip, Left is more (Very High at the left end), Right less, wrapping.
- Values: `MnItemSwData.items[31]` (on or off per display slot) and `x21`, the frequency 0 None to 5 Very High, in `mnItemSw_804D6BE8`'s user data. The display slot to item id map is `mnItemSw_803ED438`.
- The item just toggled lags in `GamePrefs.item_mask` until the next A press or leaving; read the screen's own data, or `confirmed_selection` for the hovered item.
- No description line and no title text. The item names are each column drawn as one multi-line game text (strings 2 and 3; 5 replaces 3 in one language setup); `native-menus.md` calls them pictures, which one study disputes.
- A toggles the hovered item; A on the strip does nothing. B returns to Custom Rules on Item Switch.

### Random Stage

- 29 rows: 0 to 14 the left column (Princess Peach's Castle to Kongo Jungle N64), 15 to 28 the right (Rainbow Cruise to Yoshi's Island N64). Row to stage index is `mnStageSw_803ED4C4`; on or off is bit `s` of `GamePrefs.stage_mask`, which lags for the stage last toggled like the item mask. The screen's data and gobj (`MnStageSwData`, `mnStageSw_804D6BF0`) are static to the file.
- Locked stages show "?????" and are skipped: Up and Down wrap within a column past them, Left and Right go to the nearest unlocked row in the other column. `gm_80164430` honours unlock-all; agent runs with `--no-card` have every stage unlocked.
- A toggles; turning off the last stage that is on plays the refused sound (`lbAudioAx_80024030(3)`) and keeps it on. Every D-pad press plays the move sound, changed or not.
- B saves to the card (`lbCardGame_SaveChanges()`) and returns to Additional Rules on Random Stage.

### The rules header

`mncharsel.c` builds it as game text from pieces in slot 0's table (0x4A to 0x55) plus digits written at runtime: time, coin and bonus "N-minute ..." or the no-limit piece, stock "N-man ...", Camera Mode and Stamina their own text. Stamina has no rules header target.

## Decisions

### Hosting

- **The menu reader serves both scenes.** In `GS_MENU` the existing poll. In `GS_CSS`, `hooks.cpp` feeds the same reader from `pc_a11y_css_frame` while character select's pending exit says the screens are open; no new base port hook. Character select speech drops its own "Custom Rules. No speech yet." line; name entry keeps its notice.
- **Forget on each opening from character select**, so a second visit is an arrival.
- **Descriptions are decoded from the game's text in both scenes**: the string reader accepts the SdSlChr table in `GS_CSS`. Check the item switch and random stage strings and the rule descriptions there; the text dump needs to reach `GS_CSS` to check them.
- **Item and stage names come from fork string tables**, so they do not depend on which table the scene loaded. Random Stage reuses stage select's names (`sss_names.cpp`) through a copy of the row to stage table, with a comment naming the original.
- **Rows follow the game's visibility tests**, not fixed lists.
- **Read only.** The reader reads `mn_804A04F0`, `GameRules`, `GamePrefs` and the screens' data; it writes nothing. Speech stays gated by `pc_net_resim()`.

### Speech: Custom Rules and Additional Rules

- **Opening**: title, row and value, key hint, description. "Custom Rules. Rules: Time. Left and right to change. Compete for points based on KOs and Falls in a timed battle." Additional Rules: "Additional Rules. Stock match time limit: none. Left and right to change. Set a time limit for Stock Matches."
- **Row change**: row and value, then the description. "Handicap: Off. Play without handicaps."
- **Row that opens a screen**: name and description, no hint. "Item Switch. Choose how often items will appear and turn specific items on or off."
- **Value change**: the value, then its description where the description depends on the value. "Turns. Players take turns choosing stages." Rows with one description say the value alone, without the row's name: "5 minutes", "1.2", "4".
- A Rules change that renames the next row says only the new mode with its description; moving down says "Stock: 3".
- **Returning from a sub-screen**: title and row without the hint. "Custom Rules. Item Switch. Choose how often items will appear and turn specific items on or off."
- Gaps: "Unknown row N", logged.

Value words:

| Row | Words |
|---|---|
| Rules | "Rules: Time", Stock, Coin, Bonus |
| Time Limit | "Time limit: 2 minutes", "Time limit: 1 minute", "Time limit: none" |
| Stock | "Stock: 3" |
| Handicap | "Handicap: Off", Auto, On |
| Damage Ratio | "Damage ratio: 1.0", one decimal always |
| Stage Selection | "Stage selection: On", Random, Ordered, Turns, Loser |
| Stock Match Time Limit | "Stock match time limit: none", "5 minutes" |
| Friendly Fire, Pause, Score Display | "Friendly fire: Off", "Pause: On", "Score display: Off" |
| Self-Destructs | "Self-destructs: minus 1", "0", "minus 2" |

### Speech: Item Switch

- **Opening**: "Item Switch. Food: on. Up and down to move, left and right to change column, A to turn on or off."
- **Moving to an item**: "Maxim Tomato: on". Column and position are never said.
- **Toggle**: "Off", "On".
- **The frequency strip**: the first time the cursor reaches it after Item Switch opens, "Item frequency: Medium. Left and right to adjust."; later visits "Item frequency: Medium".
- **Frequency change**: "High". Values None, Very low, Low, Medium, High, Very high.
- Item names as drawn: Food, Maxim Tomato, Heart Container, Warp Star, Ray Gun, Super Scope, Fire Flower, Lip's Stick, Star Rod, Beam Sword, Home-Run Bat, Fan, Hammer, Green Shell, Red Shell, Flipper, Freezie, Mr. Saturn, Poké Ball, Bob-omb, Motion-Sensor Bomb, Super Mushroom, Poison Mushroom, Starman, Parasol, Screw Attack, Metal Box, Bunny Hood, Cloaking Device, Barrel Cannon, Party Ball.

### Speech: Random Stage

- **Opening**: "Random Stage. Princess Peach's Castle: on. Up and down to move, left and right to change column, A to turn on or off." It opens on the first unlocked stage on the left.
- **Moving**: "Corneria: off". Locked stages are never reached, so never said.
- **Toggle**: "On", "Off".
- **Refused toggle**: "At least one stage must stay on."
- A press that changes nothing says nothing, though the game plays its move sound.

### Speech: character select

- **The rules header**: the hand reaching it says "Rules: " and the header's text as drawn, "Rules: 2-minute KO fest!", in place of today's "Rules". Only then: not in the opening, not on returning.
- **Opening the rules screens** speaks Custom Rules' opening.
- **Returning** speaks character select's opening, as today.

### Tests

- `test_menu_speech` covers each screen's opening, row and value changes, value descriptions, rows that open a screen, returns without the hint, the frequency hint once per visit, the refused toggle, wrap, hidden rows and gaps.
- `test_css_speech` covers the rules header's text and the hand-over to the menu reader.
- Drive scripts under `tools/a11y/`: one walks the four screens from the main menu tree; character select's scripts open them from the rules header. `css_slots.drive` and `sss_skipped.drive` lose their "Custom Rules. No speech yet." waits.

### Docs

- `docs/a11y/native-menus.md`: the rules screens' state, the two hosts, the SdSlChr table, whether item names are game text, the Score Display finding.
- ADR-0003: its "second call site" for `GS_CSS` resolved by feeding the reader from character select's frame hook.
- CLAUDE.md: one "Accessibility status" line, and the character select lines no longer say the rules screen has no speech.
- The `verify` skill: new scripts and changed expected lines.

## Open questions

For the implementer to find out:

1. How the reader sees a refused toggle on Random Stage. The poll sees no change; a hook at the refused branch of `fn_80235F80` is acceptable if nothing in the screen's state records it.
2. Whether `confirmed_selection` holds the hovered value on the two switch screens, or the screens' own data must be read (Random Stage's is static).
3. Whether the SdSlChr table holds the same strings at every number the rules screens use, NTSC and PAL.
4. Where to read the rules header's text: the drawn text object, or the same pieces composed from `GameRules`.
5. Why Score Display never showed.

## Issues

1. `issues/01-custom-and-additional-rules.md`: Custom Rules and Additional Rules from the main menu tree.
2. `issues/02-item-and-stage-switches.md`: Item Switch and Random Stage. Blocked by 01.
3. `issues/03-character-select.md`: the screens inside character select, and the rules header's text. Blocked by 02.
4. `issues/04-play-test.md`: the maintainer, by ear. Blocked by 03.

Docs are part of the issue they belong to.
