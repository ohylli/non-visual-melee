# 08 Play test multiplayer by ear

Status: ready-for-human
Type: task
Blocked by: none

Split from issue 07, whose offline play test passed on 2026-10-11. These two items need a second player, so they wait until a friend can play.

The maintainer plays stage select with NVDA and listens for:

- Online, against a friend or a second instance: "Waiting for your opponent." after a pick, the wait of about 1.5 seconds the second picker hears before the result, and whether the result wording ("Corneria, your pick.", "Random: Corneria, opponent's pick.", "Corneria." when both picked it) is the clearest.
- Offline, the VS stage selection rule Turns or Loser with a player other than controller 1 choosing: whether the opening ("Player 3 chooses. Steering needs controller 1.") makes clear who chooses and why the D-pad does nothing, and whether the hover speech helps while that player moves the stage cursor with their own stick. Reaching it may take more than one match (under Turns the first turn may fall to player 1; under Loser someone must lose first), and the Rules screen where the rule is set has no speech yet.

Findings become new issues here.
