# 02 The stats panels and the reading cursor

Status: ready-for-agent
Type: task
Blocked by: 01

Spec: `.scratch/results/spec.md`, "The stats panels and the reading cursor".

## Machinery

- **The snapshot grows.**
  - Each port's page, ready flag and the number of pages.
  - Every row of each page, as data: page 0's opponents with KOs and falls (name tags where the game uses them) and SDs; the 23 stats with their values; each bonus's kind and points.
  - Leave out the hidden first row, if it exists (open question 6).
- **Labels.** The stat labels, units and bonus names are decoded from SdRst. Stat labels get a space before each inner capital (open questions 1 and 2).
- **Controller 1's presses.** The reader sees the newly pressed D-pad directions and Z (open question 3).
- **The reading cursor**: the fork's own state, starting on your stats.
  - A page change resets it to the page's first row.
  - D-pad up and down step through the rows, repeating the row at an edge.
  - D-pad left and right switch players, keeping the row number clamped and saying the player then only what differs (the spec's table). At an edge the edge player is said again.
  - Z says the player's summary line.
- **Speech.**
  - The stats opening, by your press or another's, says your page's name and first row, with the keys on the first results of the run.
  - Your page changes are said; the game's silent ends stay silent.
  - Others' paging and all scrolling stay silent.
  - Bonus mode opens on Bonuses.

## Done when

- Unit tests cover every row form, page changes and their silent ends, stepping and its edges, each "what differs" case of the spec's table, the player edges, Z, the stats opened by another player, the keys once a run, and an unknown label.
- A drive script reaches the stats and checks a page change each way, a step down and up, a player switch, and Z. It can extend `results.drive` or be a new script beside it.
- Build, unit tests and style pass.
- The "Results in detail" section has the pages, the labels and the answers to open questions 2, 3, 6 and 7.

## Comments
