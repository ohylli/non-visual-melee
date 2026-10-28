# Gameplay state: what a match knows and where to read it

A primer for the work of making matches accessible. It describes what the game knows during a match and how to read it: the players' damage and stocks, where the fighters are, how a stage's ground, platforms, walls and ledges are modelled, what knocks a fighter out, and the items. It ends with candidate hook points and open questions. It does not decide what any of it should sound like.

Values marked as measured come from a probe run on 2026-10-05: temporary logging added to a debug match on Battlefield and Final Destination, driven by `tools/a11y/drive.py`, then removed.

## Mental model

A match is a 2D game drawn in 3D. Every fighter, item and piece of ground lives on one flat plane: x runs left to right, y runs bottom to top, and z is always 0. The game advances in ticks of one sixtieth of a second, and every speed is in units per tick.

To the game, the stage is not a picture but a set of line segments: floors to stand on, ceilings, and walls. Some floors are thin platforms that can be jumped up through and dropped down through, and some floor ends are ledges a fighter can hang from. Around everything is a large invisible rectangle, the blast zone: a fighter whose position leaves it is knocked out and loses a stock (a life).

Each tick the game moves every fighter, tests it against the lines, and records the outcome in the fighter's own data: on the ground or in the air, which floor line it stands on, whether it touches a wall, whether it is balancing at an edge. After the tick the fork can read all of this, and can ask the stage its own questions ("is there a floor anywhere below this point?") through the game's collision library.

So making matches accessible is mostly reading: once a tick, look at each fighter, the stage and the items, notice what changed, and decide what is worth saying or sounding. The fork must never change any of it, because online play depends on both machines simulating exactly the same match.

## Engine terms in plain words

