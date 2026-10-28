# Match announcements: damage, stocks and the time left

Status: agreed 2026-10-28 (design conversation with the maintainer)

The maintainer agreed:

- Damage said once it stops changing, or only past thresholds, set separately for you and for the others, or off.
- Stocks said as they change, set separately for you and for the others; eliminations left to the game's announcer.
- Teammates in the opponents' voice, labelled, behind their own switch.
- Player labels: a name tag where a player has one, otherwise "P2" or "CPU 3", only when the match started with two or more others.
- The time left on the screen reader, at each whole minute and at 30 seconds, in countdowns only.
- Players announced only while the HUD shows their panel.

The wording is a first proposal for the play test to judge.

## Goal

A blind player in a match hears their own damage and stocks in their voice, and everyone else's in the opponents' voice, as the HUD shows them, without a line for every hit. They hear how much time is left often enough to pace a timed match. Each part can be turned down or off, for themselves and for the others separately.

Builds on `.scratch/voices/spec.md`, which must land first. Vocabulary is `CONTEXT.md`: match announcement, voice, your voice, the opponents' voice, player label, settled damage, damage threshold, announcement, setting. What the game knows during a match is in `docs/a11y/gameplay-state.md`.

## Scope

In:

- **Damage** and **stocks** of every player whose HUD panel shows: VS and every Special Melee mode, Training, online play, and the single-player modes (Classic, Adventure, Event, All-Star) wherever their fighters have panels.
- **The time left** in every match whose timer counts down: timed VS, stock matches with a time limit, ranked online, Event matches.
- The settings rows that control them.

Out, each a later issue:

- **Stamina mode**: the HUD shows HP counting down from 150 instead of damage (`Player_GetRemainingHP`). The same rules on the shown HP, settling or crossing thresholds downward.
- **Time-match scores**: the +1 and −1 a KO gives in a timed match.
- **Coin matches**.
- **Pan**: the opponents' voice is centred (the voices spec).
- **Asking by a key** for your damage, the others' or the time left. Every controller button is a game input during a match, so this needs its own design.
- **Online teams**: online play is one against one today, so no online match needs a player label.

## What the game gives us

The findings below come from reading the decomp and the fork's match code on 2026-10-28. Address-style names are the decomp's.

### Damage

- **What the HUD shows** is `Player_GetDamage(slot)` (`src/melee/pl/player.c`): the slot's damage as a whole number, truncated from the fighter's float `Fighter.dmg.x1830_percent`, which can be up to 1 higher. The HUD copies it every tick (`ifStatus_802F5B48`, `src/melee/if/ifstatus.c`) and draws the digits straight from it: no counting up. A rise shakes the digits; the colour darkens toward 300%.
- **It can fall.** A healing item takes 1% off each tick until it is spent (`fighter.c`); Ness's PSI Magnet absorbs; Training's menu sets it; a KO sets it to 0 (`ftCo_800D34E0`, `ft_0D31.c`), and the respawned fighter reads it back from the slot.
- **Starting damage** other than 0 is set with `Player_SetHUDDamage`: sudden death's 300%, events, Training.
- **Ice Climbers**: `Player_GetDamage` returns the active fighter's, Popo's.

### Stocks

- **The count** is `Player_GetStocks(slot)`. `Player_LoseStock` takes one at a KO, in stock matches only, inside `ftCo_800D34E0`: on the KO tick for a side or bottom KO, at the end of the animation for a star or screen KO.
- **It can rise.** In a team match a player with no stocks left presses Start to take one from a teammate with more than one (`fn_8016B918`, `gmvs.c`).
- **Elimination.** At 0 stocks the game's announcer calls the player out about 50 ticks later (`gmvs.c`, the `fighters[i].x1` countdown).
- **The HUD** shows up to five stock icons, then one icon and a count; a timed match shows no stocks.

### The timer

- **Where it is**: `VsSceneState` (`src/melee/gm/types.h`), through `gmVs_GetSceneController()->state`. `timer_seconds` is the seconds left on a countdown; `unk_2C` the ticks within the second, starting at 59. The timer advances before the procs run, so the tick hook sees it fresh.
- **Which matches have one**: `StartMeleeRules` (`gm_GetStartMeleeRules()`): `timer_enabled`, `timer_counts_up` (Multi-Man, All-Star), `time_limit`. A stock match with a time limit has a countdown; with none it has no timer. Ranked online forces a countdown.
- **The HUD** shows "MM:SS.cc"; at five seconds left it swaps to large countdown digits, and the game's announcer says five to one, then "Time!". Nothing marks one minute or 30 seconds.
- **It stops** while the match is paused, and before GO (`hud_enabled`).

### Phases

