# 03 Speech subsystem, hooks and the proof-of-life announcement

Status: ready-for-agent
Type: task
Blocked by: 02

`src/pc/a11y/speech.hpp` and `.cpp` (namespace `a11y`):

- `enum class Mode { interrupt, queue };`
- `struct Config { bool enabled; bool log; };` filled by one function `Config config_from_environment()` reading `MELEE_A11Y` and `MELEE_A11Y_LOG` (`0` means off, anything else or unset means on). This function is the only thing to replace when fork settings storage arrives.
- `class Speech` constructed with a `Config` and a `std::unique_ptr<ScreenReaderBridge>`. Methods: `init()`, `shutdown()`, `announce(std::string_view text, Mode mode)`, `last() const` returning the last announcement (text and mode), `initialized_on_this_thread() const`.
- `init()`: when enabled, calls the bridge's `init` and logs `[a11y] speech backend: <name>` or `[a11y] speech backend: none (silent)`. When disabled, logs `[a11y] speech off (MELEE_A11Y=0)` and never touches the bridge. Records the calling thread's id.
- `announce`: always remembers the announcement. Off the init thread: log `[a11y] speech called off the game thread, dropped: "<text>"` and return. Disabled: log `[a11y] speak <mode> (off): "<text>"` and return. Otherwise log `[a11y] speak <mode>: "<text>"`, call the bridge, and on a non-empty error log `[a11y] speak failed (<error>): "<text>"`.
- `shutdown()`: bridge shutdown when it was initialised, then `[a11y] speech shutdown`.
- Logging goes through `pc_log_line` from `src/pc/pc.h` and is skipped entirely when `Config::log` is false. Quote the text as-is; no escaping needed for a developer log.

Hooks: `src/pc/a11y/a11y_hooks.h` (C header with the usual `extern "C"` guards, `#pragma once`) declaring `void pc_a11y_init(void);` and `void pc_a11y_shutdown(void);`. `src/pc/a11y/hooks.cpp` owns the one `Speech` instance (a function-local static or a namespace-scope object; no global constructor side effects) and implements both. `pc_a11y_init` builds the config from the environment, creates the real bridge, inits, and announces `Non-Visual Melee ready` with `Mode::interrupt`.

Base port edits, one line each, in `src/pc/main.c`: `#include "a11y/a11y_hooks.h"` (this include line is the third and last touch), `pc_a11y_init();` right after `pc_launcher_configure(&config);`, and `pc_a11y_shutdown();` as the first statement of `pc_shutdown_once()`.

Done when: a bounded run

```
SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0 SDL_AUDIO_DRIVER=dummy MELEE_BOOT_SCENE=title MELEE_EXIT_AFTER_FRAMES=300 MELEE_LOG_FILE=<path> build/melee.exe --no-card melee.iso
```

exits cleanly and the log contains `[a11y] speech backend:` and `[a11y] speak interrupt: "Non-Visual Melee ready"` and `[a11y] speech shutdown`; a second run with `MELEE_A11Y=0` logs `speech off` and the `(off)` line; `grep -rn pc_a11y_ src --exclude-dir=a11y` shows exactly the three lines in `main.c`; `python tools/check_style.py` passes.
