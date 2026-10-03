#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
r"""Drive two game runs in one netplay session over loopback, from one script.

Windows only. Starts build/melee.exe twice as drive.py does (silent, in the
background, speech off, steering on), connected to each other: instance a is
player 1 (P1, controller port 1) and instance b player 2. Each has its own key
pipe, so a press on b drives port 2. The session begins at the title screen
and the two walk the ordinary menus in sync, so a's presses move the menus of
both.

  python tools/a11y/drive_net.py [options] SCRIPT_FILE

Every step is a drive.py step (see drive.py --help) addressed to one
instance by a first word, a or b:

  a press Start until "boot scene: game mode"
  b wait '"Character select\. Player 2, no character\."'

A wait or search looks at that instance's output only, and named groups
are kept per instance. sleep and quit may stand without an instance; quit ends
the script, and the run ends with an implicit one, which closes both.

With --base-b, instance b is a base port build, the one in that build
directory, so it logs no [a11y] lines. A step addressed to b-fork runs only
when b is the fork's build and one addressed to b-base only when it is the
base port's, so one script can wait on b's speech in the one case and wait
otherwise in the other:

  b-fork wait '"Holding your coin'
  b-base sleep 1

Each instance's output goes to OUT_DIR/a.log and b.log, its game log beside
it (a.log.game.log) and its recording of controller data (MELEE_NET_RECORD)
to a.rec and b.rec. After the script, the run fails unless both logs show a
session (net: rollback with), neither logs net: DESYNC, and the two
recordings hold the same data for the frames both recorded in the session.
The summary lists the delay each side used and every glide's frames.

With --lan, the two start with no session: each walks the menus on its own
presses into the LAN lobby, where they find each other by mDNS over the
machine's network interface, and Start on one makes it the host (player 1)
and pulls the other in as player 2. Each instance's recording would start at
boot and run on into the session without a new header, so the two share no
frame numbers: --lan records nothing and leaves desyncs to net: DESYNC.
Windows Firewall must let melee.exe take UDP in, or the first run prompts
and steals focus.
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive  # noqa: E402
from drive import Game, ScriptError, parse_steps  # noqa: E402

NAMES = ("a", "b")
# Steps addressed to b that run only with b a fork build, or a base port
# build (--base-b).
ONLY = {"b-fork": False, "b-base": True}
# One frame of a recording (src/pc/net_snapshot.c FrameRecord): four
# PADStatus of 16 bytes, the checksum, the RNG seed and the scene. Each
# session starts with "MRC5" and the seed.
MAGIC = b"MRC5"
HEADER = 8
RECORD = 76
PADS = 64
DISCONNECTED = re.compile(r"net: disconnected at frame (\d+)")


def records(path):
    """The frames of the last session in a recording, oldest first. A
    session that restarts writes its header again; the last header that
    leaves whole records is the live one."""
    with open(path, "rb") as f:
        data = f.read()
    p = len(data)
    while (p := data.rfind(MAGIC, 0, p)) >= 0:
        if (len(data) - p - HEADER) % RECORD == 0:
            body = data[p + HEADER:]
            return [body[i:i + RECORD] for i in range(0, len(body), RECORD)]
    raise ScriptError(f"{path}: no whole recording of {RECORD}-byte frames")


def compare_recordings(path_a, path_b, end):
    """A list of problems: the first frame whose controller data or checksum
    differ between the two, among the frames both recorded before end."""
    rec_a, rec_b = records(path_a), records(path_b)
    both = min(len(rec_a), len(rec_b), end)
    for f in range(both):
        if rec_a[f][:PADS] != rec_b[f][:PADS]:
            return [f"controller data differ from frame {f} of {both}"]
        if rec_a[f] != rec_b[f]:
            return [f"checksum, seed or scene differ from frame {f} of {both}"]
    print(f">> recordings agree over {both} frames (a {len(rec_a)}, b {len(rec_b)})", flush=True)
    return [] if both else ["the recordings hold no frames"]


def session_end(games):
    """The first frame either game simulated outside the session. The side
    that leaves first records its next frames offline, its peer's port
    unplugged, while the other may still record that frame in session."""
    ends = [int(m.group(1)) for game in games.values() for line in game.output.lines
            if (m := DISCONNECTED.search(line))]
    return min(ends, default=sys.maxsize)


# Lines the summary repeats: the session's start, its delays and the LAN
# lobby's discovery and host election.
SUMMARY = ("net: rollback with", "net: delay ", "peer silent", "lan: found", "lan: lost",
           "lan: host election", "lan: failed", "lobby: entering CSS")


def check_logs(games):
    problems = []
    for name, game in games.items():
        lines = game.output.lines
        if not any("net: rollback with" in line for line in lines):
            problems.append(f"{name} never started a session")
        problems += [f"{name}: {line}" for line in lines if "net: DESYNC" in line]
        for line in lines:
            if any(s in line for s in SUMMARY):
                print(f">> {name} {line}", flush=True)
        for line in lines:
            if "glide to " in line:
                print(f">> {name} {line[line.index('glide to '):]}", flush=True)
    return problems


def run(args, steps):
    common = {"MELEE_SEED": args.seed, "MELEE_NET_DELAY": args.delay}
    if not args.lan:
        common["MELEE_NET_KEY"] = args.key
    if args.sim_delay_ms:
        common["MELEE_NET_SIM_DELAY_MS"] = str(args.sim_delay_ms)
    out = {name: os.path.join(args.out_dir, name) for name in NAMES}
    for name in NAMES:
        # A run that never starts its game must not be checked against an
        # earlier run's recording.
        if os.path.exists(out[name] + ".rec"):
            os.remove(out[name] + ".rec")
    if args.base_b:
        print(f">> b is the base port build in {args.base_b}", flush=True)
    games = {}
    failed = False
    try:
        for player, name in enumerate(NAMES):
            env = dict(common, MELEE_NET_PORT=str(args.port + player))
            if not args.lan:
                env["MELEE_NET"] = f"127.0.0.1:{args.port + 1 - player}"
                env["MELEE_NET_PLAYER"] = str(player)
                env["MELEE_NET_RECORD"] = out[name] + ".rec"
            exe = os.path.join(args.base_b, "melee.exe") if name == "b" and args.base_b else None
            games[name] = Game(args, out[name] + ".log", env, name, exe)
        for words in steps:
            if words[0].lower() == "quit":
                break
            if words[0].lower() == "sleep":
                print(f">> {' '.join(words)}", flush=True)
                time.sleep(float(words[1]))
                continue
            if words[0] in ONLY:
                if ONLY[words[0]] != bool(args.base_b):
                    continue
                words = ["b"] + words[1:]
            games[words[0]].step(words[1:])
    except ScriptError as e:
        print(f">> FAILED: {e}", flush=True)
        failed = True
    finally:
        codes = {name: game.close() for name, game in games.items()}
    problems = [f"{name} exited with code {code}" for name, code in codes.items() if code != 0]
    try:
        problems += check_logs(games)
        if not args.lan:
            problems += compare_recordings(out["a"] + ".rec", out["b"] + ".rec", session_end(games))
    except (OSError, ScriptError) as e:
        problems.append(str(e))
    for problem in problems:
        print(f">> FAILED: {problem}", flush=True)
    if not failed and not problems:
        print(">> passed", flush=True)
    return 1 if failed or problems else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("script", help="script file")
    ap.add_argument("--out-dir", default=os.path.join(drive.REPO, "build", "drive", "net"),
                    help="where the logs and recordings go (default build/drive/net)")
    ap.add_argument("--delay", default="auto", help="MELEE_NET_DELAY, frames or auto (default auto)")
    ap.add_argument("--sim-delay-ms", type=int, default=0,
                    help="MELEE_NET_SIM_DELAY_MS on both sides: each holds every packet it sends that long")
    ap.add_argument("--port", type=int, default=42050, help="a's UDP port; b's is the next (default 42050)")
    ap.add_argument("--key", default="a11ytest", help="MELEE_NET_KEY, shared by both")
    ap.add_argument("--lan", action="store_true",
                    help="no session at boot: the two meet in the LAN lobby")
    ap.add_argument("--seed", default="7", help="MELEE_SEED, shared by both")
    ap.add_argument("--base-b", metavar="BUILD_DIR",
                    help="run instance b from a base port build directory, such as a worktree's build")
    ap.add_argument("--disc", default="melee.iso", help="disc image, relative to the repo root")
    ap.add_argument("--all", action="store_true", help="print every game line, not just [a11y] and boot scene")
    ap.add_argument("--timeout", type=float, default=30, help="default wait timeout in seconds")
    ap.add_argument("--max-frames", type=int, default=60 * 60 * 10,
                    help="MELEE_EXIT_AFTER_FRAMES backstop (default 10 minutes)")
    args = ap.parse_args()
    # What Game reads from drive.py's options: both runs start at the title
    # screen, silent, in the background and without speech.
    args.scene, args.speech, args.sound, args.focus = "title", False, False, False
    sys.stdout.reconfigure(errors="replace")
    if sys.platform != "win32":
        ap.error("Windows only; on Linux use tools/net_test.py")
    with open(args.script, encoding="utf-8") as f:
        text = f.read()
    try:
        steps = parse_steps(text, False)
    except ScriptError as e:
        ap.error(str(e))
    for words in steps:
        cmd = words[0].lower()
        if cmd == "sleep" and len(words) != 2 or cmd == "quit" and len(words) != 1:
            ap.error(f"bad step: {' '.join(words)}")
        if cmd not in ("sleep", "quit") and (words[0] not in NAMES + tuple(ONLY) or len(words) < 2):
            ap.error(f"a step begins with a, b, b-fork or b-base: {' '.join(words)}")
    if args.base_b and not os.path.isfile(os.path.join(args.base_b, "melee.exe")):
        ap.error(f"no melee.exe in {args.base_b}")
    sys.exit(run(args, steps))


if __name__ == "__main__":
    main()