`VsSceneState.unk_0`: 0 while the match runs (paused included), 1 from GAME! or TIME!. GO enables input and, as it ends, the HUD.

### Players and labels

- **Over each fighter** the game draws "1P" to "4P" by controller port, or "CP" for every computer player with no number. A human with a name tag has the tag drawn instead. CPUs never have one.
- **Name tags**: `Player_GetNametagSlotID(slot)`, 120 for none, and `GetNameText` for the up to eight bytes of Shift-JIS text. The fork already decodes them for results (`results_opponents` in `game_access.c`, `decode_shift_jis` in `game_text.cpp`). Online each copy of the game looks the index up on its own memory card, so the peer's tag is not reliable.
- **The HUD's panels** are one per slot, left to right; in VS and online slot n is port n.
- **"You"**: `match_your_slot(local_port, online)` (`game_access.c`), as the cues use: port 1's slot offline, the local player's online. Teams: `is_teams` and each slot's team, as the cues' nearest opponent skips teammates.

### The tick hook

`pc_a11y_match_tick` runs after the game's procs on every tick, gated by `game_hook_may_speak()` (no speech while `pc_net_resim()` is true). The cue system already reads `a11y_game_match_state` there. No match speech exists yet besides Training's menu.

## Decisions

### How the fork attaches

- **No new hooks.** A match announcer is read from `pc_a11y_match_tick`, as Training's menu and results are, from a snapshot in `game_access.c` of each slot's shown damage, stocks, panel, port, kind, team and name tag, and of the timer and the match's phase.
- **It reads, never writes**: rollback needs the simulation untouched.

### When announcements run

- **From GO** (the HUD enabled) **to GAME! or TIME!**. At GAME! or TIME! damage still waiting to settle is dropped; a line already playing finishes.
- **The baseline.** Each player's damage and stocks at GO are taken as already said, so a match starting at 300% or with Training's set damage says nothing until the value changes.
- **Paused** nothing changes, so nothing is said; the voices' queue plays out. The port menu and the switches act on the voices as the voices spec says.

### Who is announced, in which voice

- **You** in your voice.
- **Everyone else whose HUD panel shows**, in the opponents' voice: opponents, and teammates behind the Teammates switch. A fighter without a panel (Multi-Man's wireframes, a Classic stage's team of Yoshis) is never announced. That follows "say what the screen shows" without a list of modes.
- **No "you"** (four computer players offline): everyone is in the opponents' voice, labelled.
- **Each line's subject**, for the voices' queueing rules, is the player's slot: a new line about a player replaces an older one about the same player, whether damage or stocks.

### Player labels

- **When.** A line in the opponents' voice starts with the player's label when the match started with two or more players besides you, teammates included. One opponent and nobody else: no label, the voice says whom. The choice is made at GO and kept, so the wording never changes as players are eliminated.
- **What.** The name tag, decoded, for a human with one, offline only; otherwise "P" and the port for a human ("P2"), "CPU" and the slot for a computer player ("CPU 3"), whose screen label "CP" has no number. A teammate's label starts with "teammate": "teammate P3", "teammate Bob".
- **Never the character's name.**

### Damage

- **Settled damage.** A player's damage is said once it has held still for the settle time (a tuning value), with no maximum wait: a long combo says nothing until it ends. The figure is the one the HUD shows.
- **Three modes**, set separately for you and for the others: Off, Every change, Thresholds.
- **Every change** says the settled damage whenever it differs from the last figure said for that player, rises and falls alike (a healing item says its end figure once).
- **Thresholds** says the settled damage only when it has crossed a damage threshold upward since the last figure said: from 35 to 52 with an interval of 20 says "52"; from 35 to 85 says "85" once. A fall is silent but lowers the figure the next crossing counts from, so 85, healed to 55, hit to 65 says "65".
- **A KO** resets the player's last figure to 0 silently: the respawn's 0% is never said. In a stock match the stock line says what happened; in a timed match nothing does (scores are out of scope).
- **The words**: the figure alone, "45", or after the label, "P2 45". The HUD's "%" is left out: the voice already says it is damage, and speed matters mid-match.

### Stocks

- **Said at once**, on the tick the count changes, rises and falls alike, in stock matches only: "2 stocks", "last stock" at one, after the label where there is one: "P2, 2 stocks", "CPU 3, last stock".
- **At 0 nothing is said**: the game's announcer calls the elimination.
- **On and off** separately for you and for the others; a teammate's follow the others' and the Teammates switch.

### The time left

- **On the screen reader, queued**, so it never cuts off what the screen reader is saying.
- **At each whole minute** of a countdown, "2 minutes left", "1 minute left", and at 30 seconds, "30 seconds left", on the tick the HUD shows that time. The match's starting minute is not said: the rules header said it.
- **Countdowns only.** A timer counting up (Multi-Man, All-Star) says nothing. The game's own five-to-one countdown is left alone.