- **Tick** (or frame): one step of the simulation, 1/60 s. The game counts time in ticks, so a 30-tick timer is half a second.
- **World units**: the game's unit of distance. Final Destination's floor is 171 units wide and a fighter is roughly 10 to 20 units tall; [Sizes measured](#sizes-measured) has more.
- **GObj** (`HSD_GObj`): a generic game object with an update function the game calls each tick (see `native-menus.md`). Fighters, items and stage parts are all GObjs, each kind kept on its own linked list.
- **Player slot**: one of the match's six player positions (four for players, two spare), holding who plays, the character, the team and the score.
- **Fighter**: the character on the stage, a GObj whose data is `struct Fighter`. A slot usually has one fighter; Ice Climbers have two, and Zelda and Sheik keep their other form as a sleeping second fighter.
- **Motion state** (the decomp's `motion_id`; the Melee community says action state): what the fighter is doing this tick, as a number: standing, walking, jumping, falling, being hit, hanging on a ledge.
- **HUD** (heads-up display): what is drawn over the match: each player's percent, stock icons and the timer.
- **Knockback**: the push a hit gives; the higher the victim's percent, the farther it flies.
- **Hitstun**: the moment after a hit when the fighter cannot act. **Tumble** is the helpless fall after a strong hit (`DamageFall`).
- **Recovery**: getting back to the stage after being knocked off it, usually with the midair jump and then the up-special move (B with the stick up).
- **Teeter**: the wobble of a fighter stopped at the very end of a floor (`Ottotto` in the decomp, from the Japanese for "whoops").
- **Line**: one straight segment of the stage's collision geometry, with a kind (floor, ceiling, wall) and flags (platform, ledge).
- **Joint**: a group of lines tied to one part of the stage's 3D model, so the lines move when that part moves.
- **Raycast**: a test along a line segment for the first surface it crosses, such as "from here straight down, what floor do I hit first?".
- **Blast zone**: the rectangle outside which a fighter is knocked out. **Stock**: one life in a stock match.
- **Magnifying glass**: the bubble the game draws at the screen's edge for a fighter outside the camera's view.
- **Resimulation**: online, the re-running of recent ticks once a late input arrives (rollback, see `netplay.md`).

## Reading safely

The fork reads game state and never writes it, because rollback re-runs ticks and both machines must compute the same match (CLAUDE.md, "Online compatibility"). Gameplay reading brings traps of its own.

- **Getters are mostly safe.** Field reads and the small `ftLib_*`, `Player_Get*`, `itGet*` and `mpLine*` getters only read. `src/pc/slp.c`, the base port's replay recorder, reads almost every value in this primer once a tick and notes the helpers it had to avoid, so it is the first place to look.
- **Functions that act are not.** Anything that draws a random number (`HSD_Rand`, `HSD_Randi`) changes the simulation: `Stage_80224FDC` (a random item spawn point) and `it_8026C75C` (a random item pick) are examples. Functions that compute a score into a cache, such as `gm_GetMatchEndPlayerScore`, write.
- **Stage raycasts belong between ticks.** They set and clear temporary flags in the collision data; see [Is there ground below?](#is-there-ground-below-over-a-gap).
- **Line ids need a guard.** A line id of -1 means "none". The collision library checks ids only in debug builds, so on the PC passing -1 reads outside the array. Check every id before passing it on. The game's own presence test, `mpLib_80054ED8(id)`, answers false for -1 and for a disabled or hidden line, but loops forever on an id past the end of the array.
- **Bitfields need the game's layout.** `Fighter`, `Item` and the match rules hold bitfields. A fork C file that reads them must be compiled the way `game_access.c` is (`-mno-ms-bitfields`, set in `a11y.cmake`), or every field after the first bitfield is read from the wrong place.
- **Some fields are written by drawing, not by the simulation.** The offscreen flag (`x221F_b0`, `ftLib_IsOffscreen`) is set while the frame is drawn, so it can lag the simulation by a tick.
- **Online, a tick can be a guess.** While `pc_net_resim()` is true the game is re-running old ticks and the fork stays silent. A tick played for the first time may also be predicted: the peer's input for it has not arrived yet, the game guessed, and a later rollback can rewrite it. A hit announced from a predicted tick may turn out never to have happened. slp.c holds each tick's data until the tick is confirmed; speech will have to choose between reacting at once and waiting a few ticks for certainty.

## When to read: after the fighters move

The match's tick runs in `gm_RunSimTick` (`src/melee/gm/gmscene.c`), in this order:

1. The scene's per-tick function, `gm_Scene_Vs_OnFrame` for a VS match (`src/melee/gm/gmvs.c`): timer, pause and match-end logic.
2. `HSD_GObj_RunProcs()`: every GObj's update, which moves the fighters and settles collisions, damage, deaths, items and stage parts.

So code at the start of `gm_Scene_Vs_OnFrame` sees the previous tick's finished state, and code right after `HSD_GObj_RunProcs()` sees this tick's. The base port's replay recorder already sits around that call (`pc_slp_tick_begin` and `pc_slp_tick_end`). `pc_slp_tick_end` receives a bitmask with one bit per GObj list, set for each list the game held still that tick. When the fighters' bit (`1 << HSD_GOBJ_PLINK_FIGHTER`) is set, the fighters did not move: the game is paused, frozen on the GAME! that ends a match, or skipping a tick in a slow-motion mode.

The match scene (`GS_VS`) is shared by VS, Classic, Target Test and more (`native-menus.md`, "Scenes"); Training has its own per-tick function, `gm_Scene_Training_OnFrame`.

### The tick hook

The fork's hook is there: `pc_a11y_match_tick(held_mask)`, right after `pc_slp_tick_end`, with the same mask. It runs in every scene that ticks, menus included, and again for each tick a rollback re-runs, which `hooks.cpp` keeps silent. `a11y_game_match_state` (`game_access.c`) reads the match from it into a snapshot, and the match cues work from that (`cues.md`). In the Training scene it also hands the Training menu to its reader, with the cues on or off (`native-menus.md`, "The Training menu in detail"), and on results it hands the results scene to its reader (`native-menus.md`, "Results in detail"). In every scene it reads the HUD for the match announcements ([The HUD](#the-hud)), which the scene hook resets, each match being a scene of its own. Whether a match is on is the snapshot's to say: the local player's slot must have a fighter, and that fighter must be on the live fighter list, which is empty outside a match. The second test keeps a slot's pointer from being followed should it outlive its fighter; the player slots are static and survive every scene.

### Pause, freezes and slow motion

The fighters' bit in the held mask does not tell a pause from slow motion, and misses one pause altogether. The game keeps a direct record: a pause word (`gm_80479D58.unk_10.x0`, read with `gm_801A4624()`; `gm_GetDbPauseFlag(bit)` tests one bit), from which `gm_RunSimTick` derives the mask's paused part. Its bits:

| Bit | Set by | Holds the fighters |
|---|---|---|
| 0 | the debug pause (debug builds only) | no, but stops the tick |
| 1 | a VS pause (`gm_DoPauseChecksAndRoutine`) | yes |
| 2 | a Training pause: the Training menu is open | no: the fighters' own update stands still on it (`fighter.c`) |
| 3 | the pause camera | yes |
| 4 | the GAME! or TIME! freeze | yes |
| 5 | a single-player mode's score tally | yes |
| 6 | an event's scripted freeze | yes |

Slow motion (Training's speeds, Slo-Mo Melee, events with a game speed below 1) works differently: the frame timer (`lb_80019880`) runs the simulation on fewer ticks, and a tick it skips holds every GObj list, the fighters' included, with the pause word clear. Measured in Training at half speed, every other tick was held.

So "the match is held still on purpose" is the pause word being nonzero (`A11yMatchState.paused`), and a held mask with a clear pause word is a skipped tick of slow motion. Measured: a VS pause set 0x2 and held the fighters, the GAME! freeze after a last stock 0x10 and held them, and Training's menu set 0x4 with no list held.

### Before GO

The fighters tick from the match's first frame, but their input stays off (`fp->input_disabled`) until GO (`fn_8016B7F8` calls `ftLib_EnableAllInput`). In the debug VS match the fighters stood in `Entry` (322) for about 1.4 seconds before input came on; Training has no Ready and GO, and its fighters' input is on from the start.

## Players and fighters

### Player slots

`StaticPlayer player_slots[6]` (`src/melee/pl/player.c`) holds one row per slot, read through `Player_*` getters:

- `Player_GetPlayerSlotType(slot)`: `Gm_PKind_Human` 0, `Gm_PKind_Cpu` 1, `Gm_PKind_Demo` 2 (the computer players of the title screen's demo), `Gm_PKind_NA` 3 for an empty slot, `Gm_PKind_Boss` 4 (Master Hand and the like).
- `Player_GetPlayerCharacter(slot)`: the `CharacterKind`, the numbering character select uses and the fork already names in `css_names.cpp`.
- `Player_GetTeam`, `Player_GetCpuLevel`, `Player_GetPadPort` (the controller port).
- `Player_GetStocks`, `Player_GetDamage`, `Player_GetFalls`, `Player_GetKOsByPlayerIndex(slot, victim)`, `Player_GetSelfDestructs`.
- `Player_GetEntity(slot)`: the slot's active fighter GObj. `Player_GetEntityAtIndex(slot, 1)` is the second one: Nana for Ice Climbers, the inactive form for Zelda and Sheik.

### Fighters

`GET_FIGHTER(gobj)` (`src/melee/ft/inlines.h`) gives the `struct Fighter` (`src/melee/ft/types.h`). Its identity fields:

- `kind`: a `FighterKind`, a different numbering from `CharacterKind` (Mario is 0, Link 6).
- `player_idx`: the slot.
- `is_sub_fighter`: set on Nana.
- `is_sleeping`: set on a fighter out of play, such as the inactive form of Zelda and Sheik, or a fighter between death and respawn.

There are two ways to walk the fighters: the fighter list, `HSD_GObjPLinkHead[HSD_GOBJ_PLINK_FIGHTER]` and each `->next`, which also meets the sleeping forms and the dead, so filter on `is_sleeping`; or by slot, as slp.c does, taking each slot that is not empty, its `Player_GetEntity`, and Nana.

Zelda and Sheik swap which fighter is active when transforming, and `Player_GetEntity` follows the swap. Ice Climbers (Popo, the lead climber, and Nana, the partner) share one slot, one stock count and one HUD percent (Popo's), though each climber has a percent of its own.

### Which fighter is the player's

Offline, the player on controller 1 is the slot whose pad port is 0. Online, `pc_net_local_player()` (`src/pc/net.c`) returns the local controller port (0 for the host, 1 for the guest), and a VS match puts slot n on port n, so the local fighter is the human slot whose `Player_GetPadPort` equals it.

## Damage percent

- **On the fighter**: `fp->dmg.x1830_percent`, a float, clamped to 999. `ftLib_GetPercent(gobj)` returns it as a whole number.
- **On the slot**: `Player_GetDamage(slot)`, the active fighter's percent as a whole number. The HUD (`ifStatus_802F5B48`, `src/melee/if/ifstatus.c`) reads this every tick without delay, so it is what a sighted player sees.
- **Where it rises**: every hit goes through `Fighter_TakeDamage_8006CC7C(fp, amount)` (`src/melee/ft/fighter.c`), which adds the damage, clamps it, copies it to the slot and records statistics. It is the natural hook for "took damage, and how much", though in a few states it is called without adding anything. A few other things call it directly too, such as damage over time while off screen. It is not told who hit; the last attacker's slot is `fp->dmg.x18c4_source_ply`, 6 meaning none.
- **Where it falls**: healing items lower it a point per tick (`fp->dmg.x18F0` counts the remaining healing down in `fighter.c`), Ness absorbing energy lowers it, and a knockout resets it. These bypass the damage function, so a complete "percent changed" signal comes from comparing the percent with last tick's.
- **Stamina mode** counts hit points down instead: `Player_GetMoreFlagsBit2(slot)` is set in stamina mode, and `Player_GetRemainingHP(slot)`, the stamina minus the damage, is what the HUD shows then.

Measured: a hit raised Link's percent by 3.0 on the tick it landed, and a thrown Proximity Mine took it from 5.0 to 33.0 in one tick.

## Stocks, KOs and respawns

### The knockout

`ftCo_800D3158` (`src/melee/ft/ft_0D31.c`) runs for every fighter each tick right after it moves. It compares the fighter's position with the four blast zone edges and starts a dead state when one is crossed.

- The left, right and bottom edges always knock out.
- The top edge knocks out only a fighter that is on the ground, frozen in ice (or Jigglypuff flying off after its shield breaks), or flying upward from a hit faster than a threshold from the game's data (`p_ftCommonData->x4F0`). A fighter jumping or floating up on its own passes through safely.
- A top knockout is usually a star KO (the fighter flies into the background) or, at random, a screen KO (it hits the camera).

The dead states are motion states 0 to 10 (`DeadDown`, `DeadLeft`, `DeadRight`, `DeadUp`, then the star and screen variants), all caught by `ftLib_IsDead`.

### The stock loss

`ftCo_800D34E0` takes the stock, credits the knockout to the last attacker or counts a self-destruct, and zeroes the slot's percent. It takes a stock only in modes that count them: stock matches (`gm_8016B094()`) and modes such as Multi-Man Melee (`gm_8016B0E8()`). It takes one only for the slot's active fighter, so Nana and the inactive Zelda or Sheik cost nothing. It runs on the knockout tick for side and bottom knockouts, and at the end of the animation for star and screen KOs. It is the natural hook for "lost a stock, knocked out by whom". The last attacker is the same `fp->dmg.x18c4_source_ply`.

Stocks left: `Player_GetStocks(slot)`.

### Respawn

When the dead animation ends, the fighter sleeps (state 11, `is_sleeping` set), then appears on the respawn platform above the stage (`Rebirth`, 12), which descends and waits (`RebirthWait`, 13) until the player moves or time runs out. `ftLib_IsRebirth` covers both. The match's opening entrance is `Entry` to `EntryEnd` (322 to 324, `ftLib_IsEntry`).

Measured on Battlefield: walking off the left edge, Link fell and entered `DeadDown` at y = -110.4 (the bottom edge is at -108.8), and his stock count dropped from 2 to 1 on that same tick. Player 1's respawn platform appeared at (12.8, 135.1) and waited 246 ticks, about four seconds; each player has a respawn point of its own.

A player who loses the last stock sleeps like any other dead fighter, except after the knockout that ends the match: the GAME! freeze stops the fighters before the dead animation finishes. Measured: after Link's last stock in a two-player match, his fighter stayed in the list with `is_sleeping` clear. So test the stock count, not the fighter, to know who is still in.

## Match rules, timer and phases

- **Rules**: `gm_GetStartMeleeRules()` returns the match's `StartMeleeRules` (`src/melee/mn/types.h`). `match_kind` is `MatchKind_Time` 0, `MatchKind_Stock` 1, `MatchKind_Coin` 2 or `MatchKind_Bonus` 3; stamina is a per-player flag, not a kind. Other fields: `is_teams`, `time_limit` in seconds, `item_freq`, `stkind` (the stage), `friendly_fire`. `gm_8016B094()` asks "stock match?" and `gm_8016B168()` "teams?" (`src/melee/gm/gmvs.c`).
- **Time match score**: compute it from `Player_GetKOsByPlayerIndex`, `Player_GetFalls` and `Player_GetSelfDestructs`; the game's own score functions write a cache.
- **Timer**: `gmVs_GetSceneState()` returns the match's `VsSceneState` (`src/melee/gm/types.h`). `timer_seconds` is whole seconds left when counting down and seconds elapsed when counting up, `unk_2C` the ticks within the second, `frame_count` the ticks of play since the GO graphic ended, not counting pauses. The game's announcer already counts down the last five seconds.
- **Phases**: `VsSceneState.unk_0` is 0 while the match runs (paused included), 1 during the GAME! or TIME! freeze (the moment the match is decided, when everything stops), 2 while a single-player mode shows its score tally, and 3 when finished. It stays 0 when a match is ended from the pause menu with L+R+A+Start. `match_result` says why the match ended (`MatchOutcome`: timeout, elimination, no contest).
- **Ready and GO**: the match opens with the "Ready" graphic. When it ends, `fn_8016B7F8` enables all input: the GO moment. When GO ends, `fn_8016B784` sets `hud_enabled`, after which the timer runs and pausing works.
- **Pause**: `gm_GetDbPauseFlag(1)` is true while a player has paused; `VsSceneState.pauser` says who.

The phases and pause flag above are the VS scene's. Training keeps its own: it never sets `unk_0`, and pauses through `gm_GetDbPauseFlag(2)`. Training does show GO, and `hud_enabled` comes on as it ends, as in VS. Each match scene's start (`fn_8016DCC0`) zeroes `VsSceneState`, so `hud_enabled` is clear until that match's GO.

`hud_enabled` also goes off mid-match: the pause camera clears it while it is up (`gmcamera.c`), and single-player modes clear it for scripted scenes, such as Master Hand's entrance in Classic (`gm_17C0.c`) and some events (`gmevent.c`).

## The HUD

The HUD at the bottom of the screen has one panel per player: the damage as a whole number, the stock icons in a stock match, and the character's emblem. Over each fighter the game draws a label: "1P" to "4P" by controller port, "CP" for every computer player with no number, or a human's name tag instead. The match announcements (`.scratch/match-announcements/spec.md`) read all of it from one snapshot, `a11y_game_match_hud` (`game_access.c`), filled each tick from the tick hook.

### Which players have a panel

- **Where**: `ifStatus_HudInfo.players[slot]` (`IfDamageState`, `src/melee/if/ifstatus.c`), one per slot. A panel shows while its `HUD_parent_entity` is not null; the fork compares the pointer and never follows it.
- **Made** at the match's start by `ifStatus_802F665C(rules.x0_3)`, which calls `ifStatus_802F6508` for every slot: a panel for each slot below `StartMeleeRules.x0_3`, the panel count, whose slot is not empty, when `rules.x2_6` allows panels (Camera Mode clears it). The count is 4 in VS, 6 in Multi-Man and All-Star, and set per stage or event elsewhere.
- **Kept through a KO and the respawn**: the respawn (`Player_80032070` calls `ifStatus_802F6E1C`) runs `ifStatus_802F6508` again, which reuses the slot's panel. Between the KO and the respawn the digits fly apart (`explode_animation`) but the panel stays.
- **Taken down** by `ifStatus_802F6788`: when `rules.x2_7` is set, 200 ticks after an eliminated player's digits fly apart (`ifStatus_802F5B48`); when a single-player mode removes a fighter (`fn_8016EF98`, `gm_80167320`); and with the whole HUD at the scene's end (`ifStatus_802F6804`).
- **Hidden without going**: `hide_all_digits` hides every panel's digits at once (the pause, Corneria's and Venom's scripted moments). The fork counts a panel as showing through it.
- **Multi-Man's wireframes have panels.** Measured on 2026-10-28 in 10-Man Melee: three wireframes on the stage, each with a panel showing its damage beside yours, the panels in slot order with no number. A single-player mode that spawns a fighter mid-match (`gm_16A2.c`) makes no panel then, but its respawn does, while the slot is below the panel count.

### Damage on the HUD

What the HUD shows is `Player_GetDamage(slot)` (see [Damage percent](#damage-percent)), copied every tick by `ifStatus_802F5B48` and clamped to 0 to 999, in stamina mode `Player_GetRemainingHP` instead (`Player_GetMoreFlagsBit2`). Measured in the debug match and with four CPUs:

- A CPU's attack on you and yours on it each changed the figure on the tick of the hit, and a combo's hits a few ticks apart.
- On Poké Floats a fighter off screen took a point at a time, a second or so apart: off-screen damage goes through `Fighter_TakeDamage_8006CC7C`, so the HUD counts it as a hit.
- Training's menu sets a CPU's damage when it closes: the figure the menu's reader already said appears on the HUD as the menu closes.

### A KO

A KO is the fighter's knockout states, motion states 0 to 11 (`DeadDown` to `Sleep`), as the cues' `out` reads them, in stock and timed matches alike: the snapshot's `out`. The damage goes to 0 inside them (`ftCo_800D34E0`), on the KO's tick for a side or bottom KO and at the end of the flight for a star or screen KO, so the damage is never 0 outside them because of the KO. The falls count (`Player_GetFalls`) would also mark it, but the state is there on every tick of the KO, so a tick skipped for a rollback re-run never hides one. Measured: Mario smashed off Final Destination at 25% in the debug match, and nothing was said of his 0% after the respawn. After the KO that ends a match the GAME! freeze stops the fighter mid-flight ([Respawn](#respawn)), so a star KO's damage can still show when the match ends.

### Stocks on the HUD

`Player_GetStocks(slot)`; the panel shows up to five icons, then one icon and a count ([Stocks, KOs and respawns](#stocks-kos-and-respawns)).

- **Which matches show them**: a stock match (`gm_8016B094()`) and the modes that count lives (`rules.x4_2`, `gm_8016B0E8()`: Classic, Adventure, Multi-Man), as `ifStatus_802F6508` gives each panel its stock icons; a timed match shows none. `ftCo_800D34E0` takes a stock at a KO on the same test. The snapshot's `stocks` says it.
- **When the count changes**: on the KO's tick for a side or bottom KO, at the end of the flight for a star or screen KO, and at a team's stock steal, below. Measured in the debug match with three stocks: Mario smashed off the right edge and Link walking off the left each went from 3 to 2 on the tick the cues stopped for the fall.
- **Multi-Man's wireframes** each start with one stock and leave the slot at their KO, so the next one in it starts afresh: in 10-Man Melee six wireframes knocked out gave no stock line.

### A team's stock steal

In a team match a player out of stocks can take one from a teammate, and the count rises.

- **Where**: `fn_8016B918` (`gmvs.c`), run each tick of a VS match with teams (`rules.is_vs` and `is_teams`) from the VS scene's update, before the outcome is checked. A player with 0 stocks whose fighter is still there presses Start (D-pad up with X in a debug ROM) and takes a stock from the first teammate by slot with more than one, if the stock icons' animation has room (`ifStock_802F7EFC`): `Player_LoseStock` on the giver, `Player_SetStocks` one higher on the taker, then `fn_8016719C(taker, 0)`, the respawn.
- **The taker's panel is there on the steal's tick.** In VS a panel is never taken down at an elimination: `rules.x2_7`, which frees it 200 ticks after the digits fly apart, is set only by single-player modes (`gm_17C0.c`), and the steal is VS's alone. The respawn also calls `ifStatus_802F6508`, which makes the panel again or reuses it, on that same tick, before the tick hook. The eliminated fighter stays on the fighter list, sleeping, so the slot stays in the snapshot throughout.
- **Measured** in `tools/a11y/match_stock_steal.drive`: Pichu, out of stocks, pressed Start, and as the press reached the game his count went from 0 to 1 and the CPU teammate's from 3 to 2, both said then.

### Labels and name tags

- **The label over the fighter** is the port's ("2P") for a human, "CP" for every computer player, or a human's name tag. In VS and online slot n plays on port n, so the third panel is P3's or the third CPU's; in Multi-Man the panels carry no number.
- **The name tag**: `Player_GetNametagSlotID(slot)`, 120 for none, then `GetPersistentNameData(index)->namedata`, up to eight bytes of Shift-JIS without a terminator, as results reads it; `decode_shift_jis` (`game_text.cpp`) decodes it. Online each copy looks the index up on its own memory card, so the peer's tag is not reliable.
- **Stable through the match**: the slot's tag index is set at the match's start from character select's choice (`gmvs.c`, from `PlayerInitData.nametag`) and to none when a single-player mode spawns a fighter mid-match (`gm_16A2.c`, `gm_181A.c`); a respawn and a team's stock steal leave it.
- **Human or computer**: `Player_GetPlayerSlotType`; a demo player and a boss count as computer players. **Team**: `Player_GetTeam(slot)` and `gm_8016B168()` for a team match, as for the cues.

## Position and motion

All on `Fighter`, with `ftLib_*` getters (`src/melee/ft/ftlib.c`) for most.

- **Position**: `cur_pos` (x, y; z is 0), at the fighter's feet. On the ground its y equals the floor's.
- **Speed**: `self_vel` (own movement in the air), `x8c_kb_vel` (knockback), `gr_vel` (speed along the ground), all in units per tick. Measured: Link's dash covered 15.6 units in 12 ticks, about 1.3 units a tick, so crossing Final Destination takes over two seconds.
- **Facing**: `facing_dir`, +1 right and -1 left.
- **Ground or air**: `ground_or_air`, `GA_Ground` 0 or `GA_Air` 1.
- **Jumps**: `x1968_jumpsUsed` against `co_attrs.max_jumps` (2 for most characters, more for Kirby and Jigglypuff). Leaving the ground without jumping counts as the first jump, so a fighter that walks off an edge has only its midair jump left (measured); landing resets the count.
- **Being hit**: `ftLib_IsInHitstun(gobj)`; the freeze on impact (hitlag) is `dmg.x195c_hitlag_frames`.
- **Shield**: `shield_health`.
- **Motion state**: `motion_id`. The shared states are the enum `ftCommon_MotionState` (`src/melee/ft/kinds/ftCommon/forward.h`), where each state's number is its position in the enum. States from 341 up are the character's own (its special moves), listed in each character's directory under `src/melee/ft/kinds/`.

A selection of shared states. The probe saw at least one state of every row except `Sleep` and the ledge rows (252 and up).

| Number | State | Meaning |
|---|---|---|
| 0 to 10 | `DeadDown` and the other dead states | knocked out |
| 11 | `Sleep` | out of play between death and respawn |
| 12 and 13 | `Rebirth`, `RebirthWait` | on the respawn platform |
| 14 | `Wait` | standing |
| 15 to 17 | `WalkSlow` to `WalkFast` | walking |
| 20, 21, 23 | `Dash`, `Run`, `RunBrake` | running |
| 24 | `KneeBend` | about to jump |
| 25 to 28 | `JumpF` to `JumpAerialB` | jumping, from the ground or midair |
| 29 | `Fall` | falling |
| 38 | `DamageFall` | tumble |
| 39 | `Squat` | crouching |
| 42 | `Landing` | landing |
| 75 to 91 | `DamageHi1` to `DamageFlyRoll` | being hit |
| 244 | `Pass` | dropping through a platform |
| 245 and 246 | `Ottotto`, `OttottoWait` | teetering at an edge |
| 252 and 253 | `CliffCatch`, `CliffWait` | grabbing, then hanging on a ledge |
| 254 and up | `CliffClimbSlow` and on | getting up from a ledge |
| 322 to 324 | `Entry` to `EntryEnd` | the match-start entrance |

## How a stage is modelled

### Lines

A stage's collision comes from its archive on the disc and is loaded with the stage (`mpLibLoad` in `src/melee/mp/mplib.c`). Points and lines are copied into runtime arrays: `mpGetGroundCollVtx()` for points (`pos` is the current world position) and `mpGetGroundCollLine()` for lines. A line id is an index into the line array, from 0 to `mpLib_8004D164()->line_count` - 1 (23 lines on Battlefield). Listing every ledge, for example, means walking that range for enabled floors with `LINE_FLAG_LEDGE`.

- **Kind**: `mpLineGetKind(id)` is `CollLine_Floor` 1, `CollLine_Ceiling` 2, `CollLine_RightWall` 4 or `CollLine_LeftWall` 8.
- **Wall naming**: a wall's kind names the way its surface faces, not the side the fighter feels it on. A `LeftWall` line faces left, so it stops a fighter moving right: the wall is on the fighter's right. A `RightWall` is on the fighter's left.
- **Flags**: `mpLineGetFlags(id)`. `LINE_FLAG_PLATFORM` (0x100) marks a platform that can be jumped up through and dropped down through. `LINE_FLAG_LEDGE` (0x200) marks a floor whose end can be hung from. The low byte is the surface material (grass, ice, metal…), which sets friction and footstep sounds.
- **Presence**: the line array's own `flags` hold `LINE_FLAG_ENABLED` and `LINE_FLAG_HIDDEN`; a disabled or hidden line is absent for the moment.
- **Ends**: `mpLineGetV0Pos(id, &pos)` and `mpLineGetV1Pos`. A floor runs from left (v0) to right (v1).
- **Neighbours**: each line knows the line joined to each of its ends (`mpLineGetPrev`, `mpLineGetNext`), so a floor made of several lines can be walked as one surface. The joined line is often a wall or ceiling, where the floor turns down into the stage's side; the result is -1 only when nothing joins.

Battlefield shows the shape of a typical stage. Its main floor is three floor lines joined end to end: a short ledge-flagged piece at the left end, a long unflagged middle, and a ledge-flagged piece at the right end. Only the end pieces carry the ledge flag, because those are what a hanging fighter grabs. Three platform lines float above. The rest of its 23 lines are the ceilings and walls of the stage's underside. Final Destination is the same main floor, wider, with no platforms. The coordinates are in [Sizes measured](#sizes-measured).

### Joints and moving geometry

Every tick `Ground_UpdateMapColl` (`src/melee/gr/ground.c`) moves each joint's points to where its model part now is, so a moving platform carries its lines with it, and `mpGetSpeed` gives a platform's speed under a point. Stages also switch whole joints on and off (Pokémon Stadium's transformations) and single lines on and off, and a moving line can even change kind as it tilts. So line positions, presence and kinds must be read fresh each tick; only the ids stay put.

### Islands

`src/melee/mp/mpisland.c` groups connected floors into islands, one per separate surface (the main stage, each platform). `mpIsland_8005AB54(line)` finds a line's island and `mpIsland_8005ACE8` its current ends. The CPU players use islands to reason about the stage, which makes them a candidate for "which piece of ground is this".

## Stage identity, blast zones, camera and points

- **Which stage**: `stage_info.grkind` (`GrKind`, `src/melee/gr/forward.h`, with English names in its comments) or `Stage_80225194()` for the `StKind` the match rules use. `St_Kind_Last` is Final Destination, not an end marker. Kinds past it are single-player variants (Target Test, Adventure, events) that share a `GrKind` with a VS stage, so a name table keyed by `GrKind` covers every mode. The game has no stage names as text, so the fork will need such a string table, as it has for characters.
- **Blast zones**: `Stage_GetBlastZoneLeftOffset()`, `…Right…`, `…Top…` and `…Bottom…` (`src/melee/gr/stage.c`), in world units. Mute City moves them mid-match, and the scrolling Adventure stages move the camera offset the getters add in, so read them each tick rather than once.
- **The camera's box**: `Stage_GetCamBoundsLeftOffset()` and its siblings: the area the camera may show, well inside the blast zones. Between the camera box and the blast zone, a sighted player sees a fighter only in the magnifying glass.
- **The floors' extent**: `mpLib_80458868[1]` (`mplib.c`) is the bounding box of all enabled floors, platforms included, updated whenever geometry moves: a ready answer to "how big is the stage". The main stage has no marker of its own; it would come from the islands.
- **The main floor**: no marker says which floor is the main one. The lowest enabled floor line across the middle of the blast zones, a solid floor before a platform, followed to its surface's outer ends with `mpFloorGetLeft` and `mpFloorGetRight`, finds it on the tournament stages; the floors' extent alone does not on Yoshi's Story. See [The tournament stages' frames](#the-tournament-stages-frames).
- **Named points**: the stage file holds points read by `Ground_801C2D24(index, &pos)` (`ground.c`): the players' start points (0 to 3), respawn points (4 to 7), item spawn points (0x7F to 0x93), the camera's centre (0x94), and the corners of the camera box (0x95, 0x96) and blast zones (0x97, 0x98). Point 0x7F always reports found, falling back to a stand-in when the stage lacks it, and points 8 and 9 are computed midpoints, not stored ones.

## Questions the stage can answer

### Is there ground below? (over a gap)

`mpCheckFloor(ax, ay, bx, by, 0, &hit, &line, &flags, &normal, -1, -1, -1, NULL, NULL)` (`src/melee/mp/mplib.c`) is a raycast from A to B against every floor, returning the floor nearest to A. It only finds floors crossed from above, so cast from just above the point straight down: from (x, y + 1) to (x, y - 1000). It sees platforms too (`flags` has `LINE_FLAG_PLATFORM`). The CPU players use the same call to judge their recoveries (`ftCo_800A0FB0`, `ftCo_0A01.c`).

Between ticks the call leaves no trace. To skip far-away joints it marks them with a temporary flag and clears the marks before returning, but only if no marks were set when it began. Inside the game's own collision step, marks for that step's fighter are already set, so the call would reuse them and could skip a floor and answer "nothing below" wrongly. So call it only from a hook outside the fighters' update. The same holds for `mpCheckLeftWall`, `mpCheckRightWall` and `mpCheckMultiple`.

Measured: standing anywhere on Battlefield's main floor, the cast found a main floor line at y = 0. One tick after Link walked off the left edge, at x = -69.6, it found nothing.

**Cast from the collision box, not the feet.** In the air a fighter's collision box (the ECB, environment collision box: the diamond the game tests against the lines) rides above its feet, and the game lands the fighter when the box's bottom point meets a floor. So for a few ticks before landing, the feet (`cur_pos`) are below the floor's surface while the box is still above it. Measured on 2026-10-14: Link falling onto Final Destination's floor had his feet at y = -1.5 to -4.9 while still in the air, the box's bottom 5.9 units above them (`coll_data.ecb.bottom.y`, an offset from `cur_pos`), and a cast from just above his feet found nothing below. Cast from just above `cur_pos.y + coll_data.ecb.bottom.y` instead. On the ground the box's bottom is at the feet.

A floor line's ends reach a unit further for the cast than the line itself where another line joins them (`mpLib_8004ED5C`), so past Final Destination's left edge at -85.6 the cast still meets the floor down to about x = -86.6.

**The fork calls the game's own cast, from the tick hook** (ADR-0006). This settles the read-only question for floor tests. What `mpCheckFloor` writes: its output parameters, and the joints' too-far mark (`CollJoint_TooFar`) with the flag `didCheckBounding` (`mplib.c`). When the flag is clear on entry, it marks every joint outside the cast's box, and every disabled or hidden joint, then clears every joint's mark and the flag before returning. Every user of the marks in the game pairs them the same way: the fighters' collision step sets them for each step and clears them after it (`mpColl_80043754`'s loop, the ledge searches), and a stage load clears them. So between ticks the flag is clear and no joint is marked, and the cast leaves both as it found them: nothing the simulation or a rollback snapshot reads changes. Everything else it calls reads: the lines, points and joints (`mpLib_8004ED5C`, `mpLineGetPrev`, `mpLineGetNext`) and the intersection helpers. Walking the line arrays in fork code would have meant copying the joined-end extension and the joint ranges, for no gain.

"Nothing below" does not by itself mean death: the fighter may still jump or recover back to the stage. It means that falling straight down, the fighter will cross the bottom blast zone.

### How far to the edge? (near an edge)

For a fighter on the ground, `coll_data.floor.index` is the floor line under it. `mpFloorGetLeft(id, &pos)` and `mpFloorGetRight(id, &pos)` follow the joined lines to the surface's outer ends, and the distance to an edge is `cur_pos.x` minus that end's x. This is the test the teeter state itself makes. `floor.index` keeps its last value after the fighter leaves the ground (measured), so read it only while `ground_or_air` is `GA_Ground`.

Measured: standing anywhere on Battlefield's main floor, the ends came back as -68.4 and 68.4; on the right side platform, 20 and 57.6.

The game also gives its own verdict. A fighter that reaches the end of a floor it faces, without the stick pushed hard toward it, stops and teeters (motion 245, then 246), and its `coll_data.env_flags` gets `Collide_Edge` (0x800000) for that tick. A fighter that keeps the stick pushed walks or runs straight off instead. Measured on Final Destination: one dash toward the left edge slid Link to x = -85.6, the very end, and into the teeter. On another run the same dash stopped him at -84.7, short of the edge, with no teeter.

Whether the edge is deadly is the [gap question](#is-there-ground-below-over-a-gap) asked just past it. The ledge warning asks it that way from the tick hook (ADR-0006), for the floor below you on the ground and in the air: the cast starts just above the end's height, a few units outwards (the edge offset, a cue setting), and goes down to the bottom blast zone. The offset must clear the unit a floor reaches past its end where another line joins it, and stay under the 10.8 units from a Battlefield side platform's end to the main floor's end, or the platforms' ends would count as deadly. Before the cast, the line joined at the end decides one case alone: a wall going up from the end is a step, not an edge, and a cast past it would start inside the stage. On a ledge the joined line is the stage's side, going down.

Measured with that test (offset 5): the main floors' ends are deadly on Final Destination (±85.6), Battlefield (±68.4), Dreamland (±77.3), Fountain of Dreams (±63.3), Pokémon Stadium (±87.8), Yoshi's Story (±56.0), Brinstar (-52.8 and 56.3) and Jungle Japes (-44.9 and 49.9, and both ends of each side floor over the river). The platforms' ends over the main floor are safe; the outer ends of Yoshi's Story's and Brinstar's side platforms, and of Kongo Jungle's left one, reach past the main floor and are deadly. Princess Peach's Castle's roof ends at its central tower (x = ±20.7) and Hyrule Temple's floors at several steps meet a rising wall and are not deadly; Mushroom Kingdom's pits make deadly ends in the middle of the stage.

Whether it is a ledge is the ledge flag on the line at that end: follow `mpLineGetPrev` (left) or `mpLineGetNext` (right) from the floor line while the result is another floor, the way `mpFloorGetLeft` and `mpFloorGetRight` do, and test the last line's `LINE_FLAG_LEDGE`.

### Can I drop through this platform?

The fighter is on a platform when it is on the ground and its floor line has `LINE_FLAG_PLATFORM` (`mpColl_IsOnPlatform` asks this through `mpLineGetFlags`, without guarding against a missing floor). Tilting the stick down quickly drops through (`ftCo_80099F1C`, `ftCo_Pass.c`): the fighter enters `Pass` (244) and the game ignores that platform until it has fallen past: it names the platform in `coll_data.floor_skip`, which its collision step passes to `mpCheckFloor` as the line to skip, and so does the fork's own cast. A quick flick down while already crouching drops through too, after a short delay; holding a crouch does not. From below, any platform can be jumped up through.

Measured on Battlefield: a down press on the right platform put Link in `Pass`, and he landed on the main floor 20 ticks later.

### Is a wall in the way?

Each tick's collision records walls touched. `coll_data.env_flags` has `Collide_LeftWallMask` for a `LeftWall` line (a wall on the fighter's right) and `Collide_RightWallMask` for a wall on its left (`src/melee/mp/forward.h`), and `coll_data.left_facing_wall.index` and `right_facing_wall.index` name the lines. `mpCheckLeftWall` and `mpCheckRightWall` cast sideways the way `mpCheckFloor` casts down, to find a wall before reaching it.

Walls a fighter walks into are uncommon on the tournament stages: on Battlefield and Final Destination every wall line belongs to the stage's underside, met only when recovering from below. The probe never saw a wall touched.

### Can I grab the ledge?

A fighter falling near the end of a ledge-flagged floor catches it (`CliffCatch`, then hangs in `CliffWait`), provided:

- it is falling, not rising;
- its current state allows grabbing (most falls and recoveries do, most attacks do not);
- the ledge is in front of it: the game looks for the left end of a floor when the fighter faces right and the right end when it faces left, so a fighter recovering toward the stage faces the ledge it can grab;
- the stick is not held down;
- no one else holds that ledge, and the fighter did not just let go of one.

The search runs in the fighter's collision step (`mpColl_80044164` for a fighter facing right, `mpColl_800443C4` facing left, `src/melee/mp/mpcoll.c`), in a box sized from the character's ledge-grab reach. Those functions write into the collision data they are handed, so the fork leaves them alone. The ledge's position is the end of the ledge-flagged floor, from `mpFloorGetLeft` or `mpFloorGetRight`, and hanging shows as `motion_id` 252 or 253. The probe did not catch a grab; this section comes from the code.

### Would falling here kill me?

The game already warns of a deadly fall. When a fighter falls past the midpoint between the camera's bottom edge and the bottom blast zone (`Stage_CalcUnkCamY`), it plays sound 96, which the decomp calls the sound you always hear near the bottom blast zone (`ft_PlaySFX` in `fighter.c`): at y = -78 on Battlefield and -110 on Final Destination. It plays once per fall, and is armed again only after the fighter climbs back above a line three quarters of the way up from the blast zone to the camera's bottom edge. The side blast zones kill without such a warning, and the top one only as [described above](#the-knockout).

## Stages that move

The stages' own code is in `src/melee/gr/`, one file per stage. A stage is made of parts, each a GObj with a map id; the union `u` in a part's data holds its state. `Ground_GetMapGObj(i)` returns the part with id i, but parts can share an id, so slp.c, which already reads some of these each tick, walks the stage's GObj list (`HSD_GOBJ_PLINK_GROUND`) and matches each part's `map_id` instead. A short guide; only the first two were read closely:

| Stage | What changes | What to read |
|---|---|---|
| Pokémon Stadium (`grpstadium.c`) | transforms between neutral, fire, grass, water and rock layouts | part 2's `u.stadium`, below |
| Brinstar (`grzebes.c`) | lava rises and falls, and burns | `stage_info.x724`, the lava's surface height (-10000 on other stages); a point below it is in the lava |
| Fountain of Dreams (`grizumi.c`) | side platforms rise and sink | the two parts with id 4, one per side, each `u.izumi3.xD0` the height; slp.c tells the sides apart by their x |
| Dream Land (`groldpupupu.c`) | Whispy's wind pushes fighters | part 7's `u.oldpupupu.xDC` |
| Rainbow Cruise, Icicle Mountain (`grrcruise.c`, `gricemt.c`) | the stage scrolls past | the scroll state in their files |
| Mute City, Big Blue (`grmutecity.c`, `grbigblue.c`) | platforms and a race track with cars | Mute City also rewrites its camera box and blast zones mid-match |
| Others | cars, Arwings, Klaptraps, Shy Guys and more | many are items; see [Items](#items) |

Pokémon Stadium's state: in part 2's `u.stadium`, `xDE` is the form, which changes as a transformation begins: 5 neutral, 3 fire, 4 grass, 9 water, 6 rock. `xDC` is the phase: 0 waiting, 1 loading the next layout from the disc, 2 and 3 the big screen previewing the coming form, 4 the old layout sinking, 5 the new one rising, 6 a final tick of tidying up. A return to neutral skips the loading.

The game's CPU players already reason about stage danger: `ftCo_800A229C` (`ftCo_0A01.c`, private to that file, so to read rather than call) lists danger areas for Rainbow Cruise, Big Blue, Icicle Mountain, Great Bay, Peach's Castle and Brinstar: prior art for warnings. The base port's frozen Pokémon Stadium option (`pc_is_frozen_stadium_enabled()`) turns the transformations off.

## Items

### What an item is

Items are GObjs on their own list, `HSD_GObjPLinkHead[HSD_GOBJ_PLINK_ITEM]`; check `gobj->classifier == HSD_GOBJ_CLASS_ITEM` while walking it. The data is `struct Item` (`src/melee/it/types.h`, `GET_ITEM(gobj)`), with:

- `kind`, `pos`, `x40_vel`, `ground_or_air`;
- `owner`: the fighter that last held or threw it, or NULL;
- `x1C`: a serial number counted up from 0 each match, which identifies an item for as long as it lives.

The list holds far more than the item switch's items: projectiles (Fox's laser, Link's arrows, Mario's fireball), Pokémon and their attacks, and many stage hazards. `kind` (`ItemKind`, `src/melee/it/forward.h`, with English names in its comments) sorts them into sections, in this order:

1. Common items, the ones the item switch controls: 0 to 0x22, from `It_Kind_Capsule` to `It_Kind_M_Ball` (the Poké Ball). A test is `kind < It_Kind_L_Gun_Ray`.
2. Item effects, such as the Ray Gun's beam and the Hammer's head.
3. Monsters such as Goombas and ReDeads.
4. Character items: projectiles, turnips, bombs.
5. Pokémon, from `It_PKind_Random` (0xA0) and `It_PKind_Start` up to `It_PKind_Terminate`.
6. Pokémon attacks.
7. More monsters, including stage creatures: Shy Guys, Klaptraps, Koopa shells, Like-Likes.
8. Stage items: Tingle, Whispy's apples, Flat Zone's falling tools, Birdo and her egg, the Arwing and Great Fox lasers.

The sections are marked with `@name` comments in `forward.h`. Kind 0xA0 is also reused for the invisible hit areas of stage hazards such as cars and lava.

`Item_IsGrabbable(gobj)` says whether the item is in a state anyone could pick up: lying loose, not held. Reach and weight are checked separately ([Within reach](#pickup-throw-use)). It also covers pickable character items such as Peach's turnips. There is no item name text in a match, so the fork names items itself: `item_names.cpp` holds the fork's item names keyed by `ItemKind` (`item_kinds.h`), so far the items the menus name (Item Switch and the Training menu), and match items would extend that table. (Training's item menu has names for 30 common items in `SdTrain.dat`, loaded only in Training.)

### Spawning

Every item is created by `Item_8026862C` (`src/melee/it/item.c`), through `Item_80268B18` for one in the air and `Item_80268B5C` for one on the ground.

- **Item switch spawns** come from a countdown in `fn_8026C88C` (`src/melee/it/itspawn.c`), which picks a kind and a spawn point and creates the item in the air with a sparkle; it then falls to the ground.
- **Containers** (capsules, crates, barrels, eggs) drop their contents through `it_8026F3D4` (`itdrop.c`), with a small hop.
- A spawn count, `it_804D6D0C`, goes up for these and for other things that appear on stage (a Party Ball's contents, Chansey's eggs, Whispy's apples); projectiles do not count.

Measured on Battlefield with items at Very High: a Party Ball appeared at (56, 55.9) above the right platform and settled at (56, 34.0); a Proximity Mine appeared at (-24, 71.8) and landed on the left platform. An item's `pos` is not its bottom: on a platform at y = 27.2, items rested at y = 29.2 to 34.

### Pickup, throw, use

- **Held**: `fp->item_gobj` on the fighter is the item in hand, NULL for none. Comparing it with last tick's caught the CPU Mario picking up the Proximity Mine and letting go when he threw it.
- **The pickup itself**: `ftpickupitem_800948A8(fighter, item)` (`src/melee/ft/kinds/ftCommon/ftpickupitem.c`) is the fighter's side and `Item_8026AB54` the item's. Healing items, the Metal Box and the Cloaking Device sit in `item_gobj` during the pickup animation and are then used up, so they leave the hand without being thrown. The Hammer and the Warp Star put the fighter into states of their own, and the Bunny Hood is worn.
- **Within reach**: `ftpickupitem_800942A0(fighter_gobj, flags)` finds, without changing anything, the nearest item the fighter could pick up now, searching in front of the fighter, so the answer depends on which way it faces; `flags` bit 0 asks for light items and bit 1 for heavy ones (crates, barrels), and the game passes 3. A candidate for "you can pick something up here".
- **Gone**: every item ends in `Item_8026A8EC`. Common items left lying loose count `xD44_lifeTimer` down (about 1400 ticks, 23 seconds, measured), blink near the end and vanish.
- **Ammunition**: `xD4C` on the item, for the Ray Gun, Super Scope, Star Rod, Lip's Stick and Fire Flower (the Hammer uses it for whether its head is still on); the Hammer's time is on the fighter.

### Item rules

`StartMeleeRules.item_freq` is -1 when items are off and 0 to 4 for Very Low to Very High; `Item_80266F3C()` asks "items on?". Stage items and character items exist even with items off. Ranked online play forces items off.

## Sizes measured

From the probe, in world units.

### Battlefield

- Main floor: x from -68.4 to 68.4, at y = 0. The ledge-flagged end pieces are its outer 8.4 units at each end.
- Platforms: x from -57.6 to -20 and from 20 to 57.6, at y = 27.2; x from -18.8 to 18.8, at y = 54.4.
- Floors' extent (`mpLib_80458868[1]`): x from -68.4 to 68.4, y from 0 to 54.4.
- Player 1's respawn platform: appears at (12.8, 135.1).
- Camera box: x from -160 to 160, y from -47.2 to 136.
- Blast zones: x from -224 to 224, y from -108.8 to 200.

### Final Destination

- Main floor: x from -85.6 to 85.6, at y = 0. The ledge-flagged end pieces are its outer 10.6 units at each end.
- Platforms: none.
- Player 1's respawn platform: appears at (16, 112.9).
- Camera box: x from -170 to 170, y from -80 to 114.
- Blast zones: x from -246 to 246, y from -140 to 188.

### The tournament stages' frames

Measured on 2026-10-13 with temporary logging in the tick hook, a debug match of about 40 seconds on each (Final Destination about 15), in world units. "Main floor" is the main floor found as above.

| Stage | Main floor x, at y | Floors' extent, x and y | Camera box top | Blast zones, x and bottom |
|---|---|---|---|---|
| Final Destination | ±85.6, 0 | ±85.6, 0 to 0 | 114 | ±246, -140 |
| Battlefield | ±68.4, 0 | ±68.4, 0 to 54.4 | 136 | ±224, -108.8 |
| Dream Land | ±77.3, 0 | ±77.3, 0 to 51.4 | 190 | ±255, -123 |
| Fountain of Dreams | ±63.3, 0 | ±63.3, -1.3 to 42.8 | 112.5 | ±198.8, -146.2 |
| Pokémon Stadium (neutral) | ±87.8, 0 | ±87.8, 0 to 25 | 120 | ±230, -111 |
| Yoshi's Story | ±56, 0 | from as far as -103 to 59.5, -33.2 to 42 | 118.3 | -175.7 and 173.6, -91 |

- **The extent is enough on five of the six.** On Battlefield, Final Destination, Dream Land and Pokémon Stadium its sides are the main floor's ends and its bottom the main floor. On Fountain of Dreams a side platform sinking into the stage takes the bottom to -1.3 for a while.
- **Not on Yoshi's Story.** Randall, the cloud circling the stage, is an enabled platform line: as it passes under the stage it drags the extent's bottom to -33.2 or -13.6, and beside the stage the extent's side out to about -103. Its blast zones are a little off centre, so the middle the snapshot looks across is x = -1. Yoshi's Story's main floor ends in slopes down to y = -3.5 at x = ±56, which `mpFloorGetLeft` and `mpFloorGetRight` follow. Randall also crosses the stage's middle beneath it, which is why a solid floor wins over a platform there.
- **Not checked**: Pokémon Stadium's other forms (no transformation came within the run) and every stage outside the six.

## Candidate hook points

By what each would signal. Every one except the first runs inside the simulation, so it runs again during resimulation and has to stay silent then.

| Signal | Where | Notes |
|---|---|---|
| A tick has finished | after `HSD_GObj_RunProcs()` in `gm_RunSimTick` (`gmscene.c`), beside `pc_slp_tick_end` | Built: `pc_a11y_match_tick`, see [The tick hook](#the-tick-hook). One hook that polls fighters, stage and items, the polling idiom of ADR-0003, and stays silent during resimulation. |
| Took damage | `Fighter_TakeDamage_8006CC7C` (`fighter.c`) | Has the fighter and the amount. Healing bypasses it. |
| Lost a stock | `ftCo_800D34E0` (`ft_0D31.c`) | Has the fighter and the attacker. Delayed for star and screen KOs. |
| Stage loaded | end of `fn_8016E730` (`gmvs.c`) | The match setup VS and Training share. Blast zones, camera box, points and collision are ready. |
| GO | `fn_8016B7F8` (`gmvs.c`) | Input is enabled here. |
| Item spawned | after `Item_80268B18` in `fn_8026C88C` (`itspawn.c`), or in `Item_8026862C` with filtering | |
| Picked up | `ftpickupitem_800948A8` (`ftpickupitem.c`) | |

Everything in the table can also be had by polling once a tick and comparing with the tick before, which keeps the fork's footprint to one line. A poll runs once, after the tick, so the `pc_net_resim()` gate covers it, and it compares whole states rather than piecing them together from events. Keying items on their serial `x1C` keeps a poll from announcing the same item twice. Neither approach escapes predicted ticks; see [Reading safely](#reading-safely) and [Open questions](#open-questions).

## Open questions

- **Predicted ticks online.** Whether an announcement waits for its tick to be confirmed, at the cost of a few ticks' delay, or speaks at once and occasionally reports a hit that rollback undoes.
- **Ledge grabs and held items** were not observed in the probe: where a held item's `pos` sits is unchecked, and the holder's position is a safe stand-in.
- **Most stages' state fields** beyond Pokémon Stadium and Brinstar are unnamed in the decomp.
