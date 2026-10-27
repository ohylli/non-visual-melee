#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
r"""Run the drive scripts of a verify level side by side and report each.

Windows only. Runs drive.py and drive_net.py runs at the same time, each
with its own transcript, shader cache slot (drive.py --cache-slot) and UDP
ports, and prints one line per run as it ends, the whole set's time, and
the failure lines of each run that failed. Exits 1 if any run failed.

  python tools/a11y/run_level.py menus
  python tools/a11y/run_level.py online [--base-b ../melee-pc-base/build]
  python tools/a11y/run_level.py css_steps sss_hover

Arguments are groups or script names, run in that order, a group's runs
side by side:

  menus   the menu drive scripts (MENUS), the launcher's three and the
          port menu's two among them
  online  css_net.drive at delay 0, delay 10 and over a 100 ms slow link,
          results_net.drive (--muted), lan_lobby.drive (--lan),
          direct_call.drive, and direct_back.drive
          and direct_lost.drive (--direct), the last two one after the other
          since both use the ports the router forwards; with --base-b the
          two are left out, since the base port build would write the
          maintainer's recent opponents
  NAME    an online run by its name (css_net-delay0, css_net-delay10,
          css_net-slow, results_net, lan_lobby, direct_back, direct_lost),
          else
          tools/a11y/NAME.drive through drive.py; the names given in a row
          run side by side

  python tools/a11y/run_level.py lan_lobby --base-b ../melee-pc-base/build

--jobs caps how many games run at once (default 8); a drive_net.py run
counts two. The environment passes through, so MELEE_A11Y_CUES=1 in front
runs every script with cues on.

Transcripts go to build/drive/level: NAME.log for drive.py, a directory
NAME for drive_net.py.
"""
import argparse
import contextlib
import os
import re
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

MENUS = ["rules_switches", "tree_walk", "css_rules", "css_setup", "rules_screens", "sss_steps",
         "sss_skipped", "sss_glide_pick", "leaf_screens", "sss_hover", "css_classic", "css_training",
         "css_slots", "css_portraits", "css_steps", "results", "results_no_contest", "results_cpu4",
         "results_teams", "training_menu", "launcher_home",
         "launcher_settings", "launcher_a11y", "port_menu", "port_menu_a11y", "direct_hub"]
# drive.py options a script needs: the launcher's scripts open the launcher,
# with accessibility on but muted, since its line reading needs the switch on,
# as the port menu's does.
ARGS = {"launcher_home": ["--launcher", "--muted"], "launcher_settings": ["--launcher", "--muted"],
        "launcher_a11y": ["--launcher", "--muted"], "port_menu": ["--muted"], "port_menu_a11y": ["--muted"],
        "match_port_menu": ["--muted"], "direct_call": ["--muted"]}
# Settings a script needs from the environment. The Accessibility
# category's scripts leave Steering and Cues to the run's a11y.ini, so their
# rows show no "(environment)" whatever the caller exports.
A11Y_ROWS = {"MELEE_A11Y_STEER": "", "MELEE_A11Y_CUES": ""}
ENV = {"launcher_a11y": A11Y_ROWS, "port_menu_a11y": A11Y_ROWS,
       "results": {"MELEE_DEBUG_VS": "cpu", "MELEE_DEBUG_VS_STOCKS": "1", "MELEE_DEBUG_VS_STAGE": "31"},
       "results_no_contest": {"MELEE_DEBUG_VS": "cpu", "MELEE_DEBUG_VS_STAGE": "31"},
       # Poke Floats, where three of the four CPUs fall out within half a minute.
       "results_cpu4": {"MELEE_DEBUG_VS": "cpu4", "MELEE_DEBUG_VS_STOCKS": "1", "MELEE_DEBUG_VS_STAGE": "23"},
       "match_opponent": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_gap": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_gap_battlefield": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2",
                                 "MELEE_DEBUG_VS_STAGE": "31"},
       "match_ledge": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_ledge_battlefield": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2",
                                   "MELEE_DEBUG_VS_STAGE": "31"},
       "match_port_menu": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2", "MELEE_A11Y_CUES": "1"}}
