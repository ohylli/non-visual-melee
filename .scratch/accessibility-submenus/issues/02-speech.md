# 02 What the submenus say

Status: ready-for-agent
Type: task
Blocked by: 01

`.scratch/accessibility-submenus/spec.md`, "Speech", "The rows that open a submenu" and "The title line".

## Machinery

- **Opening a submenu** says its label, then the first row's ordinary announcement, interrupting: "Cue settings. Cue volume, slider, 100%. Left and right to adjust." and its help (spec open question 4).
- **A row that opens a submenu** reads as its label, "Enter to open" and its description, with no role word, in both pages (spec open question 5).
- **Going back** is the ordinary focus announcement of the opening row.
- **The title line** is never read as a line by Up and Down (spec open question 6).
- **Tests.** Both readers' headless tests check the words: an opening row, the opening of each submenu including the nested one, going back, and Up and Down never stopping on the title line.

## Done when

- The headless tests above pass in both pages.
- A `drive.py --launcher --muted` run and a `drive.py --muted` run into the port menu log the opening and going back in the spec's words.
- `python tools/check_style.py` passes.
