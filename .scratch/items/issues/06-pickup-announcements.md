# 06 Pickup announcements

Status: ready-for-agent
Type: task
Blocked by: 03

Spec: `.scratch/items/spec.md` ("Pickup announcements", open question 1 as answered in issue 03). Read it first, then `.scratch/match-announcements/spec.md` ("Who is announced, in which voice", "Player labels") and `match_announcer.cpp`, whose labels and match start and end this reuses.

## What to build

- **Takes.** From the snapshot's per-slot hands and touch marks (issue 03), each tick from GO to GAME! or TIME!, every item a player took: picked up, caught after a throw, a Starman or Mushroom touched, and the same item picked up again after a drop or throw. A healing item used up on the tick it was picked up is still a take.
- **Who and which voice.** Yours in your voice; everyone else's in the opponents' voice, teammates' only with the Teammates switch on; with no "you", everyone's in the opponents' voice. Only players the match announcements would announce (a HUD panel).
- **The words.** The item's name, after the player label where `MatchAnnouncer` uses one, its labels fixed at GO: "Ray Gun", "P3, Ray Gun", "teammate P2, Food". Share the label with the match announcements rather than computing it again.
- **The subject.** A range of subjects of its own, one per player, so a pickup replaces only an older pickup about the same player and never a damage or stock line, nor they it (`speech.hpp`, the subjects).
- **The switch.** **Item pickups**, in Match announcement settings after Item appearances, on by default, under the Match announcements switch.

## Done when

- Unit tests with hand-written snapshots: a pickup by you and by a CPU, each in its voice; labels with one opponent and with three others; a teammate with the switch on and off; a catch; a touch item; a re-pickup after a throw; a healing item used up at once; nothing at or after GAME!; a pickup line not replacing a waiting damage line about the same player, nor the reverse; the switch off.
- A drive run in Training: an item spawned and picked up is said in your voice.
- The menu scripts read the new row, their step counts updated.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`; the `verify` skill's quick and menus levels.

## Comments
