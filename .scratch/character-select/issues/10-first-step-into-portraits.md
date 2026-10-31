# 10 The first step into the portraits says too much, in the wrong order

Status: resolved (2026-10-31)
Type: task
Blocked by:

Found in the play test of issue 08. Entering VS. Mode, Melee and pressing Up twice:

```
[a11y] speak interrupt: "Character select. Player 1, no character."
[a11y] speak interrupt: "Player 1: closed"
[a11y] glide to "Player 1: closed" at (-32.10, -2.20)
[a11y] glide to "Player 1: closed" ended after 18 frames: arrived
[a11y] speak interrupt: "Pichu"
[a11y] glide to "Pichu" at (-23.10, 4.50)
[a11y] speak queue: "Player 1: human, no character"
[a11y] speak queue: "Holding your coin"
[a11y] glide to "Pichu" ended after 11 frames: arrived
```

- "Player 1: human, no character" after "Pichu" is confusing. It is the local slot opening by itself as the hand reaches the portraits, which issue 09 says is meant to stay silent; here it was spoken, perhaps because the hand was gliding.
- "Holding your coin" would read better before "Pichu".

Details to settle when this is picked up.

## Comments

2026-10-31: fixed in `css_speech.cpp`; the maintainer play-tested it by ear. The game opens a closed slot and puts its coin in the hand on the same frame, as the hand enters the portraits (`mnCharSel_CursorThink`, while the slot has no portrait chosen). The slot change was said because the hand had been on the slot's button the frame before, which looked like a press of it; a slot whose coin or slider is in the hand is now never said to change, since no button reacts then. A step into the portraits that will catch the player's own coin now says it first ("Holding your coin. Pichu"), and the pickup that follows is silent, as a step down already does for the coin going back. The same holds in Classic and Training. The play test's two Ups now say "Player 1: closed", then "Holding your coin. Pichu", and nothing more.
