# 07 Ship prism.dll in the Windows release zip

Status: needs-triage
Type: task

`tools/package_windows.sh` copies a fixed list of DLLs from the build directory (`dxcompiler.dll`, `SDL3.dll`, `nod.dll`, ...). `prism.dll` is not on it, so a packaged build would stop at startup with "prism.dll was not found". Bounded runs and play-tests from `build/` are unaffected: `a11y.cmake` copies the DLL there.

Needed before the fork's first release. The script is a base port file, so the change should be as small as possible. Options: one added name in the script's DLL loop, or a fork-owned packaging step. Prism's `LICENSES/` and `NOTICE` (extracted to `build/prism-<tag>/`) should ship beside it: MPL-2.0 asks binary distributions to say where the source is.
