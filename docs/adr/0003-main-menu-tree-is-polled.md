# The main menu tree is read by polling its state from one frame hook

Status: accepted (2026-09-27)

CLAUDE.md prefers a hook at each semantic transition and allows polling only where a screen would otherwise need many hooks. The main menu tree is that case. The whole tree is one scene whose state sits in one small global, `mn_804A04F0`: the menu screen, the hovered entry and, on several leaf screens, the row and its value. Nothing in the game writes that state through one function: every tree screen's think function changes it inline, and every leaf screen's opening function sets it again. So the fork reads the state instead of being told about it. A single hook, `pc_a11y_menu_frame()`, called from the menu scene's per-frame function `mnMain_Scene_OnFrame`, compares the state with the previous frame and speaks what changed. That is one line of footprint in `mnmain.c`, it sees every tree screen including the base port's Online, and it sees every leaf screen open, which lets leaf screens without a reader still say their name.

The fork also looks up each description itself, from the game's table of description numbers and the base port's `mnOnline_Description`, so the earlier `pc_a11y_menu_description` hook is removed.

## Considered options

- **Hooks at the two choke points in `mnmain.c`**: `mn_8022B3A0` builds every tree screen, and the selection-changed branch of `fn_8022AFEC` sees every hover change. Two lines cover the tree screens, and they name the transition exactly. But no leaf screen passes through either: each of about twenty leaf screens would need a hook in its own opening function, in its own decomp layer file, and each is a line to carry across a decomp sync.
- **The description hook as the signal**: the description is rebuilt when the hovered entry changes, so the existing hook already fired once per change. It never fires for Online or for the Online entry of VS. Mode, because the base port's branch returns before reaching it, and the game does not rebuild the description for a move made while a screen slides in.
- **The menu sound function as the signal** (`lbAudioAx_80024030`): one hook for every native menu, but it carries no context, so a poll would still be needed to find out what changed.

## Consequences

- A change is seen at most one frame late, which nobody hears.
- The poll runs in the menu scene only. The rules screen, the item switch and the name keyboard also open inside character select, where this hook does not run; reading them there needs a second call site.
- Re-entering the scene on the same screen and entry as before looks like no change, so the reader forgets its previous state whenever a scene is entered.
- Leaf screens that keep their row in private state are out of the poll's reach. Sound, Screen display, Language and Multi-Man Melee set their centre text through one shared helper, `Menu_InitCenterText`, and a hook there tells the fork which text was set, which identifies the row or choice.
- The fork reads unnamed decomp globals and tables (`mn_804A04F0`, `mn_803EB6B0`). A decomp sync that renames them breaks the build, not the behaviour, and the reads are confined to `game_access.c`.
