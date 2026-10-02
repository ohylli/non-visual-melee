# 10 The first step into the portraits says too much, in the wrong order

Status: needs-triage
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
