# 06 Play-test the proof-of-life build by ear

Status: ready-for-human
Type: task
Blocked by: 03, 05

Maintainer, with NVDA. Report what you hear for each; the agent then reads the `[a11y]` lines against the report.

1. Normal launch with NVDA running. Expected: "Non-Visual Melee ready" before the launcher window appears. Log: `speech backend: NVDA`.
2. Launch with NVDA closed. Expected: the same words from a Windows voice. Log: the fallback backend's name (OneCore or SAPI).
3. `MELEE_A11Y=0` launch. Expected: silence. Log: `speech off (MELEE_A11Y=0)` and `speak interrupt (off): "Non-Visual Melee ready"`.
4. Rename `prism.dll` in the build directory and launch. Expected: NVDA reads a Windows dialog saying `prism.dll` was not found, and the game does not start. Rename it back.
5. NVDA restart while the game runs: cannot be tested yet, since only the startup announcement exists. Carry this case into the first native menu narration feature.

Anything unexpected becomes a new issue in this directory.
