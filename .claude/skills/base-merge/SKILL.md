---
name: base-merge
description: Merge the base port's master into the fork and report what changed upstream, ranked by what matters to the accessibility work.
disable-model-invocation: true
---

A base merge brings `upstream/master` into `master` (see "Remotes and branches" in `CLAUDE.md`). The deliverable is two things: a clean merge with every hook and `[a11y]` line intact, and a **digest** of the base port's changes written for the maintainer, who cannot skim a diff.

## 1. Survey before merging

Fetch, then size the drop and read the base port's own account of it:

```
git fetch upstream
git log --oneline master..upstream/master
git diff --stat master...upstream/master
git diff master...upstream/master -- .github/RELEASE_NOTES.md README.md ROADMAP.md docs/building.md docs/testing.md docs/debugging.md tools/check_style.py tools/smoke_test.py
git diff master...upstream/master -- src/UPSTREAM_COMMIT
```

`.github/RELEASE_NOTES.md` is the base port's own account of a release; use it as the outline and the commit list as the check that nothing was left out (commits after the last tag have no notes yet). A change to `src/UPSTREAM_COMMIT` means a decomp sync: note the new commit and treat the decomp layer's diff as one bulk item. Other new or changed files under `docs/` and `tools/` get named from the stat, not read in full: `tools/test_*` and netplay fixtures are one bulk item.

Then read the code diff by layer, not as one stream. Split it into the port layer (`src/pc`, `extern/aurora`, `resources`) and the game layer (`src/melee`, `src/sysdolphin`), leaving out vendored bulk (`extern/dht`, `extern/monocypher`, `stb_*`, `net*.c`). Read the port layer yourself. When the game-layer diff runs to several hundred lines, hand it to a subagent for a per-file behaviour summary that calls out menu flow, scene transitions, audio, input and any new function that looks like a semantic transition, and keep only its conclusions.

List the environment variables the drop adds, including new values of existing ones, for the tooling section of the digest:

```
git diff master...upstream/master -- src tools | grep -E '^\+.*(getenv|MELEE_[A-Z0-9_]+=)'
```

Check every rule in "Online compatibility" in `CLAUDE.md` against the drop: the version string and protocol version, what the LAN lobby and handshake compare between peers, `pc_net_resim()`, and the per-frame journal of sound starts. A change to any of them is a digest item in section 1, with what it means for playing fork builds against base port builds.

Also diff the fork's own side, so the merge notes can say whether a fork fix in a base port file is still needed or has been superseded:

```
git diff --stat upstream/master...master
git diff upstream/master...master -- <each base port file it lists>
```

Record two baselines before touching anything, so step 3 can compare against them. The hook inventory:

```
grep -rn pc_a11y_ src --exclude-dir=a11y
```

And a bounded `vs` run on the pre-merge build, rebuilt first with `cmake --build build` so it matches `master` (the exact command is under "Bounded runs" in the `verify` skill), with its log and stdout kept in the scratchpad. Check stdout for the line that proves the match was entered (also there). After the merge the old log cannot be regenerated without rebuilding the old commit.

## 2. Merge

On `master`, `git merge --no-commit --no-ff upstream/master`, so the digest can go into the merge message; without `--no-ff`, a fork with no commits since the last merge fast-forwards and there is no merge commit to hold it. Conflicts follow the `resolving-merge-conflicts` skill with one fork-specific rule from the isolation rule: in a base port file the fork owns nothing but one-line hooks, so keep theirs and re-place the hook at the semantic transition it marked. Where the base port has superseded a fork fix (as with the DLL staging in the first merge), drop the fork's version and say so in the digest. If the maintainer reported that bug to the base port, look up the issue's state with `gh issue view` and put it in the merge notes; commenting on or closing it is the maintainer's call.

## 3. Verify

Done means all of these hold:

- The hook inventory matches the one from step 1, or every difference is explained (a hook moved with its call site, or its transition no longer exists).
- `cmake --build build` succeeds. If `a11y.cmake` or the root `CMakeLists.txt` changed, reconfigure first. A change to compiler launchers or flags rebuilds the whole decomp layer; that takes minutes and is not a problem.
- A bounded `vs` run exits cleanly, its stdout shows the match was entered, and its log matches the baseline log from step 1. Compare whole logs with the timestamps stripped, which covers the `[a11y]` lines and everything around them: `diff <(sed 's/^\[[^]]*\]//' pre.log) <(sed 's/^\[[^]]*\]//' post.log)`. Expected differences: counters such as the input-poll total, the `CARD API Initialized BUILT` date, and aurora's shader pipeline and Dawn cache lines (the first run after a drop that changes shaders compiles them afresh). Any other difference needs explaining in the merge notes.
- `python tools/check_style.py` passes on `src/pc/`.
- Every code name the fork's docs mention still exists; a decomp sync renames address-style functions once they are understood. The check below lists names found nowhere in the code, some of them ordinary words that were never code (`PATH`). Update each stale name to its new one, found in the decomp diff, as part of the merge:
  ```
  for n in $(grep -oh '`[A-Za-z_][A-Za-z0-9_]*`' docs/a11y/*.md CLAUDE.md CONTEXT.md | tr -d '`' | sort -u); do grep -rqw -- "$n" src tools CMakeLists.txt || echo "missing: $n"; done
  ```
- The base port unit tests (`cmake --build build --target unit_tests`, then `ctest --test-dir build -L melee`) show no new failures against the known ones listed in the `verify` skill. New test targets are a digest item.
- The `verify` skill's base merge level passes, its drive scripts and online runs included.

## 4. Digest

Write the digest for a reader who is blind, an experienced engineer, and new to game development: plain prose, jargon spelled out the first time. Rank by relevance to the fork, and keep it short by leaving things out rather than compressing them. A single routine release should fit in about fifty lines; several releases at once may need three times that, but nothing beyond a sentence or two per item.

Sections, in this order; omit an empty one:

1. **Affects the accessibility work.** Anything touching the areas the fork hooks or plans to hook: native menus, the port menu, the launcher, input, the audio path and mixer, logging, scene boot, save data, or the files listed in the hook inventory. Say what changed and what it means for the fork (a hook to re-place, a cue path that moved, a new transition worth hooking).
2. **Documentation, testing and debugging tooling.** New or rewritten docs under `docs/`, new environment variables, new test targets, smoke test changes, debug facilities. For each, say whether "Build and run" in `CLAUDE.md` or the `verify` skill now needs an update. Make that edit in the working tree after the merge commit and leave it uncommitted for the maintainer to review; in the digest, refer to it as "a proposed `CLAUDE.md` change" or "a proposed `verify` change".
3. **New features.** One or two sentences each.
4. **Everything else.** Refactors and behaviour changes worth a sentence. Minor bug fixes collapse into one line with a count and the areas they touched.
5. **Merge notes.** Conflicts and how they were resolved, fork fixes kept or dropped because the base port superseded them, and the verification result.
6. **For the maintainer to check.** What the agent's runs cannot confirm, each with how to check it: a fix that depends on the maintainer's hardware, a change to native menus, the port menu or the launcher, anything heard rather than logged.

## 5. Commit and report

Write the digest to a file in the scratchpad and commit with `git commit -F <file>`: a body this long does not survive shell quoting on the command line. The subject is `Merge base port master (<version or short hash>)` and the digest is the body. Reply with the digest itself, not a pointer to the commit, so the maintainer reads it in place. If there is a proposed `CLAUDE.md` change, end the reply with it: what it adds and why, left uncommitted until the maintainer has reviewed it.
