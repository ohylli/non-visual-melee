# 01 Prism build wiring in a11y.cmake

Status: ready-for-agent
Type: task

Create `src/pc/a11y/a11y.cmake` and include it from the root `CMakeLists.txt` with a single line, next to the other `include(cmake/...)` lines. The base port's file gets no other change.

On Windows, `a11y.cmake`:

- Downloads Prism's release zip `prism-windows-x64.zip` for the pinned tag (v0.18.2 at the time of writing) with `file(DOWNLOAD ... EXPECTED_HASH SHA256=...)` into the build directory, and extracts it with `file(ARCHIVE_EXTRACT)`. Skips the download when the extracted tree is already present, so re-configuring offline works. The tag and the hash sit together at the top of the file as the one place to bump.
- Verify the zip's layout at implementation time (expected: `dist/{dynamic,static}/{release,debug}` plus `include/`). Use the dynamic release variant: the static one needs delay loading on the consumer side, which MinGW lacks.
- Adds the include directory, links `melee` against the import library from the zip (GCC's linker accepts MSVC import libraries; if it does not for this file, generate one with `gendef` and `dlltool` from the DLL, both in MSYS2), and copies `prism.dll` beside `melee.exe` with a POST_BUILD `copy_if_different`, the same idiom the root file uses for `nod.dll`.
- Adds the fork sources (`hooks.cpp`, `speech.cpp`, `screen_reader_bridge.cpp`) to the `melee` target with `target_sources`, and `src/pc/a11y` to its include path.

On other platforms: fork sources are still added, no download, no link; the bridge compiles as a stub (issue 02).

Done when: `cmake -B build -G Ninja` succeeds with and without network on the second run, `cmake --build build` links, `build/prism.dll` exists beside `melee.exe`, and `python tools/check_style.py` passes.

Notes: Prism is MPL-2.0. The fork's toolchain cannot build Prism from source (see ADR-0001 and the spec).
