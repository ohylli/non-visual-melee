# 07 Play test by ear

Status: resolved (2026-10-19)
Type: task
Blocked by: 06

The maintainer plays the port menu with NVDA and judges the wording of `.scratch/port-menu/spec.md`.

## What to try

- Open the menu with F1 from a native menu and mid-match, and with Back on a controller. Is "PC settings" before the tab a good opening? Do the cues stop at once?
- Close it each way: Escape, F1, B, Start, Return to game. Is "Back to game" right, and do the cues come back?
- Every tab: settings, sliders with Left and Right, a slider with Enter (it wraps), the lines on Controls and Online, the text fields.
- Changes that put news on the status line: a graphics backend change, Restore defaults on Controls, Check now on the update row. Is anything missing because "Saved." is left out, or is something spoken that should not be?
- The Controls tab as the general reader leaves it. Note what the remapping feature must fix, beyond the spec's list.

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue. CLAUDE.md's status line says the port menu was play-tested.

## Comments

### 2026-10-19, code review (agent)

Left for the maintainer, from the review of issues 04 to 06; worth listening for in the play test:

- **A status text written again unchanged is not spoken.** The reader speaks a watched region only when its text changes, so pressing Restore defaults a second time, Enter on a second binding row with no controller ("No controller in port 1." again) or a repeated save error gives silence; the button's own value does not change either. The launcher has the same gap. Options: treat a rewrite as a change (the base port's `label` makes a new text node every time, so the reader could compare the region's first child as well as its text), or leave it. Recommendation: treat a rewrite as a change, on both pages.
- **"Backend set to ... (restart required)." is never heard.** `PortMenu::apply("backend")` writes it, then falls through to `saved()`, which writes "Saved." over it in the same frame (`src/pc/launcher.cpp`), so the player hears the new backend but not that a restart is needed; the same for "Backend set by MELEE_BACKEND environment.". A base port bug. Options: a fix in its own unmarked commit (return after the label once the setting is saved), or a report upstream. Recommendation: the fix, since it helps every player.
- **An update check's result can go unheard.** Check now writes "Checking for updates...", and the base port writes the result only at the next refresh. If the player closes and reopens the menu first, the refresh comes with the opening, which takes both regions silently, so the result is only in the Check for updates row's help text. Options: queue the result after the opening when it changed while the menu was closed, or leave it. Recommendation: queue it.
- Already in issue 04's findings: Escape in an Online field closes nothing and says nothing, and a port change says only "Port 2", not the controller's name.

### 2026-10-19, play test (maintainer)

Played by ear with NVDA: no problems found. The review's remarks above are acceptable for now and stay open without issues of their own. The unheard "Backend set to ... (restart required)." matters little, since the backend row's help text already says a change needs a restart.
