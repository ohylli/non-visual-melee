# Netplay on Windows

What running the base port's online play on one Windows machine takes, learned while checking that steering holds up online. The base port's own netplay harness (`tools/net_test.py`) runs on Linux only; the fork's is `tools/a11y/drive_net.py`.

## Mental model

Two copies of the game on one machine talk over loopback (UDP to 127.0.0.1), each told the other's port. Both simulate both players: each frame, each copy samples its own player's controller, sends it to the other, and runs the frame with both players' inputs. When an input arrives late, the copy that guessed rolls back and re-runs the frames since (rollback). With an input delay of n frames, a press counts n frames after it is made, which gives the peer's input time to arrive and makes rollbacks rarer.

```
instance a (player 1)                      instance b (player 2)
key pipe --> port 1 pad --UDP loopback--> port 1 pad
             port 2 pad <--UDP loopback-- port 2 pad <-- key pipe
```

So a press on instance a moves player 1 in both copies, and anything that runs only on a (speech, steering's decisions) must leave the simulation alone: the fork's steering writes only into port 1's pad, before it is sent, so b receives it as player 1's input.

## A session from boot

A copy started with `MELEE_NET` (the peer's address and port), `MELEE_NET_PORT`, `MELEE_NET_PLAYER` (0 or 1) and a shared `MELEE_NET_KEY` connects at boot. The handshake finishes on the title screen, and from then on the two walk the ordinary menus in sync: player 1's presses move the menus in both, and every scene change waits for the peer to reach it (a scene hand-off). No LAN lobby is needed to reach character select. On character select each copy has its own hand, so player 2 chooses by its own presses.

- `MELEE_SEED` gives both the same random seed, so runs repeat.
- `MELEE_NET_DELAY` pins the input delay in frames; left unset it is automatic and changes with the measured ping.
- `MELEE_NET_SIM_DELAY_MS` holds every packet a copy sends that long, a slow link without a second machine.
- `MELEE_NET_RECORD` writes each frame's four controllers, the game state's checksum, the seed and the scene to a file. Two recordings of one session must agree frame for frame; the first frame they differ is where the copies' simulations parted. The side that leaves first records its next frames offline, with its peer's port unplugged, so a comparison stops at the first disconnect either side logs.
- Each copy needs its own `MELEE_LOG_FILE` and its own key pipe for scripted input. `drive.py` names its pipe after its process id, and `drive_net.py` adds the instance name.
- Each copy drops a few early packets: the guest does not know the host's session id until the handshake tells it.

## Stage select needs both players

Online, each player picks a stage on their own, the picks are exchanged, and a coin flip on the session's seed chooses between them (`netStageSel_*` in `mnstagesel.c`). Start on one side alone leaves the match waiting, so a script presses Start on both.

## A base port build to play against

A fork build and a base port build pair when their protocol version, version string and disc match; the fork keeps the base port's version string for this. To test against the base port, build the commit of the fork's last base merge from `upstream/master` in a git worktree with its own build directory, leaving the fork's `build/` alone, and run `drive_net.py --base-b <its build directory>`. The base port says nothing, so a script's steps addressed to `b-fork` wait on b's speech and those addressed to `b-base` wait on scene changes or sleep instead.

The base port does not build natively under MSYS2 as it stands: the fork carries the fix in the root `CMakeLists.txt` (the Windows SDK level pinned for native MinGW builds, without which abseil's time zone code picks a WinRT path MinGW cannot compile, and `zlib1.dll` copied beside the executable). Apply those two blocks to the worktree, uncommitted, before building. Neither changes what the game simulates.

## Timing gotchas for scripts

- Start pressed within a few milliseconds of Ready to Fight appearing is ignored; wait a second first, as the offline scripts do.
- A glide (steering moving the hand by generating stick input) takes its offline frame count plus the input delay, since its first stick input counts only after the delay. A glide that a later press cuts short is not an error.
