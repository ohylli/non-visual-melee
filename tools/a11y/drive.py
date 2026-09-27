#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
r"""Drive a game run with scripted controller input and print what it logged.

Windows only. Launches build/melee.exe without taking focus, feeds it input
through the base port's MELEE_KEY_FIFO test driver (src/pc/keyboard.c) over a
named pipe, and prints the [a11y] and "boot scene" lines interleaved with the
script's steps. The whole output (stdout and stderr) is saved to --out.

  python tools/a11y/drive.py [options] SCRIPT_FILE
  python tools/a11y/drive.py [options] -c "press Start; wait 'scene 1'"

Script steps, one per line (or ';'-separated with -c), '#' starts a comment:

  press BUTTON[+BUTTON...] [MS]  hold the buttons MS milliseconds (default 120)
                                 and wait until the game's pad shows them
  press ... until RE [TIMES]     press again every 1.5 s until an output line
                                 matches RE, at most TIMES (default 20)
  wait RE [SECONDS]              wait for an output line matching RE,
                                 newer than the last press or wait (default 30)
  sleep SECONDS
  shot PATH                      save the game window as a PNG
  quit                           close the window and wait for a clean exit

RE is a Python regular expression searched in each line: escape brackets
(\[a11y\]) and anchor with $ where a number could run on ("scene 1$").

Buttons are GameCube names, as in tools/devctl.py: A B X Y Z L R Start,
Up Down Left Right (main stick), CUp CDown CLeft CRight, DUp DDown DLeft
DRight. They drive controller port 1, merged with any real controller there.

The run ends with an implicit quit. A failed wait quits too and exits 1.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import queue
import re
import shlex
import struct
import subprocess
import sys
import threading
import time
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Logical GC button -> SDL scancode name, per the keymap in src/pc/keyboard.c.
BUTTONS = {
    "a": "X", "b": "Z", "x": "C", "y": "V", "z": "Tab", "l": "Q", "r": "E",
    "start": "Return",
    "up": "Up", "down": "Down", "left": "Left", "right": "Right",
    "cup": "I", "cdown": "K", "cleft": "J", "cright": "L",
    "dup": "T", "ddown": "G", "dleft": "F", "dright": "H",
}

DEFAULT_SHOW = ("[a11y]", "boot scene")
UNTIL_SETTLE = 1.5  # seconds a "press ... until" waits for its text before pressing again
PAD_RE = re.compile(r"pad: btn ([0-9a-f]+) stick (-?\d+),(-?\d+) sub (-?\d+),(-?\d+) trig (\d+),(\d+)")

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32.CreateNamedPipeW.restype = wt.HANDLE
kernel32.CreateNamedPipeW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD,
                                      wt.DWORD, wt.DWORD, wt.LPVOID]
kernel32.ConnectNamedPipe.argtypes = [wt.HANDLE, wt.LPVOID]
kernel32.WriteFile.argtypes = [wt.HANDLE, wt.LPCVOID, wt.DWORD, ctypes.POINTER(wt.DWORD), wt.LPVOID]
kernel32.CloseHandle.argtypes = [wt.HANDLE]
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
for fn, res, argtypes in (
    (user32.GetDC, wt.HDC, [wt.HWND]),
    (user32.ReleaseDC, ctypes.c_int, [wt.HWND, wt.HDC]),
    (user32.PrintWindow, wt.BOOL, [wt.HWND, wt.HDC, wt.UINT]),
    (user32.PostMessageW, wt.BOOL, [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]),
    (gdi32.CreateCompatibleDC, wt.HDC, [wt.HDC]),
    (gdi32.CreateCompatibleBitmap, wt.HBITMAP, [wt.HDC, ctypes.c_int, ctypes.c_int]),
    (gdi32.SelectObject, wt.HGDIOBJ, [wt.HDC, wt.HGDIOBJ]),
    (gdi32.DeleteObject, wt.BOOL, [wt.HGDIOBJ]),
    (gdi32.DeleteDC, wt.BOOL, [wt.HDC]),
    (gdi32.GetDIBits, ctypes.c_int, [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT, wt.LPVOID, wt.LPVOID, wt.UINT]),
):
    fn.restype, fn.argtypes = res, argtypes
INVALID_HANDLE_VALUE = wt.HANDLE(-1).value
PW_CLIENTONLY = 0x1
PW_RENDERFULLCONTENT = 0x2


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                ("biBitCount", wt.WORD), ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                ("biClrImportant", wt.DWORD)]
PIPE_ACCESS_OUTBOUND = 0x2
ERROR_PIPE_CONNECTED = 535
WM_CLOSE = 0x0010


class ScriptError(Exception):
    pass


class KeyPipe:
    """The server end of the pipe the game's fifo thread reads lines from."""

    def __init__(self, name):
        self.name = name
        self.handle = kernel32.CreateNamedPipeW(name, PIPE_ACCESS_OUTBOUND, 0, 1, 4096, 4096, 0, None)
        if self.handle == INVALID_HANDLE_VALUE:
            raise OSError(ctypes.get_last_error(), "CreateNamedPipeW failed")
        self.connected = threading.Event()
        threading.Thread(target=self._connect, daemon=True).start()

    def _connect(self):
        if kernel32.ConnectNamedPipe(self.handle, None) or ctypes.get_last_error() == ERROR_PIPE_CONNECTED:
            self.connected.set()

    def send(self, line, timeout):
        if not self.connected.wait(timeout):
            raise ScriptError("the game never opened the key pipe")
        data = (line + "\n").encode()
        written = wt.DWORD()
        if not kernel32.WriteFile(self.handle, data, len(data), ctypes.byref(written), None):
            raise ScriptError(f"writing to the key pipe failed ({ctypes.get_last_error()})")

    def close(self):
        kernel32.CloseHandle(self.handle)


