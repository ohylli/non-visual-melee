# 04 Speech unit test with a fake bridge

Status: resolved (2026-09-23)
Type: task
Blocked by: 03

`src/pc/a11y/test_speech.cpp`, registered in `a11y.cmake` the way the root file registers `tools/test_*.cpp`: `add_executable(speech_test EXCLUDE_FROM_ALL ...)` with `speech.cpp` and the test file only (not the real bridge, not `main.c`), `-UNDEBUG`, `add_test(NAME speech ...)`, label `melee`, `add_dependencies(unit_tests speech_test)`. Same plain assert style as the existing tests, no framework.

The test file defines `extern "C" void pc_log_line(const char* fmt, ...)` itself, formatting into a vector of captured lines, and a `FakeBridge` recording every `output` call with its flag and returning a configurable error.

Cases:

- interrupt and queue reach the bridge with the right flag, in order.
- log line text: `[a11y] speak interrupt: "hello"` exactly.
- disabled config: bridge never called, log shows `speech off` and `speak interrupt (off): "hello"`.
- log disabled: no lines captured, bridge still called.
- bridge init returning empty: `speech backend: none (silent)` logged, later announces still call the bridge (Prism decides; the fork does not second-guess).
- bridge error on output: `speak failed (boom): "hello"` logged.
- `last()` returns the most recent announcement, also when disabled.
- announce from a `std::thread`: bridge not called, dropped line logged.

Done when: `cmake --build build --target unit_tests` then `ctest --test-dir build -L melee` shows `speech` passing (with the MSYS2 `bin` on `PATH`, see CLAUDE.local.md). `launcher_data` failing is the known base port bug, not this issue.
