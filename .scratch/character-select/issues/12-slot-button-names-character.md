# 12 A player slot's button names its character

Status: resolved (2026-10-31)
Type: task
Blocked by:

Found in the play test of issue 08. A hand reaching a player slot's HMN/CPU button says the slot's kind only, "Player 1: human". It would help to hear the character too: "Player 1: human, Mario".

Details to settle when this is picked up.

## Comments

2026-10-31: fixed in `css_speech.cpp`; the maintainer play-tested it by ear. A hand reaching a player slot's HMN/CPU button now says the slot as a press of it does: "Player 1: human, Mario", "Player 1: human, no character" before a choice, and "Player 2: closed" alone. A step down from the portraits onto the carried coin's own slot button keeps the kind alone ("Back to Yoshi. Player 2: CPU"), since the step's first sentence already says where the coin goes and the slot has no character while the coin is in the hand.
