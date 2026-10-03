# 03 Quick chat muted on character select

Status: ready-for-agent
Type: task

Quick chat and steering both read the D-pad on online character select, so steering sends stray phrases to the other player. Spec: `.scratch/lan-play/spec.md`, section "Quick chat on character select".

- One hook in `src/pc/vi.c` filters the buttons passed to `pc_net_chat_poll`: while steering is on and the scene is character select, the D-pad bits are cleared. The lobby and results keep chat.
- The hook changes only what chat sees, never the pads the game or the netplay code sample.
- Verify online: a script steps around character select with the D-pad in a session and shows no chat sent (no chat line in either instance; check `net_chat.c` for anything logged on send). `MELEE_NET` sessions are fine for this; the lobby is not needed.
- A later switch may let the player choose chat over steering; that is out of scope here.
