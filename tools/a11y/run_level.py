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

  menus   the sixteen menu drive scripts
  online  css_net.drive at delay 0, delay 10 and over a 100 ms slow link,
          and lan_lobby.drive (--lan)
  NAME    an online run by its name (css_net-delay0, css_net-delay10,
          css_net-slow, lan_lobby), else tools/a11y/NAME.drive through
          drive.py; the names given in a row run side by side

  python tools/a11y/run_level.py lan_lobby --base-b ../melee-pc-base/build

--jobs caps how many games run at once (default 8); a drive_net.py run
counts two. The environment passes through, so MELEE_A11Y_CUES=1 in front
runs every script with cues on.

Transcripts go to build/drive/level: NAME.log for drive.py, a directory
NAME for drive_net.py.
"""
import argparse
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
         "css_slots", "css_portraits", "css_steps", "results"]
# Settings a script needs from the environment.
ENV = {"results": {"MELEE_DEBUG_VS": "cpu", "MELEE_DEBUG_VS_STOCKS": "1", "MELEE_DEBUG_VS_STAGE": "31"},
       "match_opponent": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_gap": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_gap_battlefield": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2",
                                 "MELEE_DEBUG_VS_STAGE": "31"},
       "match_ledge": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_ledge_battlefield": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "2",
                                   "MELEE_DEBUG_VS_STAGE": "31"}}
# Lines a passing run must not log; drive.py cannot fail on an absent line.
ABSENT = {"sss_skipped": re.compile(r'"Stage select\.')}
# Lines a passing run logs exactly so many times, for the same reason: the
# gap warning starts and stops once, at the edge, and never on the floor or
# the platforms.
GAP_ONCE = [(re.compile(r"\[a11y\] cue gap start "), 1), (re.compile(r"\[a11y\] cue gap stop "), 1)]
# The ledge warning on Final Destination: the left end's once, the right
# end's twice (on the way out and back), the gap warning once between; on
# Battlefield only the main floor's left end, never the platform's.
LEDGE = [(re.compile(r"\[a11y\] cue ledge left start "), 1), (re.compile(r"\[a11y\] cue ledge left stop "), 1),
         (re.compile(r"\[a11y\] cue ledge right start "), 2), (re.compile(r"\[a11y\] cue ledge right stop "), 2)]
LEDGE_BATTLEFIELD = [(re.compile(r"\[a11y\] cue ledge left start "), 1),
                     (re.compile(r"\[a11y\] cue ledge right start "), 0)]
COUNT = {"match_gap": GAP_ONCE, "match_gap_battlefield": GAP_ONCE, "match_ledge": GAP_ONCE + LEDGE,
         "match_ledge_battlefield": LEDGE_BATTLEFIELD}
NET = [("css_net-delay0", ["--delay", "0"]), ("css_net-delay10", ["--delay", "10"]),
       ("css_net-slow", ["--sim-delay-ms", "100"])]
ONLINE = [name for name, _ in NET] + ["lan_lobby"]
# Failure lines worth repeating in the report.
FAILURE = re.compile(r">> .*(FAILED|warning: no pad change)")


class Run:
    """One drive.py or drive_net.py run: its command, its transcript (a
    file or a directory) and how many cache slots it takes, one a game."""

    def __init__(self, name, cmd, out, slots, env=None):
        self.name, self.cmd, self.out, self.slots, self.env = name, cmd, out, slots, env or {}
        self.seconds = 0.0
        self.code = None
        self.failures = []


def drive_run(name, out_dir):
    out = os.path.join(out_dir, name + ".log")
    script = os.path.join(HERE, name + ".drive")
    if not os.path.isfile(script):
        sys.exit(f"no script {script}")
    return Run(name, [sys.executable, os.path.join(HERE, "drive.py"), script, "--out", out], out, 1,
               ENV.get(name))


def net_run(name, out_dir, base_b):
    """The online run called name, which the online group holds."""
    extra = ["--base-b", base_b] if base_b else []
    out = os.path.join(out_dir, name)
    if name == "lan_lobby":
        # The LAN lobby finds only copies in a LAN lobby, so the sessions
        # started at boot beside it go unseen.
        return Run(name, [sys.executable, os.path.join(HERE, "drive_net.py"), "--lan", *extra,
                          "--out-dir", out, os.path.join(HERE, "lan_lobby.drive")], out, 2)
    i = [n for n, _ in NET].index(name)
    return Run(name, [sys.executable, os.path.join(HERE, "drive_net.py"), *NET[i][1], *extra,
                      "--port", str(42110 + 10 * i), "--key", f"a11ylevel{i}", "--out-dir", out,
                      os.path.join(HERE, "css_net.drive")], out, 2)


def named_run(name, out_dir, base_b):
    if name in ONLINE:
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
    logged_lines = []
    if absent or counts:
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
    for pattern, times in counts:
        logged = sum(1 for line in logged_lines if pattern.search(line))
        if logged != times:
            run.code = run.code or 1
            run.failures.append(f">> FAILED: logged {pattern.pattern!r} {logged} times, not {times}")
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
            sets.append([drive_run(n, args.out_dir) for n in MENUS] if what == "menus"
                        else [net_run(n, args.out_dir, args.base_b) for n in ONLINE])
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
