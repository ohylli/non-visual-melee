# 04 Stage select and results announcements

Status: resolved (2026-10-03)
Type: task

Spec: `.scratch/lan-play/spec.md`, section "Stage select and results announcements". Both are scene announcements in `src/pc/a11y/scene_speech.cpp`.

- Stage select, online only (`pc_net_active()` or the fork's equivalent): "Stage select. No speech yet. Press Start for a random stage, then wait for your opponent." Offline unchanged.
- Results, offline and online: say how to continue. First find out with drive runs what actually leaves results, offline and online; `docs/netplay-verification.md` says it exits on Start only in some cases and otherwise on an internal counter, and its harness could not leave it. Word the announcement to match ("Press Start to continue." if Start works; otherwise say what does).
- Update unit tests and expected lines; update the `verify` skill if any expected line it lists changes.

## Comments

### 2026-10-03: implemented

- `scene_speech.cpp` gained a table of online words, looked up first when `pc_net_active()` is true (the hook passes it); stage select and results have entries there, every other scene says its offline words.
- What leaves results, from the decomp (`fn_801791E4`, `fn_80177920`, `fn_80178050` in `gmresultplayer.c`) and drive runs, offline (debug match, Link walking off Battlefield) and online (`css_net.drive` into a two-minute time match, then presses on each side):
  - No press is taken while the winner is revealed, about 3 seconds, until just after the announcer names the winner; a Start pressed then is lost. Only a cancelled match ("No contest") lets Start skip the reveal.
  - Then any button from any human player shows every player's stats. No sound.
  - Once the stats have slid in (50 frames), Start marks that player ready with the menu's forward sound, and a second Start takes it back with the back sound. The scene ends once every human player is ready; offline with one human that is one Start, online both players.
  - So `netplay-verification.md`'s harness pressed Start during the reveal, or once only.
- The words count sounds, not presses, because of the online case: when the opponent's press opened the stats, a player's first Start already makes them ready, and a second takes it back. The online run showed it: a pressed Start twice, and b's first Start ended the scene. Offline: "Results. No speech yet. Once the announcer finishes, press Start until you hear the confirm sound." Online adds ", then wait for your opponent."
- Stage select online: "Stage select. No speech yet. Press Start for a random stage, then wait for your opponent.", seen in both instances' logs; `css_net.drive` now waits for that wording.
- `tools/a11y/results.drive` checks the offline results line and that two Starts after the announcer leave; it needs the debug match switches, named in its header and in the `verify` skill. The online results line was seen once in a one-off two-instance run (a time match takes two minutes, too long for every online run) and is covered by the unit tests.
- For the play test (issue 05): whether "the confirm sound" is understood, and whether results needs its own speech (readiness, the stats) sooner than other screens.
