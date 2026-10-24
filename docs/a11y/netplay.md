# Netplay on Windows

What running the base port's online play on one Windows machine takes, learned while checking that steering holds up online and that two copies meet in the LAN lobby; then how Direct Connect meets a friend over the internet, and the fork's connection checker that tests a network for it. The base port's own netplay harness (`tools/net_test.py`) runs on Linux only; the fork's is `tools/a11y/drive_net.py`.

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

## Two copies in the LAN lobby

The LAN lobby (VS. Mode, Online, LAN play) finds other copies by mDNS, multicast DNS on UDP 5353 (`src/pc/net_lan.c`), and two copies on one Windows machine find each other there: multicast sent from one comes back to every listener on the machine. `drive_net.py --lan` runs them this way, and `tools/a11y/lan_lobby.drive` takes both from the title screen through the lobby into character select, backs out to the lobby, starts again and closes one copy, so the other loses its connection. A step `a quit` or `b quit` closes that copy alone.

- Discovery goes out over a real network interface, never loopback, so the machine needs one up with an address; the log names the one it uses. With none, the log says so and the lobby finds nobody.
- No `MELEE_NET`: each copy starts offline and walks the menus by its own presses until the session begins.
- Each copy needs its own `MELEE_NET_PORT`. The lobby's machine id mixes the port into the install id, so two copies of one install are told apart; both are listed by the computer's hostname.
- The lobby's count includes the machine itself: two copies show "2 players found - press START".
- Start on one copy makes it the host (player 1); the other, idle in its lobby, follows as player 2. The two then go into character select together as in a direct session.
- A copy that closes sends a goodbye: the other logs that its peer left and returns to a fresh, searching lobby.
- A fork build and a base port build pair in the lobby as they do in a direct session.
- Windows Firewall blocks incoming traffic on a Public network unless a rule allows the program, and asks the first time a program listens, which takes focus. Rules are per program path, so each build directory's `melee.exe` needs its own, allowing UDP and TCP in. A new build directory means a new prompt, best answered by the maintainer before an agent runs it.
- `MELEE_NET_RECORD` starts writing at boot and runs on into the session without a new header, so two copies' recordings share no frame numbers and are not compared. Desyncs are still logged.

## Direct Connect over the internet

Direct Connect (VS. Mode, Online) pairs two players by connect code, with no server of the project's own (`src/pc/net_match.c` on `src/pc/net_dht.c`, design in `docs/netcode-plan.md`, section 7).

- Rendezvous: each copy joins the public BitTorrent DHT and announces itself under a topic made from a code. A DHT node stores the announcing socket's address and port as it saw them, so a lookup of the friend's topic returns their router's public mapping.
- Hole punch: both copies then send to each other's public mapping from the same UDP socket. Each router lets the other's packets in because it has just sent to that address. The rollback session runs on over that socket.
- What it needs: a router that keeps one public port for a socket whatever it sends to (endpoint-independent mapping), on at least one side and usually both. A router that gives each destination a port of its own (symmetric NAT: some phone carriers, workplace and school networks) cannot be reached this way, and there is no relay and no port mapping (UPnP) to fall back on.
- Gaps the code shows: IPv4 only; candidates on private addresses are dropped, so two machines behind one router meet only if it loops traffic to its own public address back in (LAN play is the way there); a mesh VPN's addresses are private too. The game's port is left to the system by default, so port forwarding needs a fixed one first, which no settings row offers: `MELEE_NET_PORT` or the `net_port` line of `launcher.cfg`.

### Two copies on one machine

Two copies on one machine meet by Direct Connect only if the router sends traffic for its own public address back in (NAT loopback, also called hairpinning): `net_dht.c` drops candidates on private addresses, so each copy finds the other only at the router's public address. `drive_net.py --direct` runs two copies that way, each on its own port with a fresh code, and its `dial` step types one copy's code on the other's code keyboard.