# The port menu's status line says "Saved." after every change,
# "Settings save automatically." as the menu first opens, and each focus's
# description, which the focus announcement has said (a category's only
# repeats its name); none of them is ever spoken.
PORT_MENU_STATUS = re.compile(r'"(Saved|Settings save automatically|\w+ settings\.")')
NO_LOBBY_SPEECH = re.compile(r'"Online lobby\. No speech yet\.')
# Results' ready lines: with one human, or none, readying ends results and
# says nothing.
READY = r'"(Ready, waiting|Not ready\.|P\d (not )?ready\.)'
# Lines a passing run must not log; drive.py cannot fail on an absent line.
ABSENT = {"sss_skipped": re.compile(r'"Stage select\.'),
          # The Training menu's hidden row, or a value the reader cannot name.
          "training_menu": re.compile(r"\[a11y\] training menu row "),
          "port_menu": PORT_MENU_STATUS, "port_menu_a11y": PORT_MENU_STATUS,
          # The scene's placeholder, which DIRECT CONNECT's own opening replaces.
          "direct_hub": NO_LOBBY_SPEECH, "direct_call": NO_LOBBY_SPEECH,
          "results": re.compile(READY), "results_teams": re.compile(READY),
          # A cancelled match has no places and no Bonuses page.
          "results_no_contest": re.compile(r'\[a11y\] speak .*("Bonuses\.|\b[1-4](st|nd|rd|th)\b)|' + READY),
          # Four CPUs: nobody is "you".
          "results_cpu4": re.compile(r'\[a11y\] speak .*, you\b|' + READY)}
# Lines a passing run must not log between an opening line and the closing
# line after it: no cue starts or stops while the port menu is open, since
# the game stands still then and the stops come just before the opening.
PORT_MENU_CUES = (re.compile(r'\[a11y\] speak \w+.*: "PC settings\. '),
                  re.compile(r'\[a11y\] speak \w+.*: "Back to game"$'),
                  re.compile(r"\[a11y\] cue [a-z ]+ (start|stop) "), "while the port menu was open")
BETWEEN = {"match_port_menu": PORT_MENU_CUES}
# Lines a passing run logs exactly so many times, or a number of times in a
# range, for the same reason: the gap warning starts and stops once, at the
# edge, and never on the floor or the platforms.
GAP_ONCE = [(re.compile(r"\[a11y\] cue gap start "), 1), (re.compile(r"\[a11y\] cue gap stop "), 1)]
# The ledge warning on Final Destination: the left end's once, the right
# end's twice (on the way out and back), the gap warning once between; on
# Battlefield only the main floor's left end, never the platform's.
LEDGE = [(re.compile(r"\[a11y\] cue ledge left start "), 1), (re.compile(r"\[a11y\] cue ledge left stop "), 1),
         (re.compile(r"\[a11y\] cue ledge right start "), 2), (re.compile(r"\[a11y\] cue ledge right stop "), 2)]
LEDGE_BATTLEFIELD = [(re.compile(r"\[a11y\] cue ledge left start "), 1),
                     (re.compile(r"\[a11y\] cue ledge right start "), 0)]
# The Training menu says seven to nine numbers: No. of CPs and CP Damage
# changed and put back, then Right held on CP Damage, which says its first
# value, the game's first repeat and the value it settles on. The game's next
# two repeats come about 0.15 s apart, on the settle, so either may be said
# too; the quicker ones after them never.
TRAINING_NUMBERS = [(re.compile(r'\[a11y\] speak \w+( \(off\))?: "\d+"$'), range(7, 10))]
COUNT = {"match_gap": GAP_ONCE, "match_gap_battlefield": GAP_ONCE, "match_ledge": GAP_ONCE + LEDGE,
         "match_ledge_battlefield": LEDGE_BATTLEFIELD, "training_menu": TRAINING_NUMBERS}
NET = [("css_net-delay0", ["--delay", "0"]), ("css_net-delay10", ["--delay", "10"]),
       ("css_net-slow", ["--sim-delay-ms", "100"])]
