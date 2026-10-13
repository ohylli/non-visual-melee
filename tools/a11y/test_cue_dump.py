#!/usr/bin/env python3
"""The dump analysis script against a known pulse train.

cue_engine_test writes the dump (write_pulse_train in
src/pc/a11y/test_cue_engine.cpp): two seconds of a 1 kHz tick after 12 ms of
lead-in silence, every 0.25 s at pan +0.5, then from 1 s every 0.125 s at pan
-0.5. Run by ctest after it, with the dump's path.
"""

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
    print("test_cue_dump: passed")


if __name__ == "__main__":
    main()
