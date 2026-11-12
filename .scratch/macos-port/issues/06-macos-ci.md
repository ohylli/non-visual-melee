# 06 The fork's CI builds the Mac package

Status: resolved (2026-11-12)
Type: task
Blocked by: 04, 05

Spec: `.scratch/macos-port/spec.md`, "Where things stand". Read it first, then the fork's `.github/workflows/a11y-windows.yml`, the base port's macOS job in `.github/workflows/build.yml` (disabled in the fork), and CLAUDE.md, "Remotes and branches" (the CI paragraph).

## What to build

- **A Mac job** on GitHub's Mac runners that builds and packages the fork as the base port's job does (Homebrew `gcc cmake ninja zstd libpng freetype libusb`, its snapshot check, `tools/package_macos.sh`), arm64 at least. Intel (`macos-15-intel`) only if it costs nothing more than a matrix line; nobody tests it.
- **The package gets** `melee-netcheck` beside the app (built in the same build directory, as the Windows job does) and Prism's licences, as the Windows job adds them. Name the zip as the Windows one is named (`NonVisualMelee-macOS-arm64.zip`).
- **Published to `ci-latest`** beside the Windows zip. The Windows job deletes and recreates the release, so two workflows publishing separately would undo each other. Recommended: one fork workflow with the Windows and Mac jobs and a publish job that needs both; renaming `a11y-windows.yml` (and its name in CLAUDE.md) is fine. A failing Mac job should not stop the Windows zip from publishing; decide how, and say so in the workflow's header comment.
- **The log shows** what issues 04 and 05 need checked: the compile of `game_access.c` going through Homebrew GCC, and `ls Melee.app/Contents/Frameworks` with the Prism dylib.
- **The fork's unit tests** run on the Mac runner if the base port's test setup builds there without much work; otherwise leave them out and say why in the workflow.

Pushing to `master` is the maintainer's call; develop on a topic branch, with `workflow_dispatch` to run it there.

## Done when

- A run on the topic branch builds both zips; the Mac one holds `Melee.app` with Prism inside, `melee-netcheck` and the licences, and the app's signature verifies in the log (`codesign --verify`).
- Issues 04 and 05 have their Mac halves checked, noted in their comments.
- CLAUDE.md's CI paragraph and the `verify` skill name the new workflow and the Mac zip.

## Comments

### 2026-11-12, implementation (agent)

The fork's workflow builds the Mac packages beside the Windows one. Developed on the topic branch `macos-ci`; master on GitHub is untouched. The run that built all three zips: https://github.com/ohylli/non-visual-melee/actions/runs/43226714268 (the arm64 job on its second attempt, after the first was never given a runner; see below).

**The workflow**, renamed from `.github/workflows/a11y-windows.yml` to `a11y.yml` (name "A11y"), three build jobs and a publish:

- **Windows (x86_64)** as before, step for step, but it keeps its zip as a run artifact instead of publishing. The zip's file list is the same 60 entries as the `ci-latest` zip it replaces.
- **macOS (arm64)** on `macos-15` and **macOS (x86_64)** on `macos-15-intel`, a matrix of two lines; Intel builds, so it stays. The base port's steps (`brew install gcc cmake ninja zstd libpng freetype libusb`, its snapshot check, `tools/package_macos.sh`), then steps that print what issues 04 and 05 needed checked, then the package: one folder, `melee-macos-<arch>/`, holding `Melee.app`, `melee-netcheck` (built in the same build directory, stripped, ad-hoc signed) and `LICENSES.prism/`, zipped with `ditto` as `NonVisualMelee-macOS-<arch>.zip`. A last check unpacks the zip, verifies both signatures (`codesign --verify --deep --strict`: `valid on disk`), prints each binary's oldest macOS (`minos`) and starts `melee --version` from it, which loads every bundled dylib. Then the unit tests.
- **Publish ci-latest** needs all three and runs only on a push to master (`github.event_name == 'push'`, `refs/heads/master`), when the Windows job passed: a failed Mac job leaves its zip out, and the release notes say which is missing; the run still shows failed. Every other run (a push to `macos-ci`, a dispatch anywhere) keeps its zips as run artifacts and leaves `ci-latest` alone; the publish job was skipped in each run on `macos-ci`, as meant.
- **Concurrency** is per branch (`${{ github.workflow }}-${{ github.ref }}`): a newer push cancels an older run of the same branch only, so a `macos-ci` run and a master run never cancel each other.
- **Dispatch**: GitHub dispatches only workflows the default branch has, and master on GitHub still has `a11y-windows.yml`, so the workflow also runs on a push to `macos-ci`. **Once master is pushed**, remove `macos-ci` from the push branches (the comment there says so); `gh workflow run a11y.yml --ref <branch>` then runs it on any branch. The old "A11y Windows" workflow stops running by itself once its file leaves master. Also once master is pushed: its run publishes `ci-latest` with all three zips, and only then does issue 08's "Mac zip from `ci-latest`" exist; until then the zips are run artifacts, which need a GitHub login to download. The `macos-ci` branch can be deleted after.

