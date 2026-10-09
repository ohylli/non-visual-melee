# 05 Stage select online

Status: ready-for-agent
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
