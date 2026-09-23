# 02 Screen reader bridge

Status: ready-for-agent
Type: task
Blocked by: 01

`src/pc/a11y/screen_reader_bridge.hpp` and `.cpp`: the only fork file that includes `prism.h`.

Interface (namespace `a11y`), small enough for a fake in the unit test:

```cpp
class ScreenReaderBridge {
public:
    virtual ~ScreenReaderBridge() = default;
    // Returns the backend name on success ("NVDA", "SAPI 5", ...), empty when no backend could be created.
    virtual std::string init() = 0;
    virtual void shutdown() = 0;
    // interrupt=true cuts off current speech. Returns an error text on failure, empty on success.
    virtual std::string output(std::string_view utf8, bool interrupt) = 0;
};
std::unique_ptr<ScreenReaderBridge> make_screen_reader_bridge();
```

Real body, compiled when `A11Y_HAVE_PRISM` is defined (issue 01 defines it wherever Prism was downloaded; today that is Windows only): `prism_config_init`, `prism_init`, `prism_registry_create_best`, `prism_backend_output`, `prism_backend_free`, `prism_shutdown`. Look up the backend-name call in the pinned `prism.h` (the header is the source of truth; the API changed between v0.13 and v0.18). Never call `prism_backend_initialize` after `create_best`: Prism reports "Already initialized".

Stub body, compiled otherwise: `init` returns empty, the other calls do nothing.

Gate on `A11Y_HAVE_PRISM`, never on `_WIN32`, and keep the real body free of Windows headers and Windows-only calls. Prism's C API is the same on every platform it ships for, so a later macOS or Linux port should need only the cmake branch from issue 01, and this file should compile unchanged there.

Do not add re-acquire logic on failure (spec: play-test first).

Done when: it compiles on Windows against the downloaded header, and the stub compiles when `A11Y_HAVE_PRISM` is undefined (check by reading, a non-Windows build is not available here).