# Direct Connect's two-copy runs meet over the internet through the router's
# forward of UDP 42050 and 42051 to this machine (docs/a11y/netplay.md, "Two
# copies on one machine"), so both take those ports and run one at a time.
DIRECT = ["direct_back", "direct_lost"]
FORWARDED_PORTS = threading.Lock()
NET_RUNS = [name for name, _ in NET] + ["results_net", "lan_lobby"] + DIRECT
ONLINE = NET_RUNS + ["direct_call"]
# Failure lines worth repeating in the report.
FAILURE = re.compile(r">> .*(FAILED|warning: no pad change)")


class Run:
    """One drive.py or drive_net.py run: its command, its transcript (a
    file or a directory) and how many cache slots it takes, one a game."""

    def __init__(self, name, cmd, out, slots, env=None, ports=None):
        self.name, self.cmd, self.out, self.slots, self.env = name, cmd, out, slots, env or {}
        # A lock for ports no other run may use at the same time, or None.
        self.ports = ports
        self.seconds = 0.0
        self.code = None
        self.failures = []


def drive_run(name, out_dir):
    out = os.path.join(out_dir, name + ".log")
    script = os.path.join(HERE, name + ".drive")
    if not os.path.isfile(script):
        sys.exit(f"no script {script}")
    return Run(name, [sys.executable, os.path.join(HERE, "drive.py"), *ARGS.get(name, []), script, "--out", out],
               out, 1, ENV.get(name))


def net_run(name, out_dir, base_b):
    """The online run called name, which the online group holds."""
    extra = ["--base-b", base_b] if base_b else []
    out = os.path.join(out_dir, name)
    if name == "lan_lobby":
        # The LAN lobby finds only copies in a LAN lobby, so the sessions
        # started at boot beside it go unseen. Its ports are its own, the
        # drivers' default being the Direct Connect runs'.
        return Run(name, [sys.executable, os.path.join(HERE, "drive_net.py"), "--lan", *extra,
                          "--port", "42150", "--out-dir", out, os.path.join(HERE, "lan_lobby.drive")],
                   out, 2)
    if name == "results_net":
        # Muted, so speech is on and quick chat leaves the D-pad to the
        # reading cursor, as for a player.
        return Run(name, [sys.executable, os.path.join(HERE, "drive_net.py"), "--muted", *extra,
                          "--port", "42160", "--key", "a11ylevelresults", "--out-dir", out,
                          os.path.join(HERE, "results_net.drive")], out, 2)
    if name in DIRECT:
        return Run(name, [sys.executable, os.path.join(HERE, "drive_net.py"), "--direct", "--muted", *extra,
                          "--out-dir", out, os.path.join(HERE, name + ".drive")], out, 2,
                   ports=FORWARDED_PORTS)
    i = [n for n, _ in NET].index(name)
    return Run(name, [sys.executable, os.path.join(HERE, "drive_net.py"), *NET[i][1], *extra,
                      "--port", str(42110 + 10 * i), "--key", f"a11ylevel{i}", "--out-dir", out,
                      os.path.join(HERE, "css_net.drive")], out, 2)


def named_run(name, out_dir, base_b):
    if name in NET_RUNS:
        return net_run(name, out_dir, base_b)
    if name == "css_net":
        sys.exit("css_net takes two games; name one of its runs: " + ", ".join(n for n, _ in NET))
    return drive_run(name, out_dir)


class Slots:
    """The cache slots 1..jobs, handed out to runs as they start."""

    def __init__(self, jobs):
        self.free = [str(n) for n in range(1, jobs + 1)]
        self.cond = threading.Condition()

    def take(self, count):
        with self.cond:
            self.cond.wait_for(lambda: len(self.free) >= count)
            taken, self.free = self.free[:count], self.free[count:]
            return taken

    def give(self, taken):
        with self.cond:
            self.free = sorted(self.free + taken, key=int)
            self.cond.notify_all()


def execute(run, slots, lock):
    # The ports before the slots, so a run waiting for them holds no slots.
    with run.ports or contextlib.nullcontext():
        execute_held(run, slots, lock)