class Output:
    """Every stdout and stderr line of the game, in arrival order."""

    def __init__(self, proc, out_file, show):
        self.lines = []
        self.cond = threading.Condition()
        self.out_file = out_file
        self.show = show
        self.printed = queue.Queue()
        for stream in (proc.stdout, proc.stderr):
            threading.Thread(target=self._read, args=(stream,), daemon=True).start()

    def _read(self, stream):
        for raw in stream:
            line = raw.decode("utf-8", "replace").rstrip("\r\n")
            with self.cond:
                self.lines.append(line)
                self.out_file.write(line + "\n")
                self.out_file.flush()
                self.cond.notify_all()
            if self.show is None or any(s in line for s in self.show):
                print(line, flush=True)

    def mark(self):
        with self.cond:
            return len(self.lines)

    def wait_for(self, start, match, timeout):
        """Index of the first line at or after start that match() accepts."""
        deadline = time.monotonic() + timeout
        i = start
        with self.cond:
            while True:
                while i < len(self.lines):
                    if match(self.lines[i]):
                        return i
                    i += 1
                left = deadline - time.monotonic()
                if left <= 0:
                    return None
                self.cond.wait(left)


def parse_steps(text, inline):
    steps = []
    for n, line in enumerate((text.replace(";", "\n") if inline else text).splitlines(), 1):
        try:
            words = shlex.split(line, comments=True)
        except ValueError as e:
            raise ScriptError(f"step {n}: {e}")
        if words:
            steps.append(words)
    return steps


def keys_for(spec):
    keys = []
    for name in spec.split("+"):
        key = BUTTONS.get(name.lower())
        if key is None:
            raise ScriptError(f"unknown button {name!r}; known: {' '.join(sorted(BUTTONS))}")
        keys.append(key)
    return "+".join(keys)