- The maintainer's router, a TP-Link Archer AX73, has no loopback setting and loops nothing back by default: both copies reached the DHT within seconds, found each other at the router's public address, each on its own port (the router keeps a socket's port), and sent hellos for three minutes, none of which arrived.
- It does loop back ports it forwards. With UDP 42050 and 42051, the drivers' ports, forwarded to this machine, the two copies meet within about fifteen seconds and reach character select, a fork build against a base port build too. That forward is the one setup Direct Connect's scripted runs need here.
- DHT nodes also hand out strangers' addresses as candidates. Their hellos go unanswered, which is harmless.

### A session after a back-out

The base port's netcode keeps the last controller sample it read (`s_raw_last` in `net.c`) from one session to the next: `session_reset` leaves it, and the new session's first frames are filled from it before a fresh one is read. A back-out from character select holds B until the scene ends, so the next Direct Connect session opens with B pressed on the player who backed out, and the call screen takes that B as "stop": the session ends within two frames and that player lands on the Online screen. The LAN lobby ignores B while starting, so LAN play does not show it.

## The connection checker

`melee-netcheck.exe` (`src/pc/a11y/netcheck.cpp`, its pure parts in `netcheck_logic.cpp`) is a console program beside `melee.exe`, built by `a11y.cmake` and added to the CI package, for a player to try before a session with a friend. It links statically, so it runs alone, and prints plain lines for a screen reader.

- The check asks DHT nodes on several networks how they see the program's socket (the "ip" field of BEP 42 that the game's DHT also reads) and judges the router (`judge` in `netcheck_logic.cpp`, its verdicts `Mapping`). An answer the game's own vote would leave out is left out, and the most common answer stands over a few odd ones, which DHT nodes give now and then.
- The test with a friend: both run it and each types the other's printed address. Each sends probes to the other and answers every probe at once, also while its player is still typing, so a reply proves both directions and times the round trip. A side that hears probes but gets no replies reports one way only.
- The address holds only while the program runs: its port is the system's choice, like the game's, and a refresh query keeps the router's mapping while the player types.
- Everything it says but the prompts also goes to `melee-netcheck-result.txt` beside the program, so a friend can send their results back.
- Developer test: `--loopback` (the usage line lists the options) binds to 127.0.0.1 and skips the check, so two copies on one machine try the friend test without a firewall prompt. Two copies run from one `.exe` share the result file beside it, so the second copy is a copy of the program in a folder of its own. Without `--loopback` the program listens on every network, so the firewall rule of "Two copies in the LAN lobby" applies to its path too.

## How the online lobby is read

The lobby (scene `GS_ONLINE_LOBBY`, the first state of the online game mode, flow in `src/melee/gm/gmonlinemode.c`) fills an `OnlineLobbyView` each frame and draws it; the fork's hook, `pc_a11y_lobby_frame`, hands that view over once it is drawn, and the readers compare it with the last frame. The scene has two paths, each with the hook: LAN play's, and the internet lobbies' (Direct Connect, Unranked, Ranked, Profile). `a11y_game_lobby_state` in `game_access.c` reads the view into a snapshot by the online kind the menu picked: LAN play's list of machines for `lobby_speech.cpp`, Direct Connect's screens for `direct_speech.cpp`. Unranked, Ranked and Profile, and Direct Connect routed through LAN play's path by `MELEE_LAN_DIRECT` (a developer fixture), are not read and keep the scene announcement's "No speech yet".

### LAN play

The view has everything the screen shows: the machines listed (row 0 is this one), a phase and a status line.

- What pairs two machines: the protocol version, the version string and the disc image. A machine that differs in any of them is listed as another version and never paired.
- Machines are listed by hostname, cut to 15 characters, and rows can reorder, so the fork matches them by name, counting a repeated name: two copies on one computer share it.
- The phase does not tell every status apart: waiting for the host and connecting share one, and so do searching and a network that hides players. The fork tells them by the start of the status line, the base port's literal strings.
- Every way out of a session lands in a fresh, searching lobby, and by then the reason is gone: the lobby's entry disconnects, and a disconnect marks every ended session as "peer left", a clean back-out included. So the fork tells "Back to LAN play." from "Connection lost." by the scene the session left. Character select returns to the lobby when someone held B (its pending exit says so on its last frame) or when the other machine was lost; stage select, the match and results return only when it was lost. The pending exit is recorded even on frames rollback re-runs, since the frame that shows a back-out by the other player may be one.

### Direct Connect

Direct Connect's hub and code keyboard (`directEntryFrame`) and its call (the rest of the internet path) fill the same view: the hub as a list of rows with a cursor, the keyboard as 32 keys in rows of 8 with the entry line as its one row, the call as a phase and a status line. Each line is read as the screen shows it, with runs of spaces, the screen's padding, made one.

- The phase tells the screens apart where the view's screen does not. The hub and the keyboard always show the found phase, which a call never does. The frame a hub row starts a call still carries the hub's screen and rows with the call's phase, so any other phase is the call. The frame B leaves an empty keyboard for the hub shows nothing (a zeroed screen with the found phase), and the snapshot skips it.
- The hub reads the system clipboard every 30 frames and offers a code found there as its top row; when the row count changes the cursor jumps to the top without a press. A recent opponent's row ends with how long ago they played ("played 5m ago"), which changes as the clock runs, so the hovered row is compared without it.
- The keyboard's count typed comes from the entry line, "Friend's code   # K3X-Q2_-__": a '_' is a slot still to type.
- The keyboard's error stays on the hub after B leaves it (the base port does not clear it), so the hub's line under the list can show "A code has 8 characters after the #". The fork says only the title and the hovered row on that return.
- The base port swallowed B on the hub along with the keyboard's, so its "B: back" did nothing; the fork's fix in the scene's frame function leaves B to the hub.
- Y on the hub, and on the call while it searches, copies your code to the system clipboard, and the clipboard row calls the code found there: driven runs never use either (the `verify` skill, "Online runs").

