# 05 Stage select online

Status: resolved (2026-10-09)
Type: task
Blocked by: 04

Spec: `.scratch/stage-select/spec.md`, "Online" in both parts. Primer: `docs/a11y/netplay.md`.

- The per-frame hook, or a new one, gives the local and remote picks and the resolved cell.
- After the player's own pick: "Waiting for your opponent."
- The result:
  - "Corneria, your pick." or "Corneria, opponent's pick.";
  - "Corneria." when both picked it;
  - a winning Random pick: "Random: Corneria, your pick." (or opponent's).
- The opponent's pick is never said before the result.
- Find out (spec open question 1) whether the local controller 1 drives `pc_net_local_player()`'s stage cursor, so steering works for player 2 too.

## Done when

- Unit tests cover waiting and every result form.
- `tools/a11y/drive_net.py` runs: both instances steer to a stage and pick, and each logs waiting and the result from its own side. Fork against fork and against the base port build (`--base-b`), at input delays 0 and 10 and over the slow link; recordings agree. Update `css_net.drive` or add a script, and the `verify` skill.
- The online scene words in `scene_speech.cpp` no longer carry stage select's "then wait for your opponent".

## Comments

### 2026-10-09, implementation (agent)

Implemented; hearing it is issue 07.

- **A new hook for the result.** `pc_a11y_sss_resolved(cell, local_pick, remote_pick)` in `mnStageSel_Scene_OnFrame`, one line after the base port's "sss: resolved" log, before the scene ends. The frame hook cannot give the result: it runs at the top of the frame function, and the frame that reads the other player's pick (`netStageSel_Poll`) resolves the coin flip, rolls an online Random and leaves the scene on that same frame. So the frame hook's own `local_pick` and `remote_pick` (issue 01's) are not what the speech uses; they stay in the snapshot unused. The new hook is reached offline too, where the speech ignores it; online it is reached once a visit (the base port logs it once), on a different frame on each machine. Menus run in lockstep, without rollback, so `pc_net_resim()` never silences it there.
- **Waiting.** Online, the player's own pick (the pick state leaving "choosing") says "Waiting for your opponent.", whatever was picked: a stage, the random tile or Start before any tile, since online Random is rolled only at the resolution. The other player's pick reaches a copy only after its own preview (the base port polls once the pick state is 2, about 1.5 seconds after the pick), so waiting is said even when the other player picked first, and the result follows that long after.
- **"Picked" online too.** A pick during a glide that lands on another stage than the one named now says "Picked Onett. Waiting for your opponent." online as offline: the player should know what they sent before the result names it. Locked tiles do not exist online (unlock-all is forced).
- **The result**, once a visit, interrupting: "Corneria, your pick." or "Corneria, opponent's pick."; "Corneria." when both picked the same tile; "Random: Corneria, your pick." (or opponent's) when a Random pick won; "Random: Corneria." when both picked Random (the random tile and Start before any tile both count). The words are told apart from the final cell and the two picks, without repeating the base port's coin flip: a named pick equal to the final cell won, otherwise the Random one did. When one player picked Random and it rolled the other's named stage, the result credits the named pick ("Corneria, opponent's pick."), which names the stage the match is on either way.
- **Open question 1, answered: yes.** Online each copy's stage cursor answers to `pc_net_local_player()`'s port, which the local controller 1 feeds, and steering's stick on port 1's virtual pad reaches it: instance b (player 2) stepped Down to Icicle Mountain, Right to Princess Peach's Castle and Right to Kongo Jungle, each glide arriving (33, 7 and 6 frames at delay 0; the input delay added at higher delays), and picked with A. Player 2 steers stage select as player 1 does.
- **Scene words.** `scene_speech.cpp` already carried no stage select words online: issue 01 took stage select out of the scene table, and only results keeps "then wait for your opponent". Nothing to change.
- **Tests.** `test_sss_speech`: waiting after a named pick, the random tile and Start before any tile; the mid-glide pick online; nothing said while the other pick arrives before the result; each result form (your pick, opponent's pick, the same tile, a winning Random of either player, a named pick beside a Random one, both Random); the result said once a visit and again on the next; nothing offline.
- **Drive.** `tools/a11y/css_net.drive` now steers both players on stage select: player 1 to Princess Peach's Castle, player 2 (a fork build) to Kongo Jungle, each picking with A and each waiting on "sss: we picked" and "Waiting for your opponent." from its own log, then the result from its own side. A base port player 2 presses Start for Random. The default seed 7 always lets player 1's pick win; a run with `--seed 4` covers the other side ("Kongo Jungle, opponent's pick." on a, "Kongo Jungle, your pick." on b; against the base port "Random: Kongo Jungle, opponent's pick."). The `verify` skill describes the new steps and expected lines.
- **Online runs**, recordings agreeing in every one: fork against fork at delays 0, 2, 6 and 10 and over the 100 ms slow link; fork against the base port build (`../melee-pc-base/build`, at e833835, the last base merge's commit) at the same five; and seed 4 against both at delay 0. Every fork side logged waiting and its own result ("Princess Peach's Castle, your pick." on a, "..., opponent's pick." on b). The first delay 2 run ended with b crashing as the match loaded (`ARCHIVE SdIntro.dat: implausible header`, a PANIC in `lbarchive.c`, exit 0xC0000409) after both had said their results; the unchanged rerun passed. It matches the intermittent archive crash the LAN play issues recorded at the memory card screen, so the `verify` skill now names it as a known flake.
- **Docs.** `docs/a11y/netplay.md` (player 2 steers its own cursor, when each copy learns the other pick, the seed fixing the coin), `native-menus.md` (the third hook and why), CLAUDE.md (the status line, and stage select in the online compatibility paragraph).
- **Verification**, online level: build, style, unit tests (all sixteen passed, `launcher_data` included this time), a bounded `title` run, all thirteen menu drive scripts (`sss_skipped.drive` with no "Stage select" line), the five `css_net.drive` runs against fork and against base port, and the `lan_lobby.drive` run passed.

Left for the play test (issue 07), which needs two machines or a friend: how the second picker's wait of about 1.5 seconds before the result feels, and whether "your pick" and "opponent's pick" are the clearest words.
