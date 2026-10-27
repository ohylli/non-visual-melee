# 06 Play test by ear

Status: resolved (2026-10-27)
Type: task
Blocked by: 05

The maintainer, with NVDA, in the launcher and the port menu:

- the Accessibility category's place and its rows, labels and help texts
- each switch, including turning Accessibility and Speech off and back on from the same row
- the volumes, and Ledge warning distance in a match
- each Preview: whether it sounds like the cue in a match, and whether two seconds is right

## Comments

### 2026-10-20, code review (agent)

The review of 38f8bd8..HEAD fixed what was plainly the agent's (its own commit, "Accessibility settings: review fixes"). Left for the maintainer, most severe first:

- **Fork off is no longer the base port in a match.** The cue engine always opens, so the match tick reads the match and calls the floor raycast (`mpCheckFloor`) every tick even with Accessibility or Cues off (`CueSystem::tick`). ADR-0006 holds that the raycast leaves nothing changed, so online stays safe, but "Accessibility off" no longer rules the fork out when chasing a desync. Options: skip the match read while the switches leave the cues off (the voices then restart when Cues comes back, rather than resume where the match is), or keep it. Recommendation: skip it while Accessibility is off and no override holds Cues on.
- **"Speech on", then the row.** Turning Speech or Accessibility back on from its row says "Speech on" (interrupting), then the reader queues the row: "Speech, On, Left and right to change, ...", "on" twice in two announcements (`Switches::update`, `PageReader::follow_focus`). Options: leave out the switch's words when a page reader holds focus on a row, keeping them for hand edits; or keep both. Recommendation: let the row speak alone in the menus. Issue 02's "Left open" names it too.
- **Speech back on from the port menu.** Over the main menu tree, `MenuSpeech::forget` makes the tree's opening interrupt "Back to game" one frame after the close; over character select, stage select or the Training menu nothing says where the player is after "Back to game". Options: queue the tree's re-opening instead of interrupting; give the other readers a `forget` like the tree's. Recommendation: judge both by ear first.
- **`MELEE_A11Y=1` does not undo a saved Speech off.** It overrides only `accessibility.on`, so a player who turned Speech off (not Accessibility) and quit starts silent with editing `a11y.ini` the only way back. Options: let `MELEE_A11Y=1` also hold Speech on for the run; or say so in the player-facing readme. Recommendation: hold Speech too.
- **"Saved." after a save that failed.** The rename is now retried, but if it still fails the port menu's status line says "Saved." (`Page::change` checks `saves()`, not the save), the value lives only until the game closes, and the failure is only logged; nor is the file flushed before the rename. Options: return the save's result through `AccessibilitySettings::set` to the page and announce a failure; or leave it to the log. Recommendation: return it, an interface change across three modules.
- **Ledge warning distance steps 1 unit over 5 to 100**, so a big change takes dozens of presses (`MatchCues::add_ledge_values`). A tuning matter for the play test.

### 2026-10-27, play test (maintainer)

The maintainer ran the play test by ear with NVDA in the launcher and the port menu. The one finding: each cue's switch hint said only that it turns the cue on or off. Fixed in a687a7b, each cue's hint now says what the cue does. The code review's points above were not raised by the play test and stay open.
