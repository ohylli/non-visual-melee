---
name: spec-implement
description: "Implement a spec's issues, or one issue, through implementer subagents, then a code review subagent, then a summary for the maintainer."
disable-model-invocation: true
---

You are the coordinator. Every issue gets its own implementer subagent, which implements, verifies, updates and commits it; once the implementable issues are in, a reviewer subagent runs the `code-review` skill over the work; you end with a summary. You write no feature code yourself: you order the work, carry notes between agents, keep them unstuck, and report.

## 1. Read the work

The argument is a spec (`.scratch/<feature>/spec.md` or its folder) or one issue file (`.scratch/<feature>/issues/NN-*.md`).

- Read the spec and every issue file, and the conventions in `docs/agents/issue-tracker.md` and `docs/agents/triage-labels.md`.
- The **task graph**: each issue's `Blocked by:` line. Implementable means `Status: ready-for-agent`. `resolved` issues are done; `ready-for-human` ones (a play test by ear) and the other statuses are left for the maintainer.
- With one issue as the argument, its blockers must already be `resolved`; if one is not, stop and say which.
- Write down the **base commit** (`git rev-parse HEAD`) and confirm the tree is clean: the review range starts there.
- Confirm no game runs from `build/melee.exe` (`Get-Process melee`). The maintainer may be playing; a running exe blocks the link and every drive run. If one runs, ask the maintainer to close it before the first implementer starts.

## 2. Plan the order and tell the maintainer

Implementers run **one at a time, in this working tree, on the current branch**:

- Online runs need this repo's `build/melee.exe`: the firewall allows only it and the base port worktree's, and any other build directory raises a prompt that takes focus.
- Drive runs compete for the input driver and the window, and a full rebuild in a fresh worktree takes long.
- Each implementer builds on the commits and issue comments of the ones before it.

Order the implementable issues so each one's blockers come first. When several are ready, take the one whose output later issues build on, such as wording that later drive scripts must expect, before the rest; otherwise the lowest number.

Tell the maintainer the order, which issues are left for them, and the defaults (commits on the current branch, nothing pushed). Ask only what blocks a start; the spec settles the rest. Then begin.

## 3. Run an implementer per issue

Spawn each with the `Agent` tool: `subagent_type: general-purpose`, a `name` (`impl-NN`), and the prompt below with every `<placeholder>` filled in; `<attribution line>` is the commit attribution your own instructions give, since subagents may not see it. It runs in the background; its report arrives as a message.

```
You are the implementer for one issue of the <feature> feature in the Non-Visual Melee fork (repo <repo path>, branch <branch>). Follow CLAUDE.md and CLAUDE.local.md exactly.

Your issue: <issue path>. Spec: <spec path>. Issues <list> are committed; read their issue files' Comments first.
<Notes for this issue from earlier implementers' reports, as pointers or short facts.>

Do:
1. Implement the issue fully, test-first where there is a natural seam.
2. Verify with the `verify` skill at the level the change calls for, plus `python tools/check_style.py`. Run every build, test and game run in the foreground and read its result before going on. If build/melee.exe is locked by a game the maintainer started, tell the coordinator (SendMessage to main) and do not kill or rename it. <Known failures, e.g. a unit test that already fails.>
3. Do every "Done when" item, docs and the `verify` skill included. Answer any spec open question that is yours.
4. Update the issue file: `Status: resolved (<today>)`, and a dated entry under `## Comments` with what was implemented, findings worth keeping, and anything left open, in the style of the resolved issues.
5. Commit to <branch> in the style of `git log`, ending each message with `<attribution line>`. Do not push.

If something cannot be done as specified, make the sensible choice, record it in the issue's Comments and your report. If a Done-when item truly failed, leave the issue `ready-for-agent` with a comment. If you start helper subagents, wait for their reports yourself.

