# Results: speech

Status: agreed 2026-10-27 (design conversation with the maintainer)

The maintainer agreed:

- The scope: results in every mode that uses the scene.
- Silence on entry, and an opening once the summary table is built.
- What the opening says and in what order.
- A reading cursor on the D-pad over the stats panels, with Z for a player's summary line.
- What paging, stepping and switching players say.
- Ready speech.
- Quick chat giving up the D-pad on results.

The wording is a first proposal for the play test to judge.

## Goal

A blind player on results can hear who won and how every player placed. They can read every player's stats and bonuses row by row, comparing a stat across players. Then they can ready up and hear whether they are waiting on someone.

Vocabulary is `CONTEXT.md`: results, summary table, stats panel, page, bonus, ready, reading cursor, announcement. How the game's menus run and its game text are in `docs/a11y/native-menus.md`.

## Scope

In:

- **Results**, the scene `GS_RESULTS`. VS, every Special Melee mode, Tournament, online play and the debug VS match all reach the same scene, so one reader serves them all.

Out:

- **Classic, Adventure and All-Star's end screen** (`gmregclear.c`, string table SdClr). It is a different scene, a later feature. It shares the bonus table (`fn_8016F1F0`), so this feature's bonus names should be reusable there.
- **The winner's announcer and victory music**: the game's own, left alone.
- **What comes after results**: character select, an unlock notice or a new challenger. Each is its own scene.
- **Reading incoming quick chat aloud**: a later feature for every screen where chat runs.
- **Local multiplayer for a second blind player**: only controller 1 (offline) or the local player (online) gets the reading cursor.

## What the game gives us

The findings below come from reading `src/melee/gm/gmresult.c`, `gmresultplayer.c`, `gm_17AD.c`, `gm_17BA.c` and `gm/types.h`, and from five driven runs on 2026-10-27. Address-style names are the decomp's. Anything marked *unsure* is for the implementer to confirm.

### State

The state lives in two places:

- **`ResultsData lbl_8046DBE8`**: a non-static global in `gmresult.c`, `extern` in `gmresultplayer.c`. It holds:
  - `x1`, the scene's state, 0 to 4;
  - `x0_23`, a sub-phase of state 3;
  - `x8`, frames since entry;
  - `x4`, `x5` and `x6`: the winner, the winning team and the winner shown;
  - `num_pages`: 3, or 2 when the match was cancelled;
  - `x94`, a pointer to the `MatchEnd`;
  - `player_data[4]`: per slot, `x0_0` (ready), `page`, `scroll_offset`, the arrow flags and the stat text objects.
- **`MatchEnd`** (`gm/types.h`): `outcome`, `match_kind` (Time, Stock, Coin or Bonus), `is_teams`, `sd_penalty`, `team_standings[]`, `player_standings[6]` (`MatchPlayerData`: place in `is_big_loser`, KOs per victim, falls, SDs, KO time, score, coins and every stat) and `x44C[6]`, each player's bonus flags and values.
- **Slots and ports**: slot *i* is port *i*.
- **Static values**: two are static and must be recomputed rather than read. `lbl_8046E190` holds the bonus row counts, `lbl_804D3FC8` whether no human is playing.
- **A function not to call**: `fn_80174284` records bonuses as a side effect.

### Flow

| State | What happens |
|---|---|
| 0 | The announcer runs on a frame count: "This game's winner is…" at frame 2, then the name, or a team's name, or "No contest", at frame 154 (`gm_80168C5C`). It leaves at frame 160; on a cancelled match, any human's Start leaves at once. |
| 1 | The panels animate in. No input. |
| 2 | **The first point where a press counts.** Any new press from any connected human, D-pad and sticks included, opens the stats panels of every player (`fn_80177920`). Silent. |
| 3 | The stats panels, worked per slot by that port's controller (`fn_80178050`): |

State 3's controls, for a human who is not ready:

- **Paging:** A or stick right goes to the next page, B or stick left to the previous one, with the move sound and the scroll reset. At either end the press is silent.
- **Scrolling:** the analog stick scrolls by fractions of a row, silently.
- **Start** readies the player with the confirm sound and closes their panel to "READY FOR THE NEXT BATTLE". Start again un-readies them with the back sound and brings the panel back on the same page.
- **Ignored:** the D-pad, L, R, Z, X, Y and the C-stick.
- **CPU and empty slots** are ready from the start. A CPU's panel pages and scrolls by itself.

When every panel is ready, a short delay follows (state 4), then the scene leaves.

### Content

**The summary table** (state 2) shows:

