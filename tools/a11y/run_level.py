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
side by side, the longest first by their last passing times, kept in
times.json beside the transcripts:

  menus   the menu drive scripts (MENUS), the launcher's four and the
          port menu's three among them; the match announcements' scripts
          run by name
  online  css_net.drive at delay 0, delay 10 and over a 100 ms slow link,
          results_net.drive and match_net.drive (--muted), lan_lobby.drive
          (--lan), direct_call.drive, and direct_back.drive and
          direct_lost.drive (--direct), the last two one after the other
          since both use the ports the router forwards (with --base-b,
          drive_net.py sets the maintainer's recent opponents aside
          around each, and direct_call, the fork's game alone, is left
          out)
  NAME    an online run by its name (css_net-delay0, css_net-delay10,
          css_net-slow, results_net, match_net, lan_lobby, direct_back,
          direct_lost), else tools/a11y/NAME.drive through drive.py; the
          names given in a row run side by side

  python tools/a11y/run_level.py lan_lobby --base-b ../melee-pc-base/build

--jobs caps how many games run at once (default 8); a drive_net.py run
counts two. The environment passes through, so MELEE_A11Y_CUES=1 in front
runs every script with cues on.

Transcripts go to build/drive/level: NAME.log for drive.py, a directory
NAME for drive_net.py.
"""
import argparse
import json
import math
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
         "launcher_settings", "launcher_a11y", "launcher_voices", "port_menu", "port_menu_a11y",
         "port_menu_voices", "direct_hub"]
# drive.py options a script needs: the launcher's scripts open the launcher,
# with accessibility on but muted, since its line reading needs the switch on,
# as the port menu's does.
ARGS = {"launcher_home": ["--launcher", "--muted"], "launcher_settings": ["--launcher", "--muted"],
        "launcher_a11y": ["--launcher", "--muted"], "launcher_voices": ["--launcher", "--muted"],
        "port_menu": ["--muted"], "port_menu_a11y": ["--muted"],
        "port_menu_voices": ["--muted"], "match_port_menu": ["--muted"], "direct_call": ["--muted"],
        "match_damage": ["--muted"], "match_thresholds": ["--muted"], "match_stocks": ["--muted"],
        "match_stock_steal": ["--muted"], "match_time": ["--muted"]}
# Settings a script needs from the environment. The Accessibility
# category's scripts leave Steering and Cues to the run's a11y.ini, so their
# rows show no "(environment)" whatever the caller exports.
A11Y_ROWS = {"MELEE_A11Y_STEER": "", "MELEE_A11Y_CUES": ""}
ENV = {"launcher_a11y": A11Y_ROWS, "launcher_voices": A11Y_ROWS, "port_menu_a11y": A11Y_ROWS,
       "port_menu_voices": A11Y_ROWS,
       "results": {"MELEE_DEBUG_VS": "cpu", "MELEE_DEBUG_VS_STOCKS": "1", "MELEE_DEBUG_VS_STAGE": "31"},
       "results_no_contest": {"MELEE_DEBUG_VS": "cpu", "MELEE_DEBUG_VS_STAGE": "31"},
       # Poke Floats, where three of the four CPUs fall out within half a minute.
       "results_cpu4": {"MELEE_DEBUG_VS": "cpu4", "MELEE_DEBUG_VS_STOCKS": "1", "MELEE_DEBUG_VS_STAGE": "23"},
       "match_damage": {"MELEE_DEBUG_VS": "cpu", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_thresholds": {"MELEE_DEBUG_VS": "cpu", "MELEE_DEBUG_VS_STOCKS": "2"},
       "match_stocks": {"MELEE_DEBUG_VS": "1", "MELEE_DEBUG_VS_STOCKS": "3"},
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
          "port_menu_voices": PORT_MENU_STATUS,
          # The scene's placeholder, which DIRECT CONNECT's own opening replaces.
          "direct_hub": NO_LOBBY_SPEECH, "direct_call": NO_LOBBY_SPEECH,
          "results": re.compile(READY), "results_teams": re.compile(READY),
          # A count of 0 is the game's announcer's to call.
          "match_stock_steal": re.compile(r'\[a11y\] voice \w+( \(off\))?: "([^"]*, )?0 stocks"'),
          # A cancelled match has no places and no Bonuses page.
          "results_no_contest": re.compile(r'\[a11y\] speak .*("Bonuses\.|\b[1-4](st|nd|rd|th)\b)|' + READY),
          # Four CPUs: nobody is "you", in speech or in your voice.
          "results_cpu4": re.compile(r'\[a11y\] speak .*, you\b|\[a11y\] voice yours\b|' + READY),
          # A one-minute match: its starting minute is never said.
          "match_time": re.compile(r'"\d+ minutes? left"')}
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
# The voices' samples: one for each settled run of steps on a Voice, Rate or
# Volume row and one for each Preview, the port menu's quick run of two steps
# Left among them.
YOUR_SAMPLE = re.compile(r'\[a11y\] voice yours: "Your voice"$')
THEIR_SAMPLE = re.compile(r"\[a11y\] voice opponents: \"Opponents' voice\"$")
VOICE_SAMPLES = [(YOUR_SAMPLE, 6), (THEIR_SAMPLE, 2)]
LAUNCHER_VOICE_SAMPLES = [(YOUR_SAMPLE, 5), (THEIR_SAMPLE, 1)]
COUNT = {"match_gap": GAP_ONCE, "match_gap_battlefield": GAP_ONCE, "match_ledge": GAP_ONCE + LEDGE,
         "match_ledge_battlefield": LEDGE_BATTLEFIELD, "training_menu": TRAINING_NUMBERS,
         "port_menu_voices": VOICE_SAMPLES, "launcher_voices": LAUNCHER_VOICE_SAMPLES}


def threshold_crossings(lines, out):
    """Failure lines unless each damage figure said in a voice crossed a
    multiple of the damage interval, read from the run's a11y.ini, since the
    last one said in that voice. A stock line says a KO, which counts the
    next crossing from 0, as does a figure below the last said (a KO with
    stocks off, or a heal)."""
    try:
        with open(out + ".settings/a11y.ini", encoding="utf-8") as f:
            interval = int(re.search(r"^match_announcements\.damage_interval = (\d+)$", f.read(), re.M)[1])
    except (OSError, TypeError) as e:
        return [f">> FAILED: no damage interval in the run's a11y.ini: {e}"]
    failures, said = [], {}
    for line in lines:
        stock = re.search(r'\[a11y\] voice (\w+): "(\d+ stocks|last stock)"$', line)
        if stock:
            said[stock[1]] = 0
            continue
        m = re.search(r'\[a11y\] voice (\w+): "(\d+)"$', line)
        if not m:
            continue
        voice, figure = m[1], int(m[2])
        last = said.get(voice, 0)
        if figure < last:
            last = 0
        if figure // interval <= last // interval:
            failures.append(f">> FAILED: {voice} said {figure} after {last}, crossing no multiple of {interval}")
        said[voice] = figure
    return failures


# Checks of a transcript, each giving its failure lines: Thresholds says a
# figure only once it has crossed a multiple of the interval.
CHECK = {"match_thresholds": threshold_crossings}
NET = [("css_net-delay0", ["--delay", "0"]), ("css_net-delay10", ["--delay", "10"]),
       ("css_net-slow", ["--sim-delay-ms", "100"])]
# Direct Connect's two-copy runs meet over the internet through the router's
# forward of UDP 42050 and 42051 to this machine (docs/a11y/netplay.md, "Two
# copies on one machine"), so both take those ports and run one at a time.
DIRECT = ["direct_back", "direct_lost"]
FORWARDED_PORTS = "forwarded"
NET_RUNS = [name for name, _ in NET] + ["results_net", "match_net", "lan_lobby"] + DIRECT
ONLINE = NET_RUNS + ["direct_call"]
# Each run's time the last time it passed, kept beside the transcripts, so a
# set starts its longest runs first and ends on short ones.
TIMES = "times.json"
# Failure lines worth repeating in the report.
FAILURE = re.compile(r">> .*(FAILED|warning: no pad change)")


class Run:
    """One drive.py or drive_net.py run: its command, its transcript (a
    file or a directory) and how many cache slots it takes, one a game."""

    def __init__(self, name, cmd, out, slots, env=None, ports=None):
        self.name, self.cmd, self.out, self.slots, self.env = name, cmd, out, slots, env or {}
        # Ports no other run may use at the same time, or None.
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
    if name == "match_net":
        # Muted, so the voices render and log their "plays" lines.
        return Run(name, [sys.executable, os.path.join(HERE, "drive_net.py"), "--muted", *extra,
                          "--port", "42170", "--key", "a11ylevelmatch", "--out-dir", out,
                          os.path.join(HERE, "match_net.drive")], out, 2)
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


def load_times(out_dir):
    try:
        with open(os.path.join(out_dir, TIMES), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_times(out_dir, times, runs):
    times.update({run.name: round(run.seconds, 1) for run in runs if run.code == 0})
    with open(os.path.join(out_dir, TIMES), "w", encoding="utf-8") as f:
        json.dump(times, f, indent=1, sort_keys=True)


def execute(run, taken, lock):
    cmd = run.cmd + (["--cache-slots", ",".join(taken)] if run.slots == 2 else ["--cache-slot", taken[0]])
    env = dict(os.environ, **run.env)
    started = time.monotonic()
    proc = subprocess.run(cmd, cwd=REPO, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
    run.seconds = time.monotonic() - started
    run.code = proc.returncode
    lines = proc.stdout.decode("utf-8", "replace").splitlines()
    run.failures = [line for line in lines if FAILURE.search(line)]
    absent = ABSENT.get(run.name)
    counts = COUNT.get(run.name, [])
    between = BETWEEN.get(run.name)
    check = CHECK.get(run.name)
    logged_lines = []
    if absent or counts or between or check:
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
    if check:
        found = check(logged_lines, run.out)
        if found:
            run.code = run.code or 1
            run.failures += found
    for pattern, times in counts:
        logged = sum(1 for line in logged_lines if pattern.search(line))
        if logged not in (times if isinstance(times, range) else (times,)):
            run.code = run.code or 1
            expected = f"{times.start} to {times.stop - 1}" if isinstance(times, range) else times
            run.failures.append(f">> FAILED: logged {pattern.pattern!r} {logged} times, not {expected}")
    with lock:
        print(f"{'pass' if run.code == 0 else 'FAIL'}  {run.name:22} {run.seconds:5.1f} s  {run.out}", flush=True)


def run_set(runs, jobs, times):
    """Start each run once enough cache slots (1..jobs) and its ports are
    free: the longest by its last passing time first, those never timed
    before them in their given order, and a shorter run in a gap too small
    for the next longer one."""
    waiting = sorted(runs, key=lambda run: -times.get(run.name, math.inf))
    free = [str(n) for n in range(1, jobs + 1)]
    held = set()
    cond = threading.Condition()

    def startable():
        return next((run for run in waiting if run.slots <= len(free) and run.ports not in held), None)

    def execute_and_free(run, taken):
        try:
            execute(run, taken, cond)
        finally:
            with cond:
                free.extend(taken)
                free.sort(key=int)
                held.discard(run.ports)
                cond.notify_all()

    threads = []
    while waiting:
        with cond:
            run = cond.wait_for(startable)
            waiting.remove(run)
            taken = free[:run.slots]
            del free[:run.slots]
            if run.ports:
                held.add(run.ports)
        t = threading.Thread(target=execute_and_free, args=(run, taken))
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
                if args.base_b:
                    print("left out with --base-b: direct_call (one game, the fork's, as in the online "
                          "level)", flush=True)
                online = [n for n in ONLINE if not (args.base_b and n == "direct_call")]
                sets.append([named_run(n, args.out_dir, args.base_b) for n in online])
        else:
            names.append(what)
    if names:
        sets.append([named_run(n, args.out_dir, args.base_b) for n in names])

    started = time.monotonic()
    times = load_times(args.out_dir)
    every = []
    for runs in sets:
        run_set(runs, args.jobs, times)
        every += runs
    save_times(args.out_dir, times, every)
    failed = [run for run in every if run.code != 0]
    print(f"{len(every) - len(failed)} of {len(every)} passed in {time.monotonic() - started:.0f} s")
    for run in failed:
        print(f"\n{run.name} (exit {run.code}), {run.out}:")
        for line in run.failures or ["no failure line; see the transcript"]:
            print(f"  {line}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
