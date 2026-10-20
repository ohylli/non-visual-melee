# 05 Drive scripts, the verify skill and the docs

Status: ready-for-agent
Type: task
Blocked by: 04

`.scratch/accessibility-settings/spec.md`, "Tests" and "Docs".

## Done when

- The `verify` skill's menus level has a launcher script and a port menu script that open the Accessibility category, walk every row, change a switch, a volume and Ledge warning distance and back, press a Preview, and check the run's `a11y.ini` afterwards; and a run that turns Accessibility off and on again from the same row.
- `docs/a11y/rmlui-screens.md` covers the fork's own category: how its rows are built from the model and handed off, and the wiring's place in the footprint.
- CLAUDE.md gains an "Accessibility status" line for the feature.
- Every level of the `verify` skill the change calls for passes.
