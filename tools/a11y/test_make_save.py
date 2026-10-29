#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""make_save.py's card cipher and edits against a hand-made GCI file.

No real save is used: a save's first block holds the game's banner and
icons, so none is committed. The cipher's match with the game's is checked
on every real save instead: make_save.py finds the save's block only where
its checksum holds once decrypted, and refuses a file with none. Run by
ctest, or alone: python tools/a11y/test_make_save.py
"""

import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_save  # noqa: E402

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def block(block_id, payload):
    """A plain data block: a zero checksum, the header and the payload."""
    out = bytearray(make_save.SECTOR)
    struct.pack_into(">H", out, 0x10, block_id)
    out[make_save.PAYLOAD:make_save.PAYLOAD + len(payload)] = payload
    return bytes(out)


def gci(sectors):
    """A GCI file of the US save's entry and the given plain blocks after a
    banner sector, each encrypted as the game writes it."""
    entry = bytearray(0x40)
    entry[0:4] = b"GALE"
    entry[4:6] = b"01"
    entry[8:8 + len(b"SuperSmashBros0110290334")] = b"SuperSmashBros0110290334"
    struct.pack_into(">H", entry, 0x38, len(sectors) + 1)
    rng = random.Random(1)
    banner = bytes(rng.randrange(256) for _ in range(make_save.SECTOR))
    return bytes(entry) + banner + b"".join(make_save.encrypt(s) for s in sectors)


def save_payload(characters, matches, seconds):
    save = bytearray(make_save.SAVE_SIZE)
    struct.pack_into("<H", save, 0, characters)
    struct.pack_into("<II", save, 8, matches, seconds)
    save[0x100] = 0x5A  # something past the edited fields, to stay put
    return bytes(save)


def test_cipher():
    rng = random.Random(2)
    plain = bytes(16) + bytes(rng.randrange(256) for _ in range(make_save.SECTOR - 16))
    sealed = make_save.encrypt(plain)
    check(sealed[16:] != plain[16:], "encrypt leaves the block as it was")
    opened, ok = make_save.decrypt(sealed)
    check(ok, "a block just encrypted fails its checksum")
    check(opened[16:] == plain[16:], "decrypt does not undo encrypt")
    check(make_save.encrypt(opened) == sealed, "encrypting a decrypted block changes it")
    spoilt = bytearray(sealed)
    spoilt[0x100] ^= 0xFF
    check(not make_save.decrypt(bytes(spoilt))[1], "a spoilt block passes its checksum")


def test_checksum_neighbours():
    # HSD_Encrypt's checksum flips a byte equal to the one before it.
    m = make_save.checksum(bytes(64))
    check(all(m[i - 1] != m[i] for i in range(1, 16)), "checksum keeps two equal bytes side by side")


def test_challenger_edit():
    fresh_save = save_payload(0x0000, 3, 120)
    other = block(2, bytes(range(256)) * 4)
    data = gci([block(1, fresh_save), other, block(1, fresh_save)])
    edited, copies = make_save.edit_save(data, make_save.challenger_edit("Jigglypuff"))
    check(copies == 2, f"{copies} copies of the save edited, not 2")
    check(len(edited) == len(data), "the file changed size")
    check(edited[:0x40 + make_save.SECTOR] == data[:0x40 + make_save.SECTOR],
          "the entry or the banner changed")
    second = 0x40 + 2 * make_save.SECTOR
    check(edited[second:second + make_save.SECTOR] == data[second:second + make_save.SECTOR],
          "a block other than the save changed")
    for sector in (1, 3):
        start = 0x40 + sector * make_save.SECTOR
        plain, ok = make_save.decrypt(edited[start:start + make_save.SECTOR])
        check(ok, f"sector {sector} fails its checksum after the edit")
        save = plain[make_save.PAYLOAD:make_save.PAYLOAD + make_save.SAVE_SIZE]
        characters, = struct.unpack_from("<H", save, 0)
        matches, seconds = struct.unpack_from("<II", save, 8)
        check(characters == 0x7FF & ~(1 << 4), f"sector {sector}: characters {characters:#x}")
        check(matches == 50, f"sector {sector}: {matches} VS matches, not Jigglypuff's 50")
        check(seconds == 120, f"sector {sector}: VS play time changed to {seconds}")
        check(save[0x100] == 0x5A, f"sector {sector}: the rest of the save changed")


def test_mewtwo_and_names():
    check(make_save.challenger_name("mewtwo") == "Mewtwo", "mewtwo not found")
    check(make_save.challenger_name("Dr. Mario") == "Dr. Mario", "Dr. Mario not found")
    check(make_save.challenger_name("mr game & watch") == "Mr. Game & Watch", "Mr. Game & Watch not found")
    for name in ("Fox", "Kirby", ""):
        try:
            make_save.challenger_name(name)
            check(False, f"{name!r} taken as a challenger")
        except ValueError:
            pass
    save = bytearray(save_payload(0x0000, 0, 0))
    make_save.challenger_edit("Mewtwo")(save)
    characters, = struct.unpack_from("<H", save, 0)
    matches, = struct.unpack_from("<I", save, 8)
    check(characters == 0x7FF & ~(1 << 3), f"Mewtwo: characters {characters:#x}")
    check(matches == 700, f"Mewtwo: {matches} VS matches")


def test_refusals():
    try:
        make_save.edit_save(gci([block(2, b"")]), make_save.challenger_edit("Roy"))
        check(False, "a file with no save block was edited")
    except ValueError:
        pass
    # The save's only block fails its checksum, as every block would if the
    # cipher stopped matching the game's: nothing is written.
    sealed = bytearray(gci([block(1, save_payload(0, 0, 0))]))
    start = 0x40 + make_save.SECTOR
    sealed[start + 0x200] ^= 0x01
    try:
        make_save.edit_save(bytes(sealed), make_save.challenger_edit("Roy"))
        check(False, "a spoilt save was edited")
    except ValueError:
        pass


def main():
    for test in (test_cipher, test_checksum_neighbours, test_challenger_edit, test_mewtwo_and_names,
                 test_refusals):
        test()
    for failure in failures:
        print(f"test_make_save: {failure}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
