# 03 Quick chat muted on character select

Status: resolved (2026-10-03)
Type: task

Quick chat and steering both read the D-pad on online character select, so steering sends stray phrases to the other player. Spec: `.scratch/lan-play/spec.md`, section "Quick chat on character select".

- One hook in `src/pc/vi.c` filters the buttons passed to `pc_net_chat_poll`: while steering is on and the scene is character select, the D-pad bits are cleared. The lobby and results keep chat.
- The hook changes only what chat sees, never the pads the game or the netplay code sample.
- Verify online: a script steps around character select with the D-pad in a session and shows no chat sent (no chat line in either instance; check `net_chat.c` for anything logged on send). `MELEE_NET` sessions are fine for this; the lobby is not needed.
- A later switch may let the player choose chat over steering; that is out of scope here.

## Comments

### 2026-10-03: implemented

- The hook is `pc_a11y_chat_buttons` (`src/pc/a11y/hooks.cpp`), wrapped around the buttons `pc_frame_boundary` hands `pc_net_chat_poll` in `src/pc/vi.c`. It clears the D-pad bits while the scene entered last is character select and the character select reader steers.
- `net_chat.c` logs nothing on send or receive, so the check was by screenshots: `css_net.drive`'s steps up to Jigglypuff, with both windows shot after the second and fourth D-pad presses. Before the change a showed "You: Hello" and b "Opponent: Hello"; after it neither showed a chat line. No permanent script checks it, since a check needs someone to look at the screenshots.
- The base port's chat prompt ("CHAT: D-PAD twice ...") still shows on character select while chat ignores the D-pad. The only player it misleads is one who is steering, and that player cannot see it, so it was left alone.
- One post-change run of that check crashed at the memory card screen, before any session: `ARCHIVE NtMemAc: implausible header`, a PANIC in `lbarchive.c` (exit 0xC0000409). It passed on an unchanged rerun. Like the crash noted under issue 02, it looks like an intermittent base port bug.
