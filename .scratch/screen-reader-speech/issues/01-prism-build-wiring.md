# 01 Prism build wiring in a11y.cmake

Status: resolved (2026-09-23)
Type: task

Create `src/pc/a11y/a11y.cmake` and include it from the root `CMakeLists.txt` with a single line, next to the other `include(cmake/...)` lines. The base port's file gets no other change.

On Windows, `a11y.cmake`:

- Downloads Prism's release zip `prism-windows-x64.zip` for the pinned tag (v0.18.2 at the time of writing) with `file(DOWNLOAD ... EXPECTED_HASH SHA256=...)` into the build directory, and extracts it with `file(ARCHIVE_EXTRACT)`. Skips the download when the extracted tree is already present, so re-configuring offline works. The tag and the hash sit together at the top of the file as the one place to bump.
- Verify the zip's layout at implementation time (expected: `dist/{dynamic,static}/{release,debug}` plus `include/`). Use the dynamic release variant: the static one needs delay loading on the consumer side, which MinGW lacks.
- Adds the include directory, links `melee` against the import library from the zip (GCC's linker accepts MSVC import libraries; if it does not for this file, generate one with `gendef` and `dlltool` from the DLL, both in MSYS2), and copies `prism.dll` beside `melee.exe` with a POST_BUILD `copy_if_different`, the same idiom the root file uses for `nod.dll`.
- Adds the fork sources (`hooks.cpp`, `speech.cpp`, `screen_reader_bridge.cpp`) to the `melee` target with `target_sources`, and `src/pc/a11y` to its include path.
- Defines `A11Y_HAVE_PRISM` on the `melee` target (`target_compile_definitions`). This macro, not `_WIN32`, is what selects the real bridge body in issue 02.

On other platforms: fork sources are still added, no download, no link, no `A11Y_HAVE_PRISM`; the bridge compiles as a stub (issue 02).

Keep the platform choice in one place: a variable such as `A11Y_PRISM_ASSET` (the zip name, `prism-windows-x64.zip` today) set per platform at the top of the file, empty when the platform has no Prism wiring yet. Prism publishes `prism-macos-universal.zip`, `prism-linux-x64.zip` and `prism-linux-arm64.zip` for the same tag, so a later macOS or Linux port should be a new branch that sets the asset, the library file name and the runtime copy destination (the macOS app bundle's Frameworks folder), with no change to the fork's C++.

Done when: `cmake -B build -G Ninja` succeeds with and without network on the second run, `cmake --build build` links, `build/prism.dll` exists beside `melee.exe`, and `python tools/check_style.py` passes.

Notes: Prism is MPL-2.0. The fork's toolchain cannot build Prism from source (see ADR-0001 and the spec).

## Comments

2026-09-23, implementation: the zip has no `dist/` level. It is `include/`, `dynamic/{release,debug}/{bin,lib}`, `static/...`, `LICENSES/`, `NOTICE`. `dynamic/release/bin/prism.dll` imports only Windows system DLLs, so it is the only runtime file copied (the zip's `tolk.dll` is a compatibility shim nothing needs). GNU ld links the MSVC `prism.lib` directly, so no `gendef`/`dlltool`. The include line sits at the end of the root `CMakeLists.txt`, not next to the other `include(cmake/...)` lines: `a11y.cmake` needs both the `melee` and `unit_tests` targets, and `unit_tests` is created near the end. The sha256 was computed from the release download on 2026-09-23; Prism publishes no checksum file.
