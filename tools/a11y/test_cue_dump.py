#!/usr/bin/env python3
"""The dump analysis script against a known pulse train.

cue_engine_test writes the dump (write_pulse_train in
src/pc/a11y/test_cue_engine.cpp): two seconds of a 1 kHz tick after 12 ms of
lead-in silence, every 0.25 s at pan +0.5, then from 1 s every 0.125 s at pan
-0.5. Run by ctest after it, with the dump's path. Then --band against two
trains made here, which overlap: a low pulse on the right every 0.4 s and a
high tick on the left every 0.15 s.
"""

import math
import pathlib
import statistics
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cue_dump  # noqa: E402


def check(condition, message):
    if not condition:
        sys.exit(f"test_cue_dump: {message}")


def main():
    left, right = cue_dump.read_dump(sys.argv[1])
    check(len(left) == 2 * cue_dump.RATE, f"dump of {len(left)} frames, not two seconds")
    windows = cue_dump.analyse(left, right, window=0.5)
    for w in windows:
        print(cue_dump.describe(w))

    onsets = [t for w in windows for t in w["onsets"]]
    starts = [0.25 * k for k in range(4)] + [1.0 + 0.125 * k for k in range(8)]
    check(len(onsets) == len(starts), f"{len(onsets)} onsets, expected {len(starts)}")
    for found, start in zip(onsets, starts):
        # The tick sounds after its lead-in, and passes the detector's
        # threshold within its first millisecond.
        check(0.012 <= found - start <= 0.014, f"onset at {found:.4f}, pulse at {start:.3f}")

    expected = [(0.5, 0.25), (0.5, 0.25), (-0.5, 0.125), (-0.5, 0.125)]
    for w, (pan, interval) in zip(windows, expected):
        at = f"window at {w['time']:.1f} s"
        check(abs(w["pan"] - pan) < 0.05, f"{at}: pan {w['pan']:+.3f}, expected {pan:+.2f}")
        median = statistics.median(w["intervals"])
        check(abs(median - interval) < 0.001, f"{at}: interval {median:.4f}, expected {interval}")
        check(abs(w["pitch_hz"] - 1000) < 5, f"{at}: pitch {w['pitch_hz']:.1f} Hz")
    two_trains()
    print("test_cue_dump: passed")


def two_trains():
    rate = cue_dump.RATE
    left = [0.0] * (2 * rate)
    right = [0.0] * (2 * rate)

    def add(channel, start, hz, seconds, decay, gain):
        first = int(start * rate)
        for i in range(int(seconds * rate)):
            if first + i < len(channel):
                t = i / rate
                channel[first + i] += gain * math.sin(2 * math.pi * hz * t) * math.exp(-decay * t)

    for k in range(5):
        add(right, 0.4 * k, 400, 0.08, 30, 0.5)
    for k in range(14):
        add(left, 0.01 + 0.15 * k, 1500, 0.02, 150, 0.5)
    mixed = cue_dump.analyse(left, right, window=2.0)
    for name, band, interval, side in (("low", (150, 700), 0.4, 1), ("high", (1100, 12000), 0.15, -1)):
        windows = cue_dump.analyse(cue_dump.band_pass(left, *band), cue_dump.band_pass(right, *band), window=2.0)
        w = windows[0]
        median = statistics.median(w["intervals"])
        check(abs(median - interval) < 0.002, f"{name} band: interval {median:.4f}, expected {interval}")
        check(max(w["intervals"]) - min(w["intervals"]) < 0.004,
              f"{name} band: intervals {min(w['intervals']):.4f} to {max(w['intervals']):.4f}")
        check(w["pan"] * side > 0.9, f"{name} band: pan {w['pan']:+.3f}")
    # Unfiltered, a tick inside a pulse is lost.
    check(len(mixed[0]["onsets"]) < 5 + 14, "the mix alone kept every onset")


if __name__ == "__main__":
    main()
