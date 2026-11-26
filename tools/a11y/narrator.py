"""Speaks a drive script's say steps through the player's own screen reader,
for listen runs (drive.py --listen): the same Prism library the game ships,
loaded from build/ with ctypes, its best backend (a running screen reader
first, then a system voice), as the game's speech bridge picks it
(src/pc/a11y/screen_reader_bridge.cpp). Windows only, standard library only.
"""

import ctypes
import os
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Seconds a word takes at a brisk screen reader rate, and a pause after,
# so the script's next press does not cut the line off. Generous: a listen
# run is paced for a person.
SECONDS_PER_WORD = 0.32
PAUSE_AFTER = 0.6


class PrismConfig(ctypes.Structure):
    # prism.h, v0.18.2.
    _fields_ = [
        ("version", ctypes.c_uint8),
        ("registry", ctypes.c_void_p),
        ("availability_callback", ctypes.c_void_p),
        ("availability_userdata", ctypes.c_void_p),
        ("availability_poll_interval_ms", ctypes.c_uint32),
        ("availability_debounce_samples", ctypes.c_uint32),
        ("availability_backoff_max_ms", ctypes.c_uint32),
        ("availability_auto_power_manage", ctypes.c_bool),
    ]


class Narrator:
    """Prism's best backend; say() speaks a line and waits about as long as
    it takes to say."""

    def __init__(self, build_dir=None):
        build_dir = build_dir or os.path.join(REPO, "build")
        os.add_dll_directory(build_dir)
        lib = ctypes.CDLL(os.path.join(build_dir, "prism.dll"))
        lib.prism_config_init.restype = PrismConfig
        lib.prism_init.restype = ctypes.c_void_p
        lib.prism_init.argtypes = [ctypes.POINTER(PrismConfig)]
        lib.prism_registry_create_best.restype = ctypes.c_void_p
        lib.prism_registry_create_best.argtypes = [ctypes.c_void_p]
        lib.prism_backend_name.restype = ctypes.c_char_p
        lib.prism_backend_name.argtypes = [ctypes.c_void_p]
        lib.prism_backend_output.restype = ctypes.c_int
        lib.prism_backend_output.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_bool]
        lib.prism_error_string.restype = ctypes.c_char_p
        lib.prism_error_string.argtypes = [ctypes.c_int]
        self.lib = lib
        config = lib.prism_config_init()
        self.context = lib.prism_init(ctypes.byref(config))
        if not self.context:
            raise RuntimeError("Prism did not start")
        self.backend = lib.prism_registry_create_best(self.context)
        if not self.backend:
            raise RuntimeError("Prism found no screen reader or voice")
        self.name = lib.prism_backend_name(self.backend).decode("utf-8", "replace")

    def say(self, text, wait=True):
        """Speak text, cutting off what the screen reader was saying, then
        wait about as long as it takes to say. Returns an error text, or
        None."""
        error = self.lib.prism_backend_output(self.backend, text.encode("utf-8"), True)
        if error != 0:
            return self.lib.prism_error_string(error).decode("utf-8", "replace")
        if wait:
            time.sleep(len(text.split()) * SECONDS_PER_WORD + PAUSE_AFTER)
        return None


if __name__ == "__main__":
    import sys

    narrator = Narrator()
    print(f"narrator: {narrator.name}")
    print(narrator.say(" ".join(sys.argv[1:]) or "Narrator test.") or "said")