**The oldest macOS the zip runs on.** The base port sets no deployment target, so its build aims at the runner's macOS: the first arm64 run, on the base port's `macos-26`, printed `minos 26.0` for melee and every Homebrew dylib, which would shut out any Mac not on macOS 26. Now:

- `MACOSX_DEPLOYMENT_TARGET=13.3` for the Mac jobs (CMake, clang and GCC all read it): melee, SDL, the decomp, `game_access.c` and `melee-netcheck` aim at macOS 13.3, where Apple's libc++ first has `std::to_chars` for floats, which the settings use (`value_model.cpp`, `settings_page.cpp`). Prism's own floor, 11.0, was tried first; the Intel build refused it on `to_chars`.
- `std::from_chars` for floats came to libc++ only in LLVM 20, newer than Xcode 16's; it compiled on `macos-26` aimed at macOS 26, and was not tried at 13.3. So a setting's number is now read without it, in its own commit: the same grammar checked by hand, then a stream in the classic locale (`read_decimal` in `value_model.cpp`). A new test, `a_number_is_read_by_the_same_rules_everywhere`, lists the texts taken and refused; it passed on the old parser before the change and on the new one after, on Windows and on both Mac runners. One quirk kept as it was: the leading `+` is dropped before the number is read, so `+-5` reads as -5.
- **Homebrew's dylibs** (libpng, freetype, libusb, and brotli on arm64) are built for the runner's macOS and no deployment target changes them, so the arm64 job runs on `macos-15`, not the base port's `macos-26`. The run prints: arm64, `minos 15.0` for the Homebrew dylibs, 13.3 for melee, SDL and the checker, 11.0 for Prism; Intel, `minos 14.0` for the Homebrew dylibs and the rest as on arm64. **So the arm64 zip needs macOS 15 or later, the Intel zip macOS 14.** Going lower means bundling those libraries built for an older target instead of Homebrew's.
- **GitHub's arm64 Mac runners are short**: the annotation says "Due to capacity constraints, jobs targeting macOS arm64 runners may experience longer queue times", and twice a `macos-15` job was cancelled after about 15 minutes in the queue with "The job was not acquired by Runner of type hosted even after multiple attempts". A rerun of the job (`gh run rerun <run> --job <job id>`) got a runner. A master push may meet the same; its Windows zip still publishes, without the arm64 one.

**What the log shows**, each item of 04 and 05 (their comments have the lines): the configure's Prism download; `game_access.c` through `tools/gcc_launcher.py`, `gcc-16 (Homebrew GCC 16.2.0)` and the object's `DW_AT_producer ("GNU C11 16.2.0 ...")`; `@executable_path` among `melee`'s rpaths; `Contents/Frameworks` with `libprism.dylib` and no libtolk; `@rpath/libprism.dylib` in melee and as the bundled copy's id; the licences; `Apple rollback sections: game globals included, engine/audio globals excluded`.

**The unit tests** run on both Mac runners: 38 of 38, the fork's suites and the base port's that build off Linux (`launcher_data`, failing on Windows, passes there). They found a fork bug first: `voice_settings`, `accessibility_settings` and `switches` made their scratch folder at `<build>/<suite>_test`, which off Windows is the test executable itself, and aborted. The folders are now `<suite>_files`, in their own commit.

**Verified** on Windows at the quick level for the `a11y.cmake` and parser changes: build (the "Rollback sections" line as before), `check_style.py`, the unit tests (37 of 38, `launcher_data` the known base port failure), and `title` runs of 400 frames, one reading a hand-written `a11y.ini` (`cues.volume = 80%` read as 80, `0x10` refused as before).

**Docs**: CLAUDE.md's CI paragraph, the `verify` skill ("Mac build", and the new test) and the `base-merge` skill name `a11y.yml`, the Mac zips and `package_macos.sh`; the `verify` skill says how to check a Mac build on a topic branch. The spec's open question on the friend's macOS names the zip's floor.

**Left open, for the maintainer**:

- Once master is pushed: drop `macos-ci` from the workflow's push branches and delete the branch, and check that the first master run publishes all three zips to `ci-latest`.
- The friend's macOS version against the zip's floor (15 on Apple Silicon, 14 on Intel). Older than that is a follow-up.

For the issues after this one:

- **07** gets its Mac compile checked the same way: commit on master, `git push origin master:macos-ci` with the maintainer's leave (the push runs the workflow while master on GitHub lacks it; once it has it, `gh workflow run a11y.yml --ref <branch>`), `gh run watch <id> --exit-status`, and `gh run view <id> --log-failed` on a failure. An arm64 job stuck in the queue is GitHub's capacity: rerun the job. The Intel job builds with Xcode 16.4, as the arm64 one now does; anything in the C++ library newer than macOS 13.3 fails to compile there.
- **08**: the zip comes from `ci-latest` only once master is pushed; until then from the run's artifacts. The arm64 zip needs macOS 15 or later (Intel, 14), set by the Homebrew dylibs it bundles: check the friend's version first. The zip unpacks to a folder, `melee-macos-arm64/`, holding `Melee.app`, `melee-netcheck` and `LICENSES.prism/`; `xattr -dr com.apple.quarantine melee-macos-arm64` clears the quarantine from both programs at once.