- The match type as game text: SdRst strings 2 to 5 for Time, Stock, Coin and Bonus ("Stock Battle", "Time Battle").
- The winner's name, the team's name or "NO CONTEST", all drawn as images.
- One column per port, with rows KOs, Falls and SDs.
- A last row that depends on the match type:
  - **Stock:** "KO'd", the time the player was knocked out, or "—:—" for a survivor.
  - **Time:** "Total", ±N.
  - **Coin:** coins.
  - **Bonus:** the bonus total. Bonus mode shows no KOs, Falls or SDs rows.
- Per port: the place as an image (1st to 4th, ties shared, none on No Contest), a portrait, and the character's name as an image. An empty port shows "NO ENTRANT".

**The stats panel's pages:**

| Page | Rows (SdRst strings by number) |
|---|---|
| 0, KOs and falls | "KOs", then one row per other player (KOs you dealt them); "Falls", one row per other player (times they KO'd you); "SDs" with yours. An opponent appears as "P 2", or as their name tag. |
| 1, Stats | 23 stats, each a label string and a value: KOs, Falls, SDs, DamageGiven, DamageTaken, DamageRecovered, PeakDamage, FlightDistance, GroundTime, AirTime, HitPercentage, GroundAttacks, AirAttacks, SmashAttacks, Attempted Grabs, Throws, Cliffhangers, ItemsThrown, ItemsGrabbed, TopSpeed, FastestPitch, LongestDrought, Transformation Time. Distances are in feet on a US language setting. |
| 2, Bonuses | Each bonus is a name (SdRst `gm_8016F208(kind) + 58`) and its points. Absent on a cancelled match. Bonus mode opens every panel on this page. |

The labels are drawn without spaces, as listed. The exact strings, the unit strings and the PAL indexing are *unsure* until decoded at runtime.

### Reading game text

- `a11y_game_string(font_idx, idx)` in `game_access.c` reads any loaded string table. On results, font 0 is `SIS_ResultData` (SdRst).
- `menu_string` accepts only the menus' tables, so results needs an accessor of its own, with its own symbol check.
- `pc_region_sis_index` has no PAL shift for `SIS_ResultData`.
- `MELEE_A11Y_TEXT_DUMP` (`menu_text.cpp`) dumps only on menu and character select frames.

### Hosting

- **The tick hook already runs on results.** `pc_a11y_match_tick` runs after every tick in every scene, results included, as the Training menu reader uses it.
- **Polling catches every transition.** Comparing the state, the sub-phase, each slot's page and ready flag, and controller 1's buttons from tick to tick catches every transition this feature speaks.
- **The scene hook comes too early to speak.** `pc_a11y_scene_entered` runs after the scene's OnEnter, with `MatchEnd` ready. Its words today overlap the announcer.

### Online

The online mode reuses the scene. Outside fights the netcode runs in lockstep (`net.c`), so results is never re-run and `pc_net_resim()` stays false there.

- **Ports:** both copies simulate the same screen. Port `pc_net_local_player()` is the local player, the other is the remote player, and ports 2 and 3 have no controller.
- **Panels:** each player pages and readies their own panel. Either player's first press opens the stats on both machines.
- **Quick chat:** live on results, using controller 1's D-pad.
- **Disconnect:** a lost peer leads to the lobby, which `lobby_speech` already handles.

## Decisions

### Hosting

- **Polled from the tick hook.**
  - A reader in `src/pc/a11y/` gets a snapshot of results each tick from `pc_a11y_match_tick`. The snapshot is read by `game_access.c` from `lbl_8046DBE8` and its `MatchEnd`.
  - The reader also sees controller 1's newly pressed buttons: the local player's online, port 0's offline.
  - Results needs no new hook in a base port file beyond what is said under "Quick chat".
- **Read only.** The reader writes nothing in the game. The reading cursor is the fork's own state.
- **The usual speech gate.** The reader stays behind `game_hook_may_speak()` like every other reader, which includes the `pc_net_resim()` check.
- **Scene words.**
  - `scene_speech.cpp`'s results entry becomes silent: the reader speaks the opening.
  - The online results entry, which carried "then wait for your opponent", goes, since ready speech covers it.

### "You"

- **Who you are.** You are controller 1's port offline and `pc_net_local_player()` online.
- **How it's said.** "you" follows your port in the opening, in a player switch and in an opponent row on page 0.
- **When nobody is "you".** If your port is a CPU or an empty slot (the debug match with four CPUs), nobody is "you", and the reading cursor starts on P1.

### The opening

- **When.** The opening is said once, when the scene reaches state 2, where presses start to count: about 3.3 seconds in, after the announcer has named the winner. On a No Contest it comes as soon as Start has skipped the wait. Nothing is said on entry.
- **What, in order:**
  1. The match type, in the game's words: "Stock Battle."
  2. The winner: "Winner: DK." In a team match, "Winner: Blue team." On a No Contest, "No contest." with no winner.
  3. Each player in place order: place, character, port, "you", team in a team match, KOs, falls, SDs, and the last row. On a No Contest the order is port order and there are no places. Bonus mode has no KOs, falls or SDs.
  4. "Start when ready."
- **The last row:**
  - **Stock:** "KO'd at 1:46", or "not KO'd" for a survivor (the screen's "—:—" fails by ear).
  - **Time:** "total +2", "total -1" or "total 0".
  - **Coin:** "120 coins".
  - **Bonus:** "19400 points".
- **Grammar:** counts take singular and plural: "1 KO", "2 KOs", "1 fall".
- **The keys** are added before "Start when ready." on the first results of each game run only: "Any button shows the stats."
- **An example** (four players, Stock, you are P1):

  > Stock Battle. Winner: DK. 1st DK, P4, 2 KOs, 0 falls, 0 SDs, not KO'd. 2nd Link, P1, you, 1 KO, 1 fall, 0 SDs, KO'd at 1:46. 3rd Fox, P3, 0 KOs, 1 fall, 0 SDs, KO'd at 1:27. 4th Mario, P2, 0 KOs, 1 fall, 0 SDs, KO'd at 1:16. Start when ready.

- **Names:**
  - Characters come from the fork's character names (`css_names`). Zelda and Sheik follow the character the player ended as, as the game does.
  - Teams are "Red team", "Blue team" and "Green team".
  - A player with a name tag is named by the tag where the screen shows it (page 0's opponent rows); the summary uses the character.
  - Places are "1st" to "4th"; tied players share a place.
- **Any press interrupts the opening.**

### The stats panels and the reading cursor

**Paging** stays the game's: A, B and the stick.

- **Your page changes:** the page's name and its first row: "KOs and falls. KOs.", "Stats. KOs, 1.", "Bonuses. Switzerland, 12000."
- **The stats open**, by your press or anyone else's: the same announcement for your panel's page. On the first results of the run, the keys follow: "A and B change page. D-pad up and down read the rows, left and right switch player. Z the player's summary. Start when ready."
- **A on the last page, B on the first:** silent, as the game is.
- **Another player's paging or scrolling:** silent. The analog scroll is never spoken.

**The reading cursor** belongs to controller 1. It is a row on your panel's current page, for one player. It starts on your own stats; a page change moves it to that page's first row and keeps the player.

- **D-pad up and down** step through the rows and say the row. At the first or last row, the row is said again.
- **D-pad left and right** switch players: you first, then the others by port: CPUs are included, empty ports skipped. At either end the edge player is said again.
  - The cursor keeps its row number, clamped to the new player's list.
  - The announcement is the player ("Mario, P2", with "you" for yours) and then only what differs from the row you left.
- **Z** says the cursor's player's line from the summary: "Mario, P2, 4th, 0 KOs, 1 fall, 0 SDs, KO'd at 1:16."
- **Before the stats are open**, a D-pad or Z press is only the game's "any press" that opens them. The reader says the page announcement, not a row.

**Rows**, one announcement each:

| Page | Rows |
|---|---|
| KOs and falls | "KOs" alone; one row per opponent: "P2 Mario, 1" (a name tag in place of "P2" where the screen shows one; "P1 Link, you, 0" on an opponent's list); "Falls" alone; the same opponent rows; "SDs, 0". Headings are rows of their own, as on screen. |
| Stats | "Damage given, 87%": the game's label with a space before each inner capital, then the value with the game's unit. A value the game shows as "---" is said as "none" (*unsure* it appears). |
| Bonuses | "Switzerland, 12000": the game's name as written, and the points, "-1000" when negative. |

**Switching players: what differs.** In this example the cursor is on your stats (P1 Link) and you press right:

| Cursor on | Said |
|---|---|
| Stats, "Damage given, 87%" | "Mario, P2, 41%" |
| KOs and falls, "P3 Fox, 0" (Mario also has P3 there) | "Mario, P2, 0" |
| KOs and falls, "P2 Mario, 1" (Mario's row there is P1) | "Mario, P2, P1 Link, you, 0" |
| KOs and falls, "KOs" (a heading) | "Mario, P2, KOs" |
| KOs and falls, "SDs, 0" | "Mario, P2, 1" |
| Bonuses, row 1, "Switzerland, 12000" | "Mario, P2, Stiff Knees, 300" |
| Bonuses, row 5, where Mario has 3 bonuses | "Mario, P2, Ambler, 2500" (his last) |

A heading row repeats its word after a switch because the word is the row's whole content.

Unknown labels or values are spoken as the game's raw text or number and logged once, as on the other readers.

### Ready

- **Your Start readies you:** "Ready, waiting for P2." This names every human not yet ready ("waiting for P2 and P3"). Online it is "Ready, waiting for your opponent." With one human the scene leaves at once and character select speaks; nothing is said for the ready.
- **Your second Start un-readies you:** "Not ready."
- **Another human readies:** "P2 ready." This is queued after current speech, not interrupting. Online: "Your opponent is ready." Another human's un-ready, "P2 not ready." or "Your opponent is not ready.", is proposed in writing, like the point below.
- **CPUs** are said nothing of.
- **While you are ready**, the reading cursor keeps working, since the data stays. *This point was not put to the maintainer; it was proposed in writing for the play test to judge.*

### Quick chat

On results, quick chat gives up the D-pad as it does on character select. `pc_a11y_chat_buttons` (`hooks.cpp`) masks the D-pad when `screen_steers()` says so. On results, it masks it while the reader is active. This needs no new line in a base port file.

### Tests

- **A unit test of the reader** (`test_results_speech`, fake clock and bridge, snapshots built by hand). It covers:
  - the opening for each match type, teams, No Contest, ties, Bonus mode, "you" and no "you";
  - the keys on the first results of a run only;
  - the stats opening by your press and by another's;
  - page changes and the silent ends;
  - every row form on each page;
  - stepping and its edges, switching players with each "what differs" case and the edges, Z;
  - ready, un-ready, another human's ready, offline and online;
  - an unknown label.
- **The offline drive scripts.** `tools/a11y/results.drive` is extended, with scripts beside it as needed. They wait on the opening, page with A and B, step and switch with the D-pad, press Z, and ready.
  - Ways to reach results: the debug match `MELEE_DEBUG_VS=cpu` with `MELEE_DEBUG_VS_STOCKS=1` (Stock), `MELEE_DEBUG_VS=cpu` without stocks ended with L+R+A+Start (Time, No Contest), and `MELEE_DEBUG_VS=cpu4` (four CPUs, nobody "you").
  - A real timed match and a team match need the menus, which takes a two-minute match.
  - The 2026-10-27 exploration's scripts are in the session scratchpad and are not kept. Rebuild them from these notes.
- **An online run** through `tools/a11y/drive_net.py`, fork against fork and against the base port build (`--base-b`). Both instances reach results, page and read, and ready in turn: one hears "Ready, waiting for your opponent." and the other "Your opponent is ready.". It also covers un-ready, which offline drivers cannot reach: a second human offline has no controller and is readied at once.
- **A text dump on results** (`MELEE_A11Y_TEXT_DUMP`) confirms the SdRst strings for the match types, the stat labels, the units and the bonus names.

### Docs

- `docs/a11y/native-menus.md`: a "Results in detail" section with what this spec found (state, flow, content, text, online), the screen inventory's results line pointing to it, and the answers to the open questions.
- `CLAUDE.md`: one "Accessibility status" line.
- The `verify` skill: the new scripts, the online run and their expected lines.

## Open questions

For the implementer to find out:

1. **The decoded strings.** Which SdRst indexes hold the match types, stat labels, units and bonus names, and whether the "every capital after the first gets a space" rule reads every label right ("HitPercentage", "TopSpeed", "Attempted Grabs" already spaced). Also whether US and PAL indexing differ.
2. **The stat values' forms.** Damage with a %, distances and their unit, seconds as "m:ss" or a count. Some fields (`x40`–`x4C`, `x8C`, `x90`) have no agreed meaning; say each as the screen shows it.
3. **Reading controller 1's presses.**
   - Which buttons the reader sees, and where it reads them: the game's pad status for the local port, or the pad hook.
   - Online: whether the D-pad, masked from chat, still reaches the game's pad, and whether that matters. The game ignores the D-pad on results, but a D-pad press in state 2 opens the stats.
4. **Winner shown versus places on a tie.** Which player the banner names, and how ties look in `is_big_loser`.
5. **Name tags.** How page 0 gets a player's tag (`GetPersistentNameData`, Shift-JIS full width) and whether the fork already decodes tags anywhere.
6. **The hidden first row.** Whether every list really draws an entry above the clip window that the player never sees. The reading cursor must not include it.
7. **The first press.** Whether the first press can also page (a press of A that both opens the stats and pages). The code suggests it only opens.
8. **Leaving.** Whether the reader must drop a queued announcement when the scene leaves, and how a disconnect mid-results ends the scene.

## Issues

1. `issues/01-snapshot-and-opening.md`: the snapshot, the opening and the scene words.
2. `issues/02-reading-cursor.md`: the stats panels, the pages and the reading cursor. Blocked by 01.
3. `issues/03-ready-and-online.md`: ready speech, online wording and quick chat. Blocked by 02.
4. `issues/04-drive-scripts.md`: the drive scripts, the online run, the `verify` skill and the status line. Blocked by 03.
5. `issues/05-play-test.md`: the maintainer, by ear. Blocked by 04.
