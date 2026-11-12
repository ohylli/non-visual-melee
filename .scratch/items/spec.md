# Items: appearances, the item's position cue and pickups

Status: agreed 2026-11-12 (design conversation with the maintainer)

The maintainer agreed:

- Items are the item switch's items, containers included, whatever brings them into the match; character pulls, Pokémon, projectiles and stage hazards are left out for now, to be revisited after play tests.
- An item appearing is said on the screen reader, queued, by name, and placed by a one-shot appearance cue where it appears.
- The item's position cue follows the nearest loose item at a steady pace, and pulses faster while it is within reach by the game's own test. Built so that tracking several items later is a setting, not a rewrite.
- Every player's pickups are said, yours in your voice, the others' in the opponents' voice, with the player label rule the damage lines use.
- The item cues have their own Items relative to you switch, beside the opponent's.
- The cues' sounds are recorded files the maintainer supplies, free to use, prepared by a fork script.
- A pulse longer than its pace is cut with a fade rather than refused.

The wording and every value are a first proposal for the play test to judge.

## Goal

A blind player in a match with items on hears each item come in, by name and by where it lands in the stereo field, can find the nearest one by ear and knows when pressing the button would take it, and hears who took what. Each part has its own switch, and the item cues can be heard from where you are or on the stage, as the player prefers.