def parse_press(rest):
    """press BUTTONS [MS] [until RE [TIMES]] -> (keys, hold, until, times)"""
    keys = keys_for(rest[0])
    rest = rest[1:]
    hold = 120
    if rest and rest[0].lower() != "until":
        hold = int(rest.pop(0))
    until, times = None, 1
    if rest and rest[0].lower() == "until" and len(rest) in (2, 3):
        until = pattern(rest[1])
        times = int(rest[2]) if len(rest) == 3 else 20
    elif rest:
        raise ScriptError(f"bad press arguments: {' '.join(rest)}")
    return keys, hold, until, times


def pattern(text):
    try:
        return re.compile(text)
    except re.error as e:
        raise ScriptError(f"bad pattern {text!r}: {e}")


def pad_pressed(line):
    m = PAD_RE.search(line)
    return m is not None and any(int(v, 16 if i == 0 else 10) != 0 for i, v in enumerate(m.groups()))


def close_windows(pid):
    """Post WM_CLOSE to the game's windows: a clean exit that takes no focus."""
    found = game_windows(pid)
    for hwnd in found:
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    return bool(found)


def game_windows(pid):
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def each(hwnd, _):
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(each, 0)
    return found


def screenshot(pid, path):
    """Save the game window's client area as a PNG. PrintWindow asks the
    window to render itself, so this works behind other windows and needs
    no focus."""
    windows = game_windows(pid)
    if not windows:
        raise ScriptError("no game window to capture")
    hwnd = windows[0]
    rect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    w, h = rect.right, rect.bottom
    if w <= 0 or h <= 0:
        raise ScriptError("the game window has no client area (minimized?)")
    hdc_win = user32.GetDC(hwnd)
    hdc = gdi32.CreateCompatibleDC(hdc_win)
    bmp = gdi32.CreateCompatibleBitmap(hdc_win, w, h)
    old = gdi32.SelectObject(hdc, bmp)
    try:
        if not user32.PrintWindow(hwnd, hdc, PW_CLIENTONLY | PW_RENDERFULLCONTENT):
            raise ScriptError("PrintWindow failed")
        info = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        buf = ctypes.create_string_buffer(w * h * 4)
        if gdi32.GetDIBits(hdc, bmp, 0, h, buf, ctypes.byref(info), 0) != h:
            raise ScriptError("GetDIBits failed")
    finally:
        gdi32.SelectObject(hdc, old)
        gdi32.DeleteObject(bmp)
        gdi32.DeleteDC(hdc)
        user32.ReleaseDC(hwnd, hdc_win)
    bgra = buf.raw
    rows = bytearray()
    for y in range(h):
        row = bgra[y * w * 4:(y + 1) * w * 4]
        rgb = bytearray(w * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
        rows += b"\0" + rgb
    write_png(path, w, h, bytes(rows))


def write_png(path, w, h, filtered_rows):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(filtered_rows, 6)))
        f.write(chunk(b"IEND", b""))


def quit_game(proc, timeout=15):
    if proc.poll() is not None:
        return proc.returncode
    # Resend while waiting: a quit early in startup can come before the window exists.
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        close_windows(proc.pid)
        try:
            return proc.wait(0.5)
        except subprocess.TimeoutExpired:
            pass
    print(">> game did not exit after WM_CLOSE; killing it", flush=True)
    proc.kill()
    return proc.wait()


