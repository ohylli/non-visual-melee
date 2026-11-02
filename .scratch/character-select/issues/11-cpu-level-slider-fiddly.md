# 11 The CPU level slider is fiddly to move one level

Status: resolved (2026-11-02)
Type: task
Blocked by:

Found in the play test of issue 08. Moving a held CPU level slider one level takes very small taps on an Xbox controller's D-pad, and care on the keyboard too. Issue 05 meant a D-pad press to move one level, 1.25 units, so a press that moves more than that is worth finding out about: a held press repeating, the glide overshooting, or the game moving the slider on its own as well.

Details to settle when this is picked up.

## Comments

- **Resolved by the base port.** A base port change some time ago adjusted stick sensitivity, and since then moving a held CPU level slider one level by D-pad is no longer fiddly, by the maintainer's ear. No fork change.