Builds on the match cues (`.scratch/match-cues/spec.md`), the voices (`.scratch/voices/spec.md`) and the match announcements (`.scratch/match-announcements/spec.md`). Vocabulary is `CONTEXT.md`: item, appearing, loose item, within reach, appearance cue, position cue (the item's position cue), relative to you, pickup announcement, match announcement, player label, cue, voice. What the game knows about items is in `docs/a11y/gameplay-state.md`, "Items"; the cue engine and match cues are in `docs/a11y/cues.md`.

## Scope

In:

- **Items** in the fork's sense (`CONTEXT.md`): the 35 kinds the item switch controls, `It_Kind_Capsule` to `It_Kind_M_Ball`, containers among them, when they come in from the item switch, out of a broken container, or from Training's item menu. VS, Special Melee, Training, single-player modes with items, and online play.
- **The appearance announcement and the appearance cue.**
- **The item's position cue**, one item tracked.
- **Pickup announcements.**
- **The settings rows**, and the cue engine changes the cues need.
- **The sound preparation script** and the sounds' place in the package.

Out, each a later issue:

- **Character pulls**: Peach's turnips and her rarer pulls (a Bob-omb, a Beam Sword, Mr. Saturn), Link's and Young Link's bombs, and any item a fighter makes, even of a common kind.
- **Pokémon** out of a Poké Ball, monsters, stage hazards and projectiles: threats, a different feature.
- **Tracking several items at once**: the design leaves room for it (below, "Ready for several items"), but how several items sound together is that feature's question.
- **Disappearances**: an item timing out, used up, exploding or falling off the stage is not said.
- **Throws, drops and ammunition**: what you do with an item once held.

## What the game gives us

Found by reading the decomp and the fork's code on 2026-11-12. Address-style names are the decomp's.

### The item list

- **Every item** is a GObj on `HSD_GObjPLinkHead[HSD_GOBJ_PLINK_ITEM]`, data `struct Item` (`src/melee/it/types.h`): `kind`, `pos`, `x40_vel`, `ground_or_air`, `owner`, and `x1C`, a serial counted up from 0 each match that identifies the item while it lives. Container contents are new items with new serials (`Item_8026862C`, `item.c`).
- **Common kinds** are `kind < It_Kind_L_Gun_Ray` (`src/melee/it/forward.h`).
- **Made by a fighter**: `flags.x1` is clear for an item a fighter spawned, which covers character pulls and Pokémon (whose owner is the ball's thrower). That is the test that leaves Peach's Bob-omb out though its kind is common.
- **Origin otherwise**: `owner` is NULL at spawn for item switch, container and Training spawns. Training's spawn alone sets `x18 == 1` (`Item_80268B9C`). Item switch spawns start with zero velocity and a sparkle (`fn_8026C88C`, `itspawn.c`); container contents with a random hop (`it_8026F3D4`, `itdrop.c`). The fork needs no origin beyond "not made by a fighter".
- **Training's item menu** spawns through the same creation path, on the ground 10 units above player 1's model, and does not raise the spawn count `it_804D6D0C`.

### Held, thrown, loose

- **Grabbable**: `Item_IsGrabbable` is the item's `x15`, set while it lies loose and again after a throw, so a thrown item can be caught.
- **A pickup** (catches included, `it_802742F4`) sets `owner` to the taker, sets `x13` (held), clears `x15` and ORs the taker's bit into `flags.xF`. A throw or drop keeps `owner`, clears `x13`, sets `x14` (thrown) and counts `xD54_throwNum` up.
- **Landing**: `xD50_landNum` counts landings and is reset on pickup and throw (`itgroundcoll.c`). Bob-ombs, Mr. Saturns and shells keep moving on the ground.

### Taking an item

- **Into the hand**: every common item but three ends up in `fp->item_gobj`. Healing items, the Metal Box and the Cloaking Device are used up as the pickup animation ends; the Hammer and the Warp Star stay there while the fighter is in their own states; the Bunny Hood moves to `fp->x197C`; the Screw Attack stays. A healing item picked up while holding something goes to `fp->x1978`, not `item_gobj` (`ftpickupitem.c`).
- **By touch**: the Starman and the Super and Poison Mushrooms never enter a hand. Touching one (`ftColl_80077C60`, `ftcoll.c`) sets the item's `xC34_damageDealt`, applies the effect and destroys the item.
- **One tally for both**: `pl_8003E17C` raises a per-player, per-kind count (`x674[kind]`, read with `pl_8003E2CC(slot, kind)`, `plbonuslib.c`) on pickup and on touch alike, but only for an item no fighter made and only on that player's first hold of it, so it misses a re-pickup.

### Within reach

`ftpickupitem_800942A0(fighter_gobj, flags)` (`ftpickupitem.c`) returns the nearest item the fighter could take now, or NULL, and writes nothing and draws no random number: its callees only read. It tests a box in front of the fighter (`fp->x294_itPickup`, a ground box, an air box and a heavy box, scaled from the character's data) grown by the item's `xBD4_grabRange`; in the air it uses the air box; while the fighter holds an item only healing items and coins qualify; `flags` bit 0 asks for light items and bit 1 heavy ones, and the game passes 3. It does not check whether the fighter can act; its callers do. The game's own air catch calls it with Z, for light items only (`ftCo_Attack100.c`).

### The fork's side

- **The cue engine** (`cue_engine.hpp`) has looped and pulsed voices and the speech voices' once mode. The once mode pans but ignores pitch and the cue levels, so it is not for cues. A pulsed voice whose sound outlasts its fastest interval at its lowest pitch is refused at creation. `kMaxVoices` is 16, with 12 in use (five match voices, their five preview twins, the two speech voices); `kMaxCues` is 16, with 4 in use. Voices are added and never removed.
- **Files**: miniaudio is compiled with decoding (WAV, FLAC, MP3; not OGG), and `ma_decoder` can hand out mono 48 kHz floats, which `read_sound` (`cue_sounds.hpp`) already takes as a data source. The base port copies all of `resources/` beside `melee.exe` and into the Windows package, so `resources/a11y/` ships with no base port file touched; the game finds it from `SDL_GetBasePath()`.
- **The opponent's position cue** (`position_cue.cpp`) keeps its relative mapping in free functions on plain distances (`relative_pan`, `relative_pitch`) and its crossing state per opponent slot; the item's cue can reuse the functions and key the state on the item's serial.
- **Voices**: `Speech::announce_voice(voice, subject, text)`; a line replaces the one about the same subject in that voice. Subjects are ints, today a player's slot. Player labels are fixed at GO in `MatchAnnouncer`.
- **Item names** (`item_names.cpp`): 31 kinds, everything but the four containers.

## Decisions

### How the fork attaches

- **No new hooks.** Everything is read from `pc_a11y_match_tick`, gated by `game_hook_may_speak()`, from the match snapshot `a11y_game_match_state` (`game_access.c`), which gains the items. Within reach is the game's own `ftpickupitem_800942A0`, called from the snapshot between ticks, as the floor tests call `mpCheckFloor` (ADR-0006); its pickup boxes are disc-endian data and are read only in C.
- **It reads, never writes.**

### Which items

- **An item** is on the item list, of a common kind, and not made by a fighter. Everything else on the list is ignored by every part of this feature.
- **A container** is an item like any other: it appears, it is tracked, and picking it up is announced. When it breaks, its contents appear as new items.
- **Names**: the existing table, plus **Capsule**, **Crate**, **Barrel** and **Egg**, the game's English trophy and manual names, since no menu shows them as text.

### The snapshot

- **A short list of items**, nearest to you first by straight-line distance from your position to the item's, each with its serial, kind, position, whether it is loose, whether it is within reach, and who holds it (a slot, or none). The list's length is a constant, enough to find every new item in a burst and to track several later.
- **Loose**: grabbable, not held, and not flying after a throw. One thrown is loose again once it lands.
- **New items** are found by serial: an item whose serial was not on last tick's list has appeared. A list longer than the snapshot holds still notices every new serial (the fork compares against the serials it has seen this match, not only those it kept).
- **Taking** is found by comparing each slot's hands (`item_gobj`, `x1978`, `x197C`) and the touch items with last tick's, covering a re-pickup and a catch (open question 1 settles the method).

### The appearance announcement

- **On the screen reader, queued**, so it never cuts off the time left or anything else the screen reader is saying.
- **The item's name alone**: "Bob-omb".
- **Several in one tick** (a Party Ball, a broken Capsule) are one announcement listing them in the list's order, a repeated name said as often as it appears: "Food, Food, Maxim Tomato", joined by the helpers in `sentences.hpp`.
- **Paused**: Training's item menu spawns while the menu is open; the announcement is queued behind the menu's own speech.

### The appearance cue

- **One one-shot per item**, played on the tick it appears, at the place it appears, in the air where it comes in.
- **Placed** relative to you or on the stage, by the Items relative to you switch (below, "The item cues' frame").
- **Several at once** each play, on voices of their own; a burst larger than the voices kept for it takes the oldest playing.
- **Silent** while you are out with the switch on (relative to you has no place to be relative to); the announcement is still said.
- **The sound**: the maintainer's `item_appearance.wav`.

### The item's position cue

- **Follows the nearest loose item**, chosen afresh each tick. When another item becomes the nearest, the voice moves there without restarting, so the pulse keeps its rhythm. A newly appeared item that is nearest is followed at once, even while its appearance cue plays.
- **Pace**: a steady interval, and a faster one while the item is within reach. Two speeds only: distance is the pan's and the pitch's job, and closeness to the opponent is your position cue's.
- **Within reach** is the game's test with both light and heavy items asked for. An item behind you is out of reach until you turn; the pan says it is behind. Touch items are never within reach: walking into them takes them.
- **While you hold an item** it keeps tracking: where items are stays useful.
- **Silent** outside a match, while paused, before GO, with no loose item, and while you are out with the switch on.
- **The sound**: the maintainer's `item_position.wav`, a different sound from the appearance cue.

### The item cues' frame

- **Items relative to you**, a switch of their own, on by default, for both item cues.
- **Relative to you**, as the opponent's position cue: the pan from the horizontal distance with a pan floor and a full side distance, the side kept until the item is a crossing margin past you, and the pitch from the vertical distance. The item cues have their own copies of those distances, tuning values starting at the opponent cue's.
- **On the stage**, as your position cue: the same pan frame, under the shared Position cues follow the camera switch, and a pitch range of the item's position cue's own.

### Ready for several items

The item's position cue keeps its driver state (the side, a voice) per item serial, and the snapshot's list is ordered by distance, so tracking the nearest N items later needs only N voices and a setting. N is 1 now, with no row.

### Pickup announcements

- **Everyone's**: yours in your voice, everyone else's in the opponents' voice, teammates' only with the Teammates switch on.
- **The words**: the item's name, "Ray Gun", after the player label where the match announcements use one: "P3, Ray Gun", "CPU 2, Food". The label rule and labels are `MatchAnnouncer`'s, fixed at GO.
- **What counts**: picking up, catching a thrown item, touching a Starman or a Mushroom, and picking the same item up again after a drop or throw.
- **Their own subject**: a pickup replaces only an older pickup line about the same player, never a damage or stock line, and those never replace it. Subjects get a range of their own per player for pickups.
- **From GO to GAME! or TIME!**, as the other match announcements.

### The settings rows

- **Match announcement settings**, after Time left: **Item appearances** and **Item pickups**, switches, on by default.
- **Cue settings**: **Appearance cue** and **Item's position cue**, each registered with `add_cue` (a switch, a volume and Preview, its boost a tuning value), and **Items relative to you**, a switch, on by default, after the opponent's relative switch.
- **Tuning values**: the item's position cue's steady and within-reach intervals and pitch range, and the relative distances of the item cues.

### The previews

- **Appearance cue**: three one-shots a short pause apart: to your left, to your right, then above you.
- **Item's position cue**: a few pulses at the steady pace a little to one side, then a few at the within-reach pace.
- Both follow the Items relative to you switch, and play whatever the switches say, as every preview does.

### The cue engine

- **A one-shot voice**: a voice that plays its sound once from the start when triggered, with pan, pitch and gain targets, through its cue's level like any cue voice, so the Cues switch, the cue volume and the cue's own switch and volume reach it.
- **Cut with a fade**: a pulsed voice whose sound is longer than its interval is no longer refused. When the next pulse is due while one still plays, the old one fades out over a few milliseconds as the new one starts from the top, so the slow pace plays the whole sound and a fast one its start. The existing cues already fit and sound as before.
- **More voices and cues**: `kMaxVoices` and `kMaxCues` raised to hold the item cues, their one-shot pool and their preview twins, with room for several tracked items.

### The sounds

- **The maintainer's files**, free to use, kept outside git as originals.
- **`tools/a11y/prepare_sounds.py`**, run with `uv run`, its dependencies (numpy, soundfile) declared in the script: takes an original by path and writes `resources/a11y/item_appearance.wav` or `resources/a11y/item_position.wav`, mono, 48 kHz. It trims silence at both ends below a threshold, fades the end over a few milliseconds so the trim never clicks, and normalizes by peak to just under full scale; the balance between cues is set by ear through each cue's volume and boost. The engine adds the lead-in before the attack, so the files start at their first sound.
- **Its report**: each file's length, and the fastest interval at which a pulse plays whole at the cue's lowest pitch, so the maintainer knows which paces cut it.
- **The prepared files are committed**, so a cue dump compares with exactly what plays, and the game does no processing at start.
- **A missing or unreadable file** leaves its cue silent, logged once; the rest of the feature works.

### The speech log

Every appearance and pickup announcement is an `[a11y]` line, the pickups naming their voice; the item cues log their voices' starts and stops like every cue, the one-shots each play.

### Online

- **Gated** like all speech and cues: silent while `pc_net_resim()` is true.
- **Said at once**, as the stocks are: a pickup or appearance on a tick played on a guess of the peer's input may be undone by a rollback. The open question in `gameplay-state.md` covers both.
- **The within-reach call** reads only, so a base port peer is unaffected. Ranked online forces items off.

### Tests

- **Unit tests** with hand-written snapshots: new items by serial and a burst read as one announcement; names, containers included; items made by a fighter ignored; loose against held and thrown; the nearest item chosen and changed without restarting; the two paces; the frame switch both ways and the crossing margin; silence while paused, before GO, with no loose item and while you are out; pickups by hand, catch, touch and re-pickup, labels, teammates and subjects; the previews.
- **Engine tests**: the one-shot voice with pan, pitch and cue level; overlapping one-shots; a long pulse cut with a fade at a fast interval and played whole at a slow one, rendered into a buffer.
- **Drive runs**: Training's item menu brings an item in beside the player, the cue dump and trace check the appearance cue and the item's position cue, and a pickup is said; a VS match with items on (open question 4) for the item switch's spawns and a CPU's pickups. The expected lines go into the `verify` skill.
- **An online run** through `tools/a11y/drive_net.py`, fork against fork and against the base port, with items on, the session staying in sync.
- **The play test**, by ear.

### Docs

- `docs/a11y/gameplay-state.md`, "Items": the findings above that it lacks (taking by touch, the tally, the within-reach box, made by a fighter) and the answers to the open questions.
- `docs/a11y/cues.md`: the one-shot voice, the cut with a fade, sounds from files, and sections on the appearance cue and the item's position cue.
- `ACCESSIBILITY.md`: what a player hears of items and the new rows.
- `CLAUDE.md`: one "Accessibility status" line.
- The `verify` skill: the drive runs, the online run and their expected lines.

## Open questions

For the implementer to find out:

1. **One way to see a take.** Whether comparing each slot's `item_gobj`, `x1978` and `x197C` with last tick's, plus the touch items disappearing with `xC34_damageDealt` set, catches every take, and which slot touched a Starman or Mushroom (whether the item's victim field names the fighter); or whether `flags.xF` or the tally serves better. A healing item may be picked up and used up on the same tick, and must still be said.
2. **Thrown and landed.** Which field says a thrown item has landed and is loose again: `x14`, `xD50_landNum`, `ground_or_air`, or a combination, checked on an item that bounces.
3. **The appearing tick.** Whether physics has already moved an item on the tick it first shows on the list, so the appearance cue's place is where it came in.
4. **Items in a drive run.** How a debug VS match (`MELEE_DEBUG_VS`) can have items on at a high frequency, or else which drive script turns the item switch on through the rules screens.
5. **Training's spawn while the menu is open.** Whether the item is created while the menu holds the match or as it closes, and so when its appearance cue plays.
6. **The sounds' credits.** Whether `resources/a11y/` should hold a short note naming each file's source.

## Issues

1. `issues/01-one-shots-and-cut-pulses.md`: the cue engine's one-shot voices, pulses cut with a fade, more voices and cues.
2. `issues/02-sounds-from-files.md`: `prepare_sounds.py`, loading from `resources/a11y/`, placeholder files until the maintainer's arrive.
3. `issues/03-item-snapshot.md`: the items in the match snapshot, the four container names, and open questions 1, 2, 3 and 5.
4. `issues/04-item-appearances.md`: the announcement with its Item appearances row, the appearance cue, and the Items relative to you switch. Blocked by 01, 02 and 03.
5. `issues/05-item-position-cue.md`: the item's position cue with within reach. Blocked by 04, which brings the frame switch.
6. `issues/06-pickup-announcements.md`: pickup announcements with the Item pickups row. Blocked by 03.
7. `issues/07-drive-scripts-and-docs.md`: a VS match with items (open question 4), the online run, the `verify` skill, the docs and the status line. Blocked by 04, 05 and 06.
8. `issues/08-play-test.md`: the maintainer, by ear. Blocked by 07.