def run(args, steps):
    env = dict(os.environ)
    pipe_name = rf"\\.\pipe\melee-drive-{os.getpid()}"
    env.update({
        "SDL_WINDOW_ACTIVATE_WHEN_SHOWN": "0",
        "SDL_AUDIO_DRIVER": "dummy",
        "MELEE_KEY_FIFO": pipe_name,
        "MELEE_INPUT_TRACE": "1",
        "MELEE_BOOT_SCENE": args.scene,
        "MELEE_NO_ATTRACT": "1",
        "MELEE_EXIT_AFTER_FRAMES": str(args.max_frames),
        "MELEE_LOG_FILE": os.path.abspath(args.out) + ".game.log",
    })
    if not args.speech:
        env["MELEE_A11Y"] = "0"
    exe = os.path.join(REPO, "build", "melee.exe")
    disc = os.path.join(REPO, args.disc)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    pipe = KeyPipe(pipe_name)
    with open(args.out, "w", encoding="utf-8") as out_file:
        proc = subprocess.Popen([exe, "--no-card", disc], cwd=REPO, env=env,
                                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = Output(proc, out_file, None if args.all else DEFAULT_SHOW)
        cursor = 0
        failed = False

        def step(msg):
            print(f">> {msg}", flush=True)
            out_file.write(f">> {msg}\n")

        try:
            for words in steps:
                if proc.poll() is not None:
                    raise ScriptError(f"the game exited early with code {proc.returncode}")
                cmd, rest = words[0].lower(), words[1:]
                step(" ".join(words))
                if cmd == "press" and rest:
                    keys, hold, until, times = parse_press(rest)
                    cursor = output.mark()
                    for n in range(1, times + 1):
                        pipe.send(f"{keys} {hold}", args.timeout)
                        if output.wait_for(output.mark(), pad_pressed, 5 + hold / 1000) is None:
                            step("warning: no pad change seen for that press")
                        if until is None:
                            break
                        i = output.wait_for(cursor, until.search, UNTIL_SETTLE)
                        if i is not None:
                            step(f"{until.pattern!r} after {n} press(es)")
                            cursor = i + 1
                            break
                    else:
                        raise ScriptError(f"no {until.pattern!r} after {times} presses")
                elif cmd == "wait" and len(rest) in (1, 2):
                    timeout = float(rest[1]) if len(rest) == 2 else args.timeout
                    i = output.wait_for(cursor, pattern(rest[0]).search, timeout)
                    if i is None:
                        raise ScriptError(f"timed out after {timeout:g} s waiting for {rest[0]!r}")
                    cursor = i + 1
                elif cmd == "sleep" and len(rest) == 1:
                    time.sleep(float(rest[0]))
                elif cmd == "shot" and len(rest) == 1:
                    screenshot(proc.pid, rest[0])
                elif cmd == "quit" and not rest:
                    break
                else:
                    raise ScriptError(f"bad step: {' '.join(words)}")
        except ScriptError as e:
            step(f"FAILED: {e}")
            failed = True
        finally:
            code = quit_game(proc)
            pipe.close()
            time.sleep(0.2)  # let the reader threads drain the last lines
            step(f"game exited with code {code}")
    return 1 if failed or code != 0 else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("script", nargs="?", help="script file ('-' for stdin)")
    ap.add_argument("-c", dest="inline", help="script text instead of a file")
    ap.add_argument("--scene", default="title", help="MELEE_BOOT_SCENE (default title)")
    ap.add_argument("--disc", default="melee.iso", help="disc image, relative to the repo root")
    ap.add_argument("--out", default=os.path.join(REPO, "build", "drive", "last.log"),
                    help="full output transcript (default build/drive/last.log)")
    ap.add_argument("--speech", action="store_true", help="speak through the screen reader (off by default)")
    ap.add_argument("--all", action="store_true", help="print every game line, not just [a11y] and boot scene")
    ap.add_argument("--timeout", type=float, default=30, help="default wait timeout in seconds")
    ap.add_argument("--max-frames", type=int, default=60 * 60 * 10,
                    help="MELEE_EXIT_AFTER_FRAMES backstop (default 10 minutes)")
    args = ap.parse_args()
    if sys.platform != "win32":
        ap.error("Windows only; on Linux use MELEE_KEY_FIFO with a mkfifo pipe")
    if (args.script is None) == (args.inline is None):
        ap.error("give a script file or -c TEXT, not both")
    if args.inline is not None:
        text = args.inline
    elif args.script == "-":
        text = sys.stdin.read()
    else:
        with open(args.script, encoding="utf-8") as f:
            text = f.read()
    try:
        steps = parse_steps(text, args.inline is not None)
    except ScriptError as e:
        ap.error(str(e))
    sys.exit(run(args, steps))


if __name__ == "__main__":
    main()
