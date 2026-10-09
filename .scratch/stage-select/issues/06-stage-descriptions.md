# 06 Stage descriptions

Status: ready-for-agent
Type: task
Blocked by: 01

Spec: `.scratch/stage-select/spec.md`, "Stage names and descriptions".

- Take a screenshot of each stage's large preview on stage select with every stage unlocked. An online session forces unlock-all; `tools/a11y/drive_net.py` with a script that stops at stage select reaches it. Keep the images in the scratchpad, not in git.
- Write one or two plain sentences per stage, 29 in all, about what a player would want to know:
  - the ground's shape: one flat platform, several levels, walk-off edges;
  - moving parts and hazards;
  - its feel.
- Base them on the previews and on what `docs/a11y/gameplay-state.md` records; say nothing that a preview and the code don't support.
- Hover says the name and the description as one announcement: "Corneria. A fight on the hull of a ship flying over a city." Locked tiles and the random tile have no description.

## Done when

- The table has a description for every stage, checked against its preview.
- Unit tests and the drive scripts' expected hover lines are updated.
- Issue 07 asks the maintainer to judge the descriptions.

## Comments
