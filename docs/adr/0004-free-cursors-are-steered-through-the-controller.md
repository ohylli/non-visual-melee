# Free-cursor screens are steered through the controller

Status: accepted (2026-09-30)

Character select, and later stage select, has a free cursor: a hand that slides over the screen at the speed the stick gives it, between targets separated by empty space. A blind player can hear where the hand is, but cannot aim it. So the fork moves it for them. A D-pad press (a step) picks the neighbouring target, and the fork pushes the hand there by generating stick input on the player's own controller (a glide), the way a sighted player would push the stick. It writes nothing else: no position, no selection, no state of the screen. The one place it writes is a hook in `publish_locked` (`src/pc/keyboard.c`), `pc_a11y_pad`, which adds the fork's stick to port 1's virtual pad before the game and the netplay code sample it. The player still presses A, B and Start themselves.

Online this keeps the fork compatible with base port builds. Both machines simulate the same frames from the same controller data, and the peer receives the steered stick as it receives any player's, so it cannot tell the difference. The steering logic itself can differ between the two machines, or between two versions of the fork; only its output travels.

## Considered options

- **Guidance only**: speech tells the player where the hand is and which way the nearest target lies, and the player pushes the stick. Nothing is written. But the hand moves by the square of the tilt, a keyboard only gives a full push, and a target 7 units wide goes by in six frames at full speed, so hitting it by ear is slow and tiring.
- **Shaping the player's stick input**: the player pushes in a direction, and the fork slows or bends the push to stop on the next target. It writes only controller input too, but it has to guess the player's intent from a stick that also belongs to a sighted helper, and a keyboard's all-or-nothing push leaves little to shape.
- **Writing the screen's state, or replacing the screen**: set the hand's position or the chosen character directly, or draw a fork menu in place of character select. Precise and simple offline, but online the peer's copy of the screen would not change, and the two machines would disagree without noticing until the match broke. It also means writing into a file whose state is all private.
- **A list of targets with a confirm button**: a fork menu of characters and buttons, choosing an entry moves the hand there and presses A. It hides the screen's own layout, which a blind player shares with sighted players, and pressing buttons on the player's behalf is a larger step than moving a stick.

## Consequences

- The glide has to live with the controller's timing: input arrives some frames after it is published, more online, and samples can be repeated or lost. It measures the delay by matching what the game applied with what it asked for, and gives way as soon as a stick it did not ask for moves the hand.
- A steered stick is published once and then dropped unless the next simulated frame asks again, so a scene change, a pause or the port menu cannot leave the stick tilted.
- The D-pad presses that steer also reach the game and the peer. Character select ignores the D-pad, so they change nothing there.
- Steering follows the accessibility switch. With it off, the D-pad does nothing on character select, as in the base port.
- The glide (`steering.cpp`) knows character select's cursor formula and nothing else about the screen. Stage select moves its cursor by its own formula, so reusing the glide there means making the formula a parameter.