def execute_held(run, slots, lock):
    taken = slots.take(run.slots)
    cmd = run.cmd + (["--cache-slots", ",".join(taken)] if run.slots == 2 else ["--cache-slot", taken[0]])
    env = dict(os.environ, **run.env)
    started = time.monotonic()
    try:
        proc = subprocess.run(cmd, cwd=REPO, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
    finally:
        slots.give(taken)
    run.seconds = time.monotonic() - started
    run.code = proc.returncode
    lines = proc.stdout.decode("utf-8", "replace").splitlines()
    run.failures = [line for line in lines if FAILURE.search(line)]
    absent = ABSENT.get(run.name)
    counts = COUNT.get(run.name, [])
    between = BETWEEN.get(run.name)
    logged_lines = []
    if absent or counts or between:
        try:
            with open(run.out, encoding="utf-8", errors="replace") as f:
                logged_lines = [line.rstrip("\n") for line in f]
        except OSError as e:
            run.code = run.code or 1
            run.failures.append(f">> FAILED: transcript unreadable: {e}")
    if absent:
        found = [line for line in logged_lines if absent.search(line)]
        if found:
            run.code = run.code or 1
            run.failures += [f">> FAILED: logged {absent.pattern!r}: {line}" for line in found]
    if between:
        opening, closing, banned, when = between
        inside = False
        for line in logged_lines:
            if opening.search(line):
                inside = True
            elif closing.search(line):
                inside = False
            elif inside and banned.search(line):
                run.code = run.code or 1
                run.failures.append(f">> FAILED: logged {banned.pattern!r} {when}: {line}")
    for pattern, times in counts:
        logged = sum(1 for line in logged_lines if pattern.search(line))
        if logged not in (times if isinstance(times, range) else (times,)):
            run.code = run.code or 1
            expected = f"{times.start} to {times.stop - 1}" if isinstance(times, range) else times
            run.failures.append(f">> FAILED: logged {pattern.pattern!r} {logged} times, not {expected}")
    with lock:
        print(f"{'pass' if run.code == 0 else 'FAIL'}  {run.name:22} {run.seconds:5.1f} s  {run.out}", flush=True)


def run_set(runs, jobs):
    slots, lock = Slots(jobs), threading.Lock()
    threads = []
    for run in runs:
        t = threading.Thread(target=execute, args=(run, slots, lock))
        t.start()
        threads.append(t)
        time.sleep(0.05)
    for t in threads:
        t.join()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what", nargs="+", help="menus, online, online run names or script names")
    ap.add_argument("--jobs", type=int, default=8, help="games running at once (default 8)")
    ap.add_argument("--base-b", metavar="BUILD_DIR", help="online runs: instance b from this base port build")
    ap.add_argument("--out-dir", default=os.path.join(REPO, "build", "drive", "level"),
                    help="where transcripts go (default build/drive/level)")
    args = ap.parse_args()
    if sys.platform != "win32":
        ap.error("Windows only")
    if args.jobs < 2:
        ap.error("--jobs must be at least 2, the two games of an online run")
    os.makedirs(args.out_dir, exist_ok=True)

    sets, names = [], []
    for what in args.what:
        if what in ("menus", "online"):
            if names:
                sets.append([named_run(n, args.out_dir, args.base_b) for n in names])
                names = []
            if what == "menus":
                sets.append([drive_run(n, args.out_dir) for n in MENUS])
            else:
                online = [n for n in ONLINE if not (args.base_b and n in DIRECT)]
                if args.base_b:
                    print(f"left out with --base-b: {', '.join(DIRECT)} (the base port build would write "
                          "the maintainer's recent opponents)", flush=True)
                sets.append([named_run(n, args.out_dir, args.base_b) for n in online])
        else:
            names.append(what)
    if names:
        sets.append([named_run(n, args.out_dir, args.base_b) for n in names])

    started = time.monotonic()
    every = []
    for runs in sets:
        run_set(runs, args.jobs)
        every += runs
    failed = [run for run in every if run.code != 0]
    print(f"{len(every) - len(failed)} of {len(every)} passed in {time.monotonic() - started:.0f} s")
    for run in failed:
        print(f"\n{run.name} (exit {run.code}), {run.out}:")
        for line in run.failures or ["no failure line; see the transcript"]:
            print(f"  {line}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