### The settings rows

In the Accessibility category, after the voices' rows (the voices spec), in this order:

- **Match announcements**: a switch over every row below.
- **Your damage**: a choice, Off, Every change or Thresholds; Every change by default.
- **Opponents' damage**: the same choice, the same default.
- **Damage interval**: a slider of the threshold spacing, 10 to 100 in steps of 10, 20 by default; shared by both damage rows.
- **Your stocks**: a switch, on by default.
- **Opponents' stocks**: a switch, on by default.
- **Teammates**: a switch, on by default: whether teammates are announced at all.
- **Time left**: a switch, on by default.

The settle time is a tuning value. The descriptions are the implementer's first proposal. Match announcements sit under the Speech switch, and Accessibility above it.

### The speech log

Every match announcement is one `[a11y]` line naming its voice (the voices spec); the time left is an ordinary screen reader line. A damage figure dropped at GAME! is logged as dropped.

### Online

- **Gated** like all speech: silent while `pc_net_resim()` is true. A predicted tick that is re-run may have changed a damage figure; the settle time hides it, as nothing is said until the figure holds.
- **No labels** online, as every online match is one against one, so the peer's unreliable name tag never matters.
- A base port peer is unaffected: the fork only reads.

### Tests

- **A unit test of the announcer** (fake clock, snapshots by hand): settling, rises and falls, a heal, a KO reset, the GO baseline (300%), thresholds with one and several crossings and a fall then a crossing, stocks with a loss, last stock, 0 and a steal, labels (one opponent, two, a teammate, a name tag, a CPU, no "you", fixed at GO), panels (a fighter without one), the time left (each minute, 30 seconds, the starting minute, counting up), GAME! dropping a pending figure, each row and switch.
- **Drive runs**: a debug match (`MELEE_DEBUG_VS=cpu` with `MELEE_DEBUG_VS_STOCKS`) for damage and stocks against a CPU that attacks; a timed match for the time left; `MELEE_DEBUG_VS=cpu4` for labels with no "you". The expected lines go into the `verify` skill.
- **An online run** through `tools/a11y/drive_net.py`, fork against fork and against the base port (`--base-b`): both say their own and the other's damage and stocks, unlabelled, and the session stays in sync.
- **The play test**, by ear.

### Docs

- `docs/a11y/gameplay-state.md`: the HUD's damage, stocks, timer and labels as found here, and the answers to the open questions.
- `docs/a11y/speech.md`: a short pointer from "Voices" to what speaks through them.
- `CLAUDE.md`: one "Accessibility status" line.
- The `verify` skill: the drive runs, the online run and their expected lines.

## Open questions

For the implementer to find out:

1. **Whether a slot's HUD panel shows.** Which flag of `ifStatus_HudInfo` (or the slot's state) tells it, in every mode in scope, and whether it holds through a respawn.
2. **A KO in a timed match.** What marks it for the silent reset, with no stock lost: the fighter's KO states (`motion_id` 0 to 11, as the cues use), or the slot's falls count.
3. **The timer's tick.** On which tick the HUD first shows "N:00.00" (`timer_seconds` and `unk_2C`), whether single-player and Event matches use `VsSceneState` and `StartMeleeRules` the same way, and whether sudden death has a timer (the code reads as none).
4. **Teams.** Where a slot's team is read during a match, and whether a team stock steal can make a player's stock count rise on a tick where their panel is not yet back.
5. **Training.** Whether Training's menu setting a CPU's damage should be said once the menu closes, as Every change would, or is already said by the menu's own reader.
6. **Name tags on stock steal and respawn.** Whether `Player_GetNametagSlotID` is stable through the match (the label is fixed at GO anyway).

## Issues

1. `issues/01-snapshot-and-damage.md`: the match snapshot, the Match announcements switch, and damage on every change, with labels, for you and the others. Blocked by `.scratch/voices/issues/02-voice-engine.md`.
2. `issues/02-thresholds-and-damage-rows.md`: threshold mode and the damage rows. Blocked by 01 and `.scratch/voices/issues/01-choice-values.md`.
3. `issues/03-stocks-and-teammates.md`: stocks and the Teammates switch. Blocked by 01.
4. `issues/04-time-left.md`: the time left. Blocked by 01.
5. `issues/05-drive-scripts-and-docs.md`: the drive scripts, the online run, the `verify` skill, the docs and the status line. Blocked by 02, 03 and 04.
6. `issues/06-play-test.md`: the maintainer, by ear. Blocked by 05.
