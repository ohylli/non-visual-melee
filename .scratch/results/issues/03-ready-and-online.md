# 03 Ready, online and quick chat

Status: ready-for-agent
Type: task
Blocked by: 02

Spec: `.scratch/results/spec.md`, "Ready" and "Quick chat". Primer: `docs/a11y/netplay.md`.

- **Your ready.** Your Start says "Ready, waiting for P2." (every human not yet ready), or "Ready, waiting for your opponent." online. A second Start says "Not ready.". With one human nothing is said: the scene leaves.
- **Another human's ready.** It is said as "P2 ready." or "Your opponent is ready.", queued after current speech. Their un-ready is said as "P2 not ready." or "Your opponent is not ready." (proposed in writing).
- **While you are ready**, the reading cursor keeps working (proposed in writing).
- **Quick chat.** On results, `pc_a11y_chat_buttons` masks the D-pad while the reader is active, as it does on character select.
- **Leaving.** Settle open question 8: drop a queued announcement when the scene leaves, and check a disconnect mid-results.

## Done when

- Unit tests cover ready, un-ready, waiting with one, two and three others, another human's ready and un-ready, and the online forms.
- `tools/a11y/drive_net.py` runs fork against fork and against the base port build (`--base-b`):
  - both instances reach results, open the stats and step with the D-pad, and no quick chat phrase is sent;
  - one readies and hears "Ready, waiting for your opponent.", the other hears "Your opponent is ready.";
  - the first un-readies and readies again, and both then ready, and the session returns to character select.

  Recordings agree. Add a script for this or extend an existing one.
- Build, unit tests and style pass; the online level of the `verify` skill passes.

## Comments
