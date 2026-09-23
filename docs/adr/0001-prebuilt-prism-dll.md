# Use Prism's prebuilt Windows DLL instead of building it from source

Status: accepted (2026-09-23)

The fork speaks through the Prism screen reader library. Prism's Windows build needs Microsoft's MIDL tool for its NVDA stubs and relies on MSVC delay loading; its own CMake warns that under MinGW "Prism will malfunction", and its CI builds Windows with MSVC only. The fork must build with GCC from MSYS2 because the decomp layer requires it. So the fork does not build Prism at all: `src/pc/a11y/a11y.cmake` downloads the pinned release zip at configure time, links the MSVC-built `prism.dll` through its import library (the exported surface is plain C with opaque pointers, so the compiler mismatch does not matter) and copies the DLL beside `melee.exe`.

## Considered options

- **Build Prism from source with GCC.** Not supported by Prism; would need MIDL and a delay-load shim.
- **An MSVC ExternalProject, as the Starship fork does.** Works, but demands Visual Studio and the Windows SDK next to MSYS2 for every contributor, and a second toolchain to keep healthy.
- **Vendor the DLL in git.** Offline builds, but a multi-megabyte binary updated by hand.

## Consequences

- `prism.dll` is a required runtime file for everyone, sighted players included, like SDL's DLL. A missing DLL stops the game with the standard Windows dialog, which a screen reader reads; that was chosen over runtime loading so a blind player never mistakes a broken install for "speech is off".
- The first configure needs network access. Bumping Prism is one tag and one hash in `a11y.cmake`.
- The fork depends on Prism continuing to publish Windows release zips.
