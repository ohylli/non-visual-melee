#!/usr/bin/env python3
"""Analyse a dump of the cue engine's mix, so a cue can be checked without
hearing it.

A dump is what MELEE_A11Y_CUE_DUMP=<file> writes: the cue mix as raw 32-bit
float stereo at 48 kHz, interleaved left and right, from the moment the engine
opens. For each window of the dump this reports:

- level: the RMS of both channels, in dBFS, and the peak;
- pan: the left-right balance as the engine's constant-power pan that would
  give it, from -1 (all left) to +1 (all right);
- onsets: how many pulses start in the window, and the intervals from each
  one's previous pulse (median, smallest, largest);
- pitch: the dominant frequency, from the spacing of the mono mix's upward
  zero crossings while something sounds.

Onsets are found where the level rises past a tenth of the dump's peak after
at least 5 ms below a quarter of that; a held tone has one onset, at its
start. Two cues sounding at once blur the pan and the pitch; check one cue at
a time, or look at the windows where only one sounds.

Standard library only: the Python CMake finds has no numpy. About a second
per ten seconds of dump; --start and --end narrow it.

Usage:
  python tools/a11y/cue_dump.py DUMP [--window 0.5] [--start S] [--end S] [--onsets]
"""

import argparse
import math
import statistics
import sys
from array import array

RATE = 48000
BLOCK = RATE // 1000  # 1 ms, the onset detector's step
QUIET_BLOCKS = 5  # ms below the low threshold before a new onset counts


def read_dump(path, start=0.0, end=None):
    """The dump's left and right channels, as two arrays of floats, from start
    to end seconds."""
    data = array("f")
    with open(path, "rb") as f:
        data.frombytes(f.read())
    if sys.byteorder != "little":
        data.byteswap()
    first = int(start * RATE) * 2
    last = len(data) if end is None else min(len(data), int(end * RATE) * 2)
    data = data[first:last]
    return data[0::2], data[1::2]


def block_peaks(left, right):
    """The peak of both channels in each 1 ms block."""
    peaks = []
    for b in range(0, len(left), BLOCK):
        peaks.append(
            max(
                max(map(abs, left[b : b + BLOCK]), default=0.0),
                max(map(abs, right[b : b + BLOCK]), default=0.0),
            )
        )
    return peaks


def thresholds(peaks):
    """The onset detector's high and low thresholds for this dump."""
    high = max(0.1 * max(peaks, default=0.0), 1e-4)
    return high, high / 4


def find_onsets(left, right, peaks=None):
    """Frames where a pulse (or a held sound) starts."""
    peaks = block_peaks(left, right) if peaks is None else peaks
    high, low = thresholds(peaks)
    onsets = []
    quiet = QUIET_BLOCKS
    for b, p in enumerate(peaks):
        if p < low:
            quiet += 1
            continue
        if p >= high and quiet >= QUIET_BLOCKS:
            first = b * BLOCK
            frame = next(
                i
                for i in range(first, min(first + BLOCK, len(left)))
                if abs(left[i]) >= high or abs(right[i]) >= high
            )
            onsets.append(frame)
        quiet = 0
    return onsets


def dominant_pitch(left, right, first, last, peaks):
    """The median spacing of upward zero crossings of the mono mix, as a
    frequency, over the sounding part of frames first to last; None when
    nothing sounds."""
    _, low = thresholds(peaks)
    periods = []
    previous = None  # the last upward crossing, in fractional frames
    last_value = 0.0
    for i in range(first, last):
        value = left[i] + right[i]
        if peaks[i // BLOCK] < low:
            previous = None
        elif last_value <= 0.0 < value:
            crossing = i - 1 + (-last_value) / (value - last_value)
            if previous is not None:
                period = crossing - previous
                if RATE / 8000 <= period <= RATE / 20:
                    periods.append(period)
            previous = crossing
        last_value = value
    if not periods:
        return None
    return RATE / statistics.median(periods)


def analyse(left, right, window=0.5, start=0.0):
    """One dict per window: time, level_db, peak_db, pan, onsets (times),
    intervals (seconds), pitch_hz. Silent windows have None for level, pan
    and pitch."""
    peaks = block_peaks(left, right)
    onsets = find_onsets(left, right, peaks)
    size = max(int(window * RATE), 1)
    windows = []
    for first in range(0, len(left), size):
        last = min(first + size, len(left))
        sum_l = sum(x * x for x in left[first:last])
        sum_r = sum(x * x for x in right[first:last])
        n = last - first
        rms = math.sqrt((sum_l + sum_r) / (2 * n))
        top = max(peaks[first // BLOCK : (last + BLOCK - 1) // BLOCK], default=0.0)
        here = [k for k, f in enumerate(onsets) if first <= f < last]
        silent = rms < 1e-6
        windows.append(
            {
                "time": start + first / RATE,
                "level_db": None if silent else 20 * math.log10(rms),
                "peak_db": None if silent else 20 * math.log10(top),
                "pan": None
                if silent
                else math.atan2(math.sqrt(sum_r), math.sqrt(sum_l)) * 4 / math.pi - 1,
                "onsets": [start + onsets[k] / RATE for k in here],
                "intervals": [(onsets[k] - onsets[k - 1]) / RATE for k in here if k > 0],
                "pitch_hz": None if silent else dominant_pitch(left, right, first, last, peaks),
            }
        )
    return windows


def describe(w):
    if w["level_db"] is None:
        return f"{w['time']:7.2f}  silent"
    intervals = w["intervals"]
    if intervals:
        spread = (
            f"{statistics.median(intervals):.3f} "
            f"({min(intervals):.3f}-{max(intervals):.3f})"
        )
    else:
        spread = "-"
    pitch = "-" if w["pitch_hz"] is None else f"{w['pitch_hz']:.0f}"
    return (
        f"{w['time']:7.2f}  {w['level_db']:6.1f}  {w['peak_db']:6.1f}  {w['pan']:+5.2f}"
        f"  {len(w['onsets']):6d}  {spread:21}  {pitch:>6}"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("dump", help="raw f32 stereo 48 kHz, from MELEE_A11Y_CUE_DUMP")
    parser.add_argument("--window", type=float, default=0.5, help="seconds per row")
    parser.add_argument("--start", type=float, default=0.0, help="seconds into the dump")
    parser.add_argument("--end", type=float, default=None, help="seconds into the dump")
    parser.add_argument("--onsets", action="store_true", help="also list every onset's time")
    args = parser.parse_args()

    left, right = read_dump(args.dump, args.start, args.end)
    if not left:
        sys.exit(f"{args.dump}: no audio in that range")
    windows = analyse(left, right, args.window, args.start)
    print(f"{args.dump}: {len(left) / RATE:.2f} s")
    print("   time   level    peak    pan  onsets  interval s (min-max)    pitch Hz")
    for w in windows:
        print(describe(w))
    if args.onsets:
        print("onsets:", " ".join(f"{t:.3f}" for w in windows for t in w["onsets"]))


if __name__ == "__main__":
    main()