Final report, short: commit hashes, verification and results, deviations from the spec, open remarks for the maintainer, and notes for the issues after yours.
```

When a report arrives:

1. Check `git log` and `git status`: the commits exist and the tree is clean. A dirty tree means work is unfinished; message the implementer.
2. Collect its open remarks for the summary, and its notes for later issues for their prompts.
3. Tell the maintainer in a few lines what landed, any deviation, and what starts next.
4. Start the next issue. An issue left `ready-for-agent` blocks its dependents: skip them, implement the rest.

## 4. Keep agents unstuck

Messages from subagents need judgement, not relay:

- An **idle notification** from a finished teammate repeats its report; mention only what it adds.
- A **misrouted report**: a helper's report (a fact-checker, a review viewpoint) often reaches you instead of the agent that started it, and that agent then waits for it forever. Forward each one with `SendMessage` to its spawner, saying it came to you, unless the report says it was also sent to its spawner directly.
- A **locked exe**: an implementer reporting that `build/melee.exe` is locked means the maintainer's game is running. Ask the maintainer to close it, and tell the implementer once they have; never kill or rename it.
- A **stall**: when an agent's last message says it is waiting on scripts or helpers, look at once rather than wait. Its background tasks write to the `tasks/` folder of this session's temp directory (`*.output`); if they finished, send the agent their results and tell it to go on in the foreground.
- **Overlap**: a follow-up you send to a finished implementer runs beside the next one in the same tree. Afterwards check with `git show --stat` that its commit holds only its own files.

## 5. Review

Once every implementable issue is done, spawn a reviewer (`name: reviewer`):

```
Review and fix the <feature> work in the Non-Visual Melee fork (repo <repo path>, branch <branch>). Follow CLAUDE.md and CLAUDE.local.md exactly.

Invoke the `code-review` skill on the range <base commit>..HEAD (<one line per commit: hash, issue>; spec <spec path>). <Changes outside the feature the review must keep in view, e.g. a refactor shared with another screen.> Follow the skill: fix what is plainly yours in a separate commit, and leave the rest for the maintainer. After the fixes, verify with the `verify` skill at <highest level any issue needed>, or say in your report why a lower level suffices. Run verification in the foreground; if build/melee.exe is locked by a game the maintainer started, tell the coordinator (SendMessage to main) and do not kill or rename it. The four viewpoint reports may come to you through the coordinator; carry on once all four are in. <Known failures.> A failure or flake not among the known ones: if it recurred or has a clear cause, record it in the `verify` skill in your fix commit; if seen once, list it in your report.

Commit in the style of `git log`, ending each message with `<attribution line>`. Do not push. Record the items left for the maintainer where the skill or the feature's issues say (the play test issue's Comments, if there is one).

Final report, short: fix commit hash with what each fix changed, verification and results, items left for the maintainer each with a one-line why, and dropped findings the maintainer might question.
```

The four viewpoint reports usually reach you as hand-backs, and often go to the reviewer directly as well. Forward only those that do not say they were sent to the reviewer. If the reviewer says it is still waiting, tell it which reports are already in its inbox rather than sending them again. The step is done when the reviewer's fix commit is in, the tree is clean, and its report has arrived.

## 6. Summarize

The maintainer reads with a screen reader and navigates by heading. Summarize under headings, short lines, file paths and hashes where they help:

- **What a player hears now**: the feature as a player meets it.
- **Commits**: a table of issue, commits and one line each, the review fix commit last.
- **Changes outside the feature**: anything a shared refactor or fix changed elsewhere, such as another screen's behaviour.
- **Verification**: the highest level that passed after the review fixes, and why if it is lower than an issue needed; known failures, flakes and their reruns, and whether a new flake was recorded in the `verify` skill.
- **What the review fixed**.
- **Open remarks**: everything left for the maintainer from implementers and the reviewer, merged and deduplicated, most important first, each with its recommendation.
- **For the play test**: what only a listener can judge, and what no drive run reached.

End with what is left: issues not implemented and why, and the play test.
