# 04 The time left

Status: resolved (2026-10-28)
Type: task
Blocked by: 01

Spec: `.scratch/match-announcements/spec.md`, "The time left".

## What to build

- **The snapshot** gains the timer: `timer_seconds` and `unk_2C` from `VsSceneState`, and from `StartMeleeRules` whether the timer is on and whether it counts up.
- **On the screen reader, queued**: "2 minutes left", "1 minute left" at each whole minute of a countdown, and "30 seconds left", on the tick the HUD shows that time (open question 3). The match's starting minute is not said. A timer counting up says nothing; the game's five-to-one countdown is left alone.
- **The row**, last of the match announcements: "Time left", a switch, on by default.

## Done when

- The announcer's unit test covers each minute, 30 seconds, the starting minute, a limit that is not a whole minute, counting up, no timer, a pause (nothing said twice), and the switch.
- A drive run of a timed match shows the lines at the right times (the `verify` skill names how to reach one).
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
- `docs/a11y/gameplay-state.md` has the timer as the spec found it, with the answer to open question 3.

## Comments

### 2026-10-28, implementation (agent)

Implemented; a countdown says the time left at each whole minute and at 30 seconds, on the screen reader, queued, behind a new Time left row.

- **Where things live.** `game_access.c`: `A11yMatchHud.timer`, `timer_counts_up` (`StartMeleeRules`), `timer_seconds` (-1 for a countdown with no limit) and `timer_frames` (`VsSceneState.unk_2C`). `match_announcer.hpp/.cpp`: `time_left()`, run each tick of a running match before the players, `countdown()`, the time the HUD shows in ticks, and `m_time_left`, the time last tick, taken at GO; `MatchAnnouncementValues::time_left`. `match_announcement_settings.hpp/.cpp`: the switch `match_announcements.time_left` ("Time left", on), registered after Teammates and before the settle time. No hook or `hooks.cpp` change.
- **The rule as built.** The countdown's time in ticks is `timer_seconds * 60 + 59 - timer_frames`, which falls by one a tick and is a whole minute exactly when the HUD shows "N:00.00". A line is said when that time reaches a mark below the time last tick: a whole minute ("2 minutes left", "1 minute left") or 30 seconds ("30 seconds left"); if several were passed at once, the latest. The time at GO is the baseline, so the starting minute is never said. A time standing still (paused, the HUD hidden) crosses nothing, so a minute is never said twice; a time set back up (an event's next stage) becomes the new baseline. Off (the row or the Match announcements switch), a mark passes unsaid, so turning it on says only the next. Through `Speech::announce` with `Mode::queue`, logged `[a11y] speak queue: "1 minute left"`.
- **Open question 3, the timer's tick: "N:00.00" shows while `unk_2C` is 59, the tick before the second comes off.** `fn_8016CD98` (`gmvs.c`) advances the timer before the procs, only while `hud_enabled` and not paused; `unk_2C` counts up 0 to 59 on a countdown too, starting at 59, and as it wraps to 0 a second comes off; the HUD's hundredths are `99 - 99 * unk_2C / 59` (`gm_8016AF0C`). Measured in VS's default two-minute time match: `"1 minute left"` and `"30 seconds left"` 30.0 s apart, TIME! 30.0 s after the second, no `"2 minutes left"`.
- **Single-player and Event matches** run on the same `VsSceneController` (`fn_8016E730` copies the rules in), so they read the same way. Events take both flags and the limit from the event's data, and an event of several stages carries its time over (`gmevent.c`), so a match can start at a time that is not a whole minute; one Adventure stage sets `timer_seconds` as its match starts (`fn_8017C7EC`).
- **Sudden death has no timer.** `gm_SetupSuddenDeath` clears `timer_enabled`, and VS never sets `rules.x1_0`, the other flag `ifTime_CreateTimers` draws a timer for, so the call in `gm_Scene_SuddenDeath_OnEnter` makes none. Found by reading; no sudden death was played.
- **A finding against the spec's premise: 3-Minute Melee counts down** (`gmMultiman_InitTimedRules`), so it says "2 minutes left", "1 minute left" and "30 seconds left"; the other Multi-Man modes and All-Star count up and say nothing. Home-Run Contest counts down from 10 seconds, so says nothing.
- **Descriptions (first proposal).** Time left: "Says the time left at each whole minute and at 30 seconds, in a match whose timer counts down." The Match announcements switch now ends "..., and the time left on the screen reader."
- **Tests.** `match_announcer` gains each minute and 30 seconds of a three-minute match on the tick the HUD shows them and none in the voices, a one-minute match (30 seconds only), a limit that is not a whole minute, a timer set back up partway through, counting up, no timer, a countdown with no limit, a pause and the HUD hidden at a mark (said once), the Time left and Match announcements switches off and on, and nothing after TIME!; the rows test checks the row's place, kind, label, default and file order. The fake screen reader now records each line with its mode.
- **Drive scripts.** `launcher_a11y.drive`, `port_menu_a11y.drive` and `port_menu_voices.drive` read the new row after Teammates and the switch's longer description; their counts past it grew by one. The `verify` skill has the unit test's cases, the row and the timed match by hand (VS's default rules through the menus, about two and a half minutes).
- **Verification**, quick and menus levels with `MELEE_A11Y_CUES=1`: build with the "Rollback sections" line, style, unit tests 34 of 35 (`launcher_data`, known), `run_level.py menus` with the seven match scripts 36 of 36 (no time line in any of them), and the timed match twice, as above. The online level was not run: the snapshot only reads, and nothing online-specific changed; issue 05's online run covers it, though ranked's countdown is not reachable there.
- **For issue 05.** The timed match is the `verify` skill's by-hand command, ready to become a script (`match_time.drive`?); at about two and a half minutes it would be the longest in the menus level, so it may belong in a level of its own or run with a one-minute limit set on Custom Rules (30 seconds only, about 1.5 minutes). `docs/a11y/gameplay-state.md` has "The timer on the HUD".
- **Left open.** The play test: whether the minutes are often enough, and whether the line should wait for the screen reader to be quiet (it is queued, so it never cuts off a line, but a long line before it delays it).
