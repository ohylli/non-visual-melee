# 06 The fork's CI builds the Mac package

Status: ready-for-agent
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
