# 04 Stage select and results announcements

Status: ready-for-agent
Type: task

Spec: `.scratch/lan-play/spec.md`, section "Stage select and results announcements". Both are scene announcements in `src/pc/a11y/scene_speech.cpp`.

- Stage select, online only (`pc_net_active()` or the fork's equivalent): "Stage select. No speech yet. Press Start for a random stage, then wait for your opponent." Offline unchanged.
- Results, offline and online: say how to continue. First find out with drive runs what actually leaves results, offline and online; `docs/netplay-verification.md` says it exits on Start only in some cases and otherwise on an internal counter, and its harness could not leave it. Word the announcement to match ("Press Start to continue." if Start works; otherwise say what does).
- Update unit tests and expected lines; update the `verify` skill if any expected line it lists changes.
