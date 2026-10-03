---
name: code-review
description: "Review a change from four viewpoints in parallel (intent and player experience, correctness, robustness and online safety, design), fix what is plainly the agent's to fix in a separate commit, and report the rest for the maintainer to decide. Use to review uncommitted work, a commit, a range or a branch, and after implementing a feature."
---

A review here judges a change against its intent, not the letter of its spec. A spec is the starting shape of a feature; play-testing decides how well the idea worked, so a departure from the spec matters only when it works against what the spec was for. The review ends with the agent's own fix pass: what is plainly fixable gets fixed, and the maintainer decides only what needs their judgement.

## 1. Pin the target

- An argument naming a commit, range or branch is the target: `git show <commit>`, `git diff <range>`, or `git diff master...<branch>`.
- With no argument, the target is the uncommitted work (`git diff HEAD`, plus the untracked files `git status` lists), or the latest commit (`git show HEAD`) when the tree is clean.
- Any focus instructions in the argument ("focus on the coin logic") go to every reviewer.

Confirm the target resolves and its diff is non-empty before going on, and list its changed files. Changes inside the decomp layer beyond hook lines are out of scope.

## 2. Find the intent

The intent is what the change is meant to achieve, in a few sentences. Take it from the first source that exists:

1. A spec or issue: a path in the argument, a `.scratch/` issue named in the commit messages, or the `.scratch/<feature>/` folder the change belongs to.
2. The commit messages.
3. The request in this session that started the work.

Write it down for the intent reviewer. Subagents do not see this conversation, so an intent taken from the session must be spelled out in their prompt.

## 3. Run the four reviewers in parallel

Spawn four `general-purpose` subagents in one message, one per viewpoint below. Each prompt carries the diff command, the changed files (the reviewer reads them itself), its viewpoint pasted in full, the focus instructions, and this brief: "Report findings only; edit nothing. For each finding give file:line, what goes wrong and when, a suggested fix, and your confidence. Under 400 words." The intent reviewer also gets the intent from step 2; the design reviewer also gets the paths of `CODING_STYLE.md`, `CLAUDE.md` and `CONTEXT.md`.

## 4. Check every finding

Reviewers raise false alarms. Read the code at every finding and confirm it or drop it. Merge duplicates; two reviewers flagging the same thing raises confidence. Then sort each confirmed finding into **fix**, **your call** or **drop** by the rules under "What the fix pass decides". The step is done when every finding has a group.

## 5. Fix and verify

If the target is uncommitted work, first stage the reviewed files by name, so the fixes stay apart as unstaged changes: `git diff --cached` is the reviewed work, `git diff` the fixes.

Apply every fix, then run the `verify` skill at the level the fixes call for. A fix that grows past its expected size, or breaks verification for a reason that is not plain, is reverted and moved to your call.

If the target is committed, commit the fixes as their own commit: the area prefix of the reviewed commits, then "review fixes" (`A11y: character select, review fixes`), with one body line per fix. Uncommitted work stays uncommitted, split as above.

## 6. Report

The maintainer reads with a screen reader and navigates by heading, so report under three headings, one line per item with its file:line:

- **Fixed**: what changed and why, then the verify level and its result, and the fix commit's hash.
- **Your call**: most severe first, each with the problem, the options, and the agent's recommendation.
- **Dropped**: the count, with a few words on any drop the maintainer might question.

## The four viewpoints

### 1. Intent and player experience

- Does the change achieve the intent? Name anything the intent needs that is missing, and any departure from a spec that works against the spec's purpose or has no explanation. A departure that serves the purpose is fine.
- Listen to the change as a blind player would, through the announcements and cues the code produces: speech said twice or not at all at a transition, one thing split across several announcements (CLAUDE.md: compose each announcement as one text), words that only make sense to someone seeing the screen, an announcement that interrupts one the player still needs, silence where the player needs to know the result of an action.
- When the change touches nothing a player hears (tooling, build, docs), review the intent alone.

### 2. Correctness and logic

Bugs, logic errors, off-by-one errors, wrong conditions, states and transitions the code does not handle (a screen left and re-entered, a player slot changing hands, a menu opened from a different path), and fork code reading game data in the wrong units or at the wrong moment of the frame.

### 3. Robustness, memory safety and online safety

- Memory safety: buffer overflows, use-after-free, uninitialized reads, integer overflow in size and index math, fixed-size buffers that the game's text or a table can outgrow.
- Lifetimes: pointers into game objects or scene data kept past the point the game frees them, chiefly across a scene change; per-screen state left stale when the player leaves and returns.
- The C/C++ boundary: hooks are called from C, so no exception may escape a `pc_a11y_*` function.
- Undefined behaviour: strict aliasing, alignment, signed overflow, and state shared with another thread, such as the audio mixer's.
- Online safety (CLAUDE.md, "Online compatibility"): fork code reads game state and never writes it, the pad hook being the one exception; it never calls a game function with side effects, such as one that draws a random number; speech and cues stay silent while `pc_net_resim()` is true; and every cue is wrapped in `net_sfx_private(true)` / `net_sfx_private(false)`. A slip here breaks online play against base port builds with no visible error.

### 4. Design and standards

- `CODING_STYLE.md`, minus what `tools/check_style.py` already enforces.
- The isolation rule in CLAUDE.md: fork code in `src/pc/a11y/`, its footprint in base port files limited to one-line hooks declared in `a11y_hooks.h` and the marked screen-reader-only fixes.
- The docs rules in CLAUDE.md: a change to verification updates the `verify` skill; a landed feature updates its "Accessibility status" line; a primer or ADR the change makes stale is updated; the vocabulary of `CONTEXT.md` is used.
- Code smells, each a judgement call that a documented repo standard overrides: _Mysterious Name_ (the name hides what it does or holds; the decomp's address-style names are exempt), _Duplicated Code_ (one logic shape in several places; extract it), _Feature Envy_ (a function working mostly on another module's data; move it there), _Data Clumps_ (fields or parameters that always travel together; make them one type), _Primitive Obsession_ (a bare int or string standing in for a domain concept), _Repeated Switches_ (the same switch on the same value in several places; one table both share), _Shotgun Surgery_ (one logical change scattered over many files), _Divergent Change_ (one file changed for unrelated reasons), _Speculative Generality_ (parameters, hooks or abstraction no current need uses; inline them), _Middle Man_ (a function that only forwards; call the target).

## What the fix pass decides

**Fix**: findings with one plainly right answer that stays inside the change's own modules:

- Bugs and robustness or online-safety slips whose fix is clear.
- Duplicated code, unclear names, dead code, small restructuring inside one module.
- Style, missing or wrong comments, stale docs the change should have updated.
- Plain errors in spoken text: a misspelling, a wrong name, a wrong value.

**Your call**: findings that need the maintainer's judgement:

- Any other change to what the player hears: wording, order, timing, what is said at all.
- Departures from the intent, and doubts about how something should behave.
- Refactors that cross modules or change an interface, and anything that would deserve an ADR.
- New edits to base port files.

**Drop**: false alarms, findings the intent or a documented standard already answers, and nitpicks with no effect on behaviour or reading.
