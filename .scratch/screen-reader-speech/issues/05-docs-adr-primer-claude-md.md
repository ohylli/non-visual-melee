# 05 Docs: primer, CLAUDE.md status line, log-path correction

Status: ready-for-agent
Type: task
Blocked by: 03

- `docs/a11y/speech.md`: the primer. Opens with a short mental model: an announcement is one text plus interrupt-or-queue, it goes to speech, speech logs it and hands it to the screen reader bridge, which hands it to Prism, which hands it to whatever screen reader is running or to a Windows voice. Then: the switches (`MELEE_A11Y`, `MELEE_A11Y_LOG`), the speech log lines with an example, the threading rule and why, the Prism pin and how to bump it (tag plus hash in `a11y.cmake`), and why the DLL is prebuilt (link to ADR-0001). Plain terms, jargon spelled out on first use.
- ADR-0001 already exists (`docs/adr/0001-prebuilt-prism-dll.md`); link it, do not rewrite it.
- CLAUDE.md "Accessibility status": add one line: speech subsystem with Prism, `src/pc/a11y/`, proof-of-life announcement at startup, switches `MELEE_A11Y` and `MELEE_A11Y_LOG`.
- CLAUDE.md "Build and run": correct the sentence about `melee-pc.log`: it is written in the working directory, not beside `melee.exe`.
- CLAUDE.md "Verification": mention that a bounded `title` run's expected `[a11y]` lines are the backend line and the proof-of-life announcement.

Done when: the docs read correctly for someone who has not seen the spec, and `git diff` touches only the files above.