The call's reading is in `wait_status.cpp`, apart from the rest, so that Unranked's search ("Looking for an opponent  0:12") can reuse it.

- While the call searches, its status line ends with the call's clock, "m:ss" from the call's start (`pc_net_match_progress`). The clock ticking is no change, so lines are compared with it left out; joining the network, calling, "isn't answering yet" at 0:45 and the lines without a clock each count as a change.
- A wait gives no sound of its own, so a long silence sounds like a hang. The fork says the current line again, with its clock, at a few moments timed from the call's start: early on, then on a steady rhythm once the base port's own change at 0:45 has passed, which the rhythm leaves out since the change is said anyway. The moment is read from the line's own clock where it shows one, so a reminder never says the second before it (the clock drawn is formatted a moment before the snapshot reads the call's own). Behind a line without a clock ("Found them but couldn't connect. Still trying...") the call's clock runs on, and the same moments repeat that line without a time. Reminders and changes are queued, as LAN play's lobby says its changes, so a new line soon after the opening leaves the buttons heard.
- Connecting and starting show the base port's line for a call with no reason, "Searching for an opponent...", even once the other player is found. Connecting lasts a frame or two; starting about two seconds, with the countdown line "Starting in 1..." above the status line (its count is never set on the internet path, so it always says 1), which the snapshot copies as drawn. The screen also lists both players' codes then; the fork says neither, so the player who waited never hears who called.
- The call names its own buttons, "Y: copy my code   B: stop", only beside a line with its clock; beside any other line the view names none and the screen draws "B: back". The frame a hub row starts a call still carries the hub's buttons, which the call never shows, so the reader takes the call's buttons from the line: the view's beside a clock, "B: back" otherwise.
- Every way out of a session lands in a fresh lobby, whose entry opens the hub again, so the returns are told apart by the scene the session left, as LAN play's are.

## Quick chat and steering share the D-pad

Quick chat (`src/pc/net_chat.c`) lets a player in a session send one of sixteen phrases with two D-pad presses, on character select, results and the lobby. It reads controller 1's physical buttons once a video frame, outside the simulation, so what it sees never reaches the game or the peer's inputs, only the phrases it sends. On character select steering uses the same D-pad, so while steering is on the fork's chat hook (`pc_a11y_chat_buttons`, called from `src/pc/vi.c`) hands chat the buttons without the D-pad there. Results and the lobby keep chat, and so does a player with steering off. The base port logs nothing when a phrase is sent; the chat line at the bottom of the screen ("You: Hello", "Opponent: Hello") is the only sign, so a check takes screenshots.

## Stage select needs both players

Online, each player picks a stage on their own, the picks are exchanged, and a coin flip on the session's seed chooses between them (`netStageSel_*` in `mnstagesel.c`). Start on one side alone leaves the match waiting, so a script presses Start or A on both.

- Each copy's stage cursor answers to its own player's port, which the local controller 1 feeds, so steering works for player 2 as for player 1.
- A copy reads the other's pick only once its own preview has shown, about 1.5 seconds after its own pick, so the player who picks second hears the result that long after.
- For a given pair of picks the coin flip depends on the seed alone, so a script with a fixed seed always gets the same side's pick.

## A base port build to play against

A fork build and a base port build pair when their protocol version, version string and disc match; the fork keeps the base port's version string for this. To test against the base port, build the commit of the fork's last base merge from `upstream/master` in a git worktree with its own build directory, leaving the fork's `build/` alone, and run `drive_net.py --base-b <its build directory>`. The base port says nothing, so a script's steps addressed to `b-fork` wait on b's speech and those addressed to `b-base` wait on scene changes or sleep instead.

The base port does not build natively under MSYS2 as it stands: the fork carries the fix in the root `CMakeLists.txt` (the Windows SDK level pinned for native MinGW builds, without which abseil's time zone code picks a WinRT path MinGW cannot compile, and `zlib1.dll` copied beside the executable). Apply those two blocks to the worktree, uncommitted, before building. Neither changes what the game simulates.

That build also lacks `MELEE_USER_DIR`, the fork's base port fix that moves the settings folder (`pc_get_pref_path` in `src/pc/paths.c`), unless the base port has taken it since. The drivers give each fork instance a fresh settings folder of its own, so a starts from the default settings, but a base port b reads the maintainer's own `%APPDATA%\melee-pc` (player name, Unlock everything and the rest) and saves `launcher.cfg` back into it. The two still pair and stay in sync: the session agrees the rules and the unlock state between the peers, whatever each one's settings say.

## Timing gotchas for scripts

- Start pressed within a few milliseconds of Ready to Fight appearing is ignored; wait a second first, as the offline scripts do.
- A glide (steering moving the hand by generating stick input) takes its offline frame count plus the input delay, since its first stick input counts only after the delay. A glide that a later press cuts short is not an error.
