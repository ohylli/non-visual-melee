# 03 Stocks and the Teammates switch

Status: ready-for-agent
Type: task
Blocked by: 01

Spec: `.scratch/match-announcements/spec.md`, "Stocks" and "Who is announced, in which voice".

## What to build

- **Stocks.** In stock matches only, said on the tick the count changes, rises and falls alike: "2 stocks", "last stock" at one, after the label where there is one ("P2, 2 stocks", "CPU 3, last stock"). At 0 nothing: the game's announcer calls the elimination. A team stock steal raises a count and is said the same way (open question 4).
- **The rows**, after the damage rows: "Your stocks" and "Opponents' stocks", switches, on by default; "Teammates", a switch, on by default, whether teammates are announced at all, damage and stocks.
- **A stock line replaces** a waiting damage line about the same player, as the voices' queueing rules give for one subject.

## Done when

- The announcer's unit test covers a loss, last stock, 0, a steal, each switch, a teammate off and on, a timed match with no stock lines, and a stock line replacing a waiting damage line.
- A drive run of the debug match with stocks shows the stock lines of both players.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
- `docs/a11y/gameplay-state.md` has the answer to open question 4.
