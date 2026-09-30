# 09 Say when someone else changes your player slot

Status: needs-triage
Type: task
Blocked by:

Found in the review of 3d2a5cf (issue 02). The spec, section "Stage 1: announcements", asks for "Online, the other player's slot changes | as for one's own." A change to the local player's own slot is not spoken today unless their own hand made it.

## What the game allows

When any hand presses A, `mnCharSel_CursorThink` tries the HMN/CPU button of every slot, not only the hand's own. It skips a slot only while:

- one of its sliders is held;
- its coin is in a hand;
- its own hand holds something;
- its name tag window is open;
- it is the fourth slot in Camera mode.

Nothing checks whose hand pressed, and the base port adds no online rule here. So online, while the local player's hand is empty, the other player can press their tab and make them a CPU, then a closed slot.

The game also turns a human slot into a CPU when its controller is unplugged (the same function, where a hand's controller reports an error).

## Why it is silent

`CssSpeech::slot_announcements` drops a change to the local slot unless the local hand is on its button (`else if (!own)`). That silence is meant for one case: the local slot opening by itself as the local hand first reaches the portraits, which "Holding your coin" already says.

## A likely fix

At that opening, the local hand holds its own coin, and the game never lets anyone press a slot's button while that slot's own hand holds anything. So `!holds(i)` in place of `!own` may keep the opening silent and queue other players' changes to the local slot. A unit test needs to confirm the coin is in hand on the frame the slot opens.

Open for the maintainer:

- The wording. "Player 1: CPU, Fox" matches other slots; "You: CPU" or "Your slot: CPU" names the player directly.
- Whether it interrupts rather than queues. Losing your own slot matters more than another player's change.
- Whether an unplugged controller should be said offline too.

## Comments
