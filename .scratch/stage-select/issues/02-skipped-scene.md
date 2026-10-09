# 02 The skipped scene names the stage

Status: ready-for-agent
Type: task
Blocked by: 01

Spec: `.scratch/stage-select/spec.md`, "The skipped scene".

With the VS stage-select rule set to Random or Ordered, `gm_80167FC4` sets the stage in advance and stage select leaves on its first frame. The scene announcement probably still fires there; not yet checked in a run.

- First confirm in a run that the announcement fires.
- Find where the chosen stage can be read at that moment (the match's rules, `sss_data->vs.start.rules.stkind`, or the forced stage) and say "Stage: Corneria." instead of the opening, from issue 01's stage name table.
- The opening must not be said as well.

## Done when

- A unit test covers the skipped scene's words and the absence of the opening.
- A drive run with the rule set to Random (VS mode's rules screen) logs "Stage: ..." and the match, and no "Stage select." line. If the script joins verification, the `verify` skill lists it.

## Comments
