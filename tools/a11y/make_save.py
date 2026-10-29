#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
r"""Make a memory card save for a test, edited from a fresh one.

Windows only. Writes a save file (a .gci, the emulated card's file) that
drive.py --card loads, or starts the maintainer's own game on it.

  python tools/a11y/make_save.py challenger NAME [--out GCI]
  python tools/a11y/make_save.py challenger NAME --play [--dry-run]
  python tools/a11y/make_save.py show GCI

Edits:

  challenger NAME   NAME comes after the next VS match: every other
                    character that can be unlocked is unlocked, and the VS
                    match count is set to NAME's threshold. NAME is one of
                    the 11 (Jigglypuff, Dr. Mario, Pichu, Falco, Marth,
                    Young Link, Ganondorf, Mewtwo, Luigi, Roy, Mr. Game &
                    Watch), in any case, spaces and punctuation optional.
                    The game checks the count after every VS match with a
                    human, a match quit from the pause included, which
                    leaves the count as it was: so every VS match brings
                    NAME, run after run, until NAME is beaten.

show prints what the edits touch in a save, each copy of it.

The edit starts from a fresh save, the one the game makes on an empty card
at "Create Game Data?", kept as build/drive/cache/fresh_save.gci. The first
time it is needed, a drive.py --card run of memcard_create.drive makes it,
silent and in the background, in the shader cache slot --cache-slot names.
Delete it to have it made again, after a base merge that changes the save.
No save is ever committed: its first block holds the game's banner and
icons.

--out names the edited save (default build/drive/challenger.gci for the
challenger, overwritten each time). For a drive run:

  python tools/a11y/make_save.py challenger jigglypuff
  python tools/a11y/drive.py tools/a11y/challenger.drive --card=build/drive/challenger.gci

--play is the maintainer's own test: it writes the save into a test folder,
build/play, with a copy of the maintainer's accessibility settings
(%APPDATA%\melee-pc\a11y.ini), and starts build\melee.exe on the disc
there, as a player's game: with the card, no boot scene, sound, and the
window taking focus. MELEE_USER_DIR and MELEE_CACHE_DIR name the folder to
the game, so the game's settings, card, shader caches and log
(melee-pc.log) are the folder's, and %APPDATA%\melee-pc is only read, for
a11y.ini. Each --play copies the save and a11y.ini in again, so the
challenger comes again even after a win, and a setting changed during the
test is not kept; the rest of the folder stays. An agent never runs
--play: --dry-run makes the folder and prints the command, its folder and
the environment it changes, instead of starting the game.

Layout (src/sysdolphin/baselib/card.c, crypt.c; src/melee/lb/lbcardgame.c;
src/melee/gm/types.h; docs/a11y/native-menus.md, "The save file"):
- A GCI file is a 0x40-byte directory entry, then the file's 0x2000-byte
  sectors. Sector 0 holds the comment, banner and icons.
- Every other sector is one block: a 0x20-byte header (a 16-byte checksum,
  the block id, a sequence number, a file table), then the payload,
  checksummed and scrambled by HSD_Encrypt.
- Block id 1 holds GmSaveData (0x1790 bytes) at payload offset 0, in host
  byte order (little-endian on PC), in two copies on a fresh save; every
  copy is edited.
"""
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
FRESH_SAVE = os.path.join(REPO, "build", "drive", "cache", "fresh_save.gci")
PLAY_DIR = os.path.join(REPO, "build", "play")

# The card cipher, a port of HSD_Encrypt and HSD_Decrypt
# (src/sysdolphin/baselib/crypt.c). A block is sealed from byte 16 on:
# bytes 0 to 15 get the checksum of the rest, then each byte is scrambled
# with the byte before it, already scrambled, as its key.
KEYS = (0x26, 0xFF, 0xE8, 0xEF, 0x42, 0xD6, 0x01, 0x54, 0x14, 0xA3, 0x80, 0xFD, 0x6E)
SECTOR = 0x2000
GCI_ENTRY = 0x40
PAYLOAD = 0x20  # the block header's size
BLOCK_ID = 0x10  # big-endian u16 in the header
SAVE_BLOCK_ID = 1
SAVE_SIZE = 0x1790  # GmSaveData


def checksum(data):
    """The 16-byte sum HSD_Encrypt keeps in a block's first bytes: the
    bytes added in turn to a 16-byte seed, then each byte equal to the one
    before it flipped."""
    m = bytearray((0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                   0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10))
    for i, b in enumerate(data):
        m[i % 16] = (m[i % 16] + b) & 0xFF
    for i in range(1, 16):
        if m[i - 1] == m[i]:
            m[i] ^= 0xFF
    return bytes(m)


def _scramble(prev, cur):
    v = (prev ^ cur ^ KEYS[prev % 13]) & 0xFF
    k = prev % 7
    if k == 0:
        r = ((v & 1) | ((v << 3) & 0x10) | ((v >> 1) & 2) | ((v << 2) & 0x20) | ((v >> 2) & 4)
             | ((v << 1) & 0x40) | ((v >> 3) & 8) | (v & 0x80))
    elif k == 1:
        r = (((v << 3) & 8) | ((v >> 1) & 1) | (v & 4) | ((v << 3) & 0x40) | ((v << 1) & 0x20)
             | ((v >> 1) & 0x10) | ((v << 1) & 0x80) | ((v >> 6) & 2))
    elif k == 2:
        r = (((v << 6) & 0x40) | ((v << 4) & 0x20) | ((v >> 2) & 1) | ((v >> 2) & 2) | ((v >> 1) & 8)
             | ((v << 2) & 0x80) | ((v >> 4) & 4) | ((v >> 3) & 0x10))
    elif k == 3:
        r = (((v << 1) & 2) | ((v << 2) & 8) | ((v << 5) & 0x80) | ((v << 1) & 0x10) | ((v >> 4) & 1)
             | ((v >> 3) & 4) | ((v >> 1) & 0x20) | ((v >> 1) & 0x40))
    elif k == 4:
        r = (((v << 7) & 0x80) | ((v << 1) & 4) | ((v << 3) & 0x20) | ((v >> 3) & 1) | ((v << 2) & 0x40)
             | ((v >> 4) & 2) | ((v >> 2) & 0x10) | ((v >> 4) & 8))
    elif k == 5:
        r = (((v & 1) << 5) | ((v << 5) & 0x40) | ((v << 2) & 0x10) | (v & 8) | ((v << 3) & 0x80)
             | ((v >> 5) & 1) | ((v >> 5) & 2) | ((v >> 5) & 4))
    else:
        r = (((v << 2) & 4) | (v & 2) | ((v & 4) << 4) | ((v << 4) & 0x80) | (v & 0x10) | ((v >> 2) & 8)
             | ((v >> 6) & 1) | ((v >> 2) & 0x20))
    return r & 0xFF


def _unscramble(prev, c):
    k = prev % 7
    if k == 0:
        c = ((c & 1) | ((c << 1) & 4) | ((c << 2) & 0x10) | ((c << 3) & 0x40) | ((c >> 3) & 2)
             | ((c >> 2) & 8) | ((c >> 1) & 0x20) | (c & 0x80))
    elif k == 1:
        c = (((c << 1) & 2) | ((c << 6) & 0x80) | (c & 4) | ((c >> 3) & 1) | ((c << 1) & 0x20)
             | ((c >> 1) & 0x10) | ((c >> 3) & 8) | ((c >> 1) & 0x40))
    elif k == 2:
        c = (((c & 1) << 2) | ((c << 2) & 8) | ((c << 4) & 0x40) | ((c << 1) & 0x10) | ((c << 3) & 0x80)
             | ((c >> 4) & 2) | ((c >> 6) & 1) | ((c >> 2) & 0x20))
    elif k == 3:
        c = (((c << 4) & 0x10) | ((c >> 1) & 1) | ((c << 3) & 0x20) | ((c >> 2) & 2) | ((c >> 1) & 8)
             | ((c << 1) & 0x40) | ((c << 1) & 0x80) | ((c >> 5) & 4))
    elif k == 4:
        c = (((c << 3) & 8) | ((c << 4) & 0x20) | ((c >> 1) & 2) | ((c << 4) & 0x80) | ((c << 2) & 0x40)
             | ((c >> 3) & 4) | ((c >> 2) & 0x10) | ((c >> 7) & 1))
    elif k == 5:
        c = (((c & 1) << 5) | ((c << 5) & 0x40) | ((c & 4) << 5) | (c & 8) | ((c >> 2) & 4)
             | ((c >> 5) & 1) | ((c >> 5) & 2) | ((c >> 3) & 0x10))
    else:
        c = (((c << 6) & 0x40) | (c & 2) | ((c >> 2) & 1) | ((c << 2) & 0x20) | (c & 0x10)
             | ((c << 2) & 0x80) | ((c >> 4) & 4) | ((c >> 4) & 8))
    return ((c & 0xFF) ^ KEYS[prev % 13] ^ prev) & 0xFF


def encrypt(block):
    """HSD_Encrypt: the checksum of block[16:] into block[0:16], then
    block[16:] scrambled."""
    b = bytearray(block)
    b[0:16] = checksum(b[16:])
    for i in range(16, len(b)):
        b[i] = _scramble(b[i - 1], b[i])
    return bytes(b)


def decrypt(block):
    """HSD_Decrypt: (the plain block, whether its checksum holds)."""
    b = bytearray(block)
    prev = b[15]
    for i in range(16, len(b)):
        cur = b[i]
        b[i] = _unscramble(prev, cur)
        prev = cur
    return bytes(b), checksum(b[16:]) == bytes(b[0:16])


def save_copies(data):
    """Each copy of GmSaveData in the GCI bytes data: (sector, plain block),
    the block one whose checksum holds."""
    sectors = (len(data) - GCI_ENTRY) // SECTOR
    for sector in range(1, sectors):
        start = GCI_ENTRY + sector * SECTOR
        plain, ok = decrypt(data[start:start + SECTOR])
        if ok and struct.unpack_from(">H", plain, BLOCK_ID)[0] == SAVE_BLOCK_ID:
            yield sector, plain


def edit_save(data, edit):
    """The GCI bytes data with edit applied to every copy of GmSaveData, and
    the number of copies. edit takes the save's bytes, a bytearray, and
    changes them in place. ValueError if the file holds no copy."""
    out = bytearray(data)
    copies = 0
    for sector, plain in save_copies(data):
        block = bytearray(plain)
        save = block[PAYLOAD:PAYLOAD + SAVE_SIZE]
        edit(save)
        block[PAYLOAD:PAYLOAD + SAVE_SIZE] = save
        sealed = encrypt(bytes(block))
        start = GCI_ENTRY + sector * SECTOR
        out[start:start + SECTOR] = sealed
        copies += 1
    if not copies:
        raise ValueError("no block of the game's save data whose checksum holds: not a save of this game, "
                         "or the cipher no longer matches the game's")
    return bytes(out), copies


# GmSaveData's fields the edits touch (src/melee/gm/types.h).
CHARACTERS = 0x00  # u16, the unlocked characters' bits
VS_MATCHES = 0x08  # u32, VS matches played (unk_8.x0)
VS_SECONDS = 0x0C  # u32, VS play time in seconds (unk_8.x4)
ALL_CHARACTERS = 0x7FF

# The 11 characters that can be unlocked: name, unlock bit (lbl_803B78C8,
# gm_1601.c) and the VS match count that brings them (lbl_803B7AD0,
# gm_16F1.c). After a VS match, gm_80172DD4 brings the locked character
# with the lowest threshold at or below the count.
CHALLENGERS = (
    ("Mr. Game & Watch", 0, 1000), ("Luigi", 1, 800), ("Marth", 2, 400), ("Mewtwo", 3, 700),
    ("Jigglypuff", 4, 50), ("Falco", 5, 300), ("Young Link", 6, 500), ("Dr. Mario", 7, 100),
    ("Roy", 8, 900), ("Pichu", 9, 200), ("Ganondorf", 10, 600),
)


def _key(name):
    """name without case, spaces, punctuation, "and" or a leading "Mr", so
    "Mr. Game & Watch" and "gameandwatch" are one."""
    key = re.sub(r"[^a-z0-9]", "", name.lower()).replace("and", "")
    return key[2:] if key.startswith("mr") else key


def challenger_name(name):
    """The challenger NAME means, as CHALLENGERS spells it; ValueError for
    none."""
    for known, _, _ in CHALLENGERS:
        if name and _key(name) == _key(known):
            return known
    raise ValueError(f"{name!r} is no challenger; one of: " + ", ".join(n for n, _, _ in CHALLENGERS))


def challenger_edit(name):
    """The edit that makes NAME come after the next VS match."""
    _, bit, threshold = next(c for c in CHALLENGERS if c[0] == challenger_name(name))

    def edit(save):
        struct.pack_into("<H", save, CHARACTERS, ALL_CHARACTERS & ~(1 << bit))
        struct.pack_into("<I", save, VS_MATCHES, threshold)

    return edit


def describe(save):
    characters, = struct.unpack_from("<H", save, CHARACTERS)
    matches, seconds = struct.unpack_from("<II", save, VS_MATCHES)
    locked = [n for n, bit, _ in CHALLENGERS if not characters & (1 << bit)]
    return (f"locked: {', '.join(locked) or 'none'}; VS matches {matches}; "
            f"VS play time {seconds // 3600}:{seconds // 60 % 60:02}:{seconds % 60:02}")


def make_fresh_save(cache_slot):
    """FRESH_SAVE, made by the game the first time: memcard_create.drive on
    the run's own empty card answers Yes to "Create Game Data?". Its
    transcript and settings folder are removed once the save is out, and
    kept for a look if the run fails."""
    if os.path.isfile(FRESH_SAVE):
        return FRESH_SAVE
    out = os.path.join(REPO, "build", "drive", f"fresh_save.{cache_slot}.log")
    print(f">> making the fresh save: drive.py memcard_create.drive --card, transcript {out}", flush=True)
    code = subprocess.call([sys.executable, os.path.join(HERE, "drive.py"), "--card=",
                            os.path.join(HERE, "memcard_create.drive"), "--out", out, "--cache-slot", cache_slot],
                           cwd=REPO, stdin=subprocess.DEVNULL)
    folder = os.path.join(out + ".settings", "USA", "Card A")
    made = [os.path.join(folder, f) for f in os.listdir(folder)] if os.path.isdir(folder) else []
    if code != 0 or len(made) != 1:
        sys.exit(f">> FAILED: no fresh save made (drive.py exit {code}, {len(made)} file(s) in {folder}); "
                 f"see {out}")
    os.makedirs(os.path.dirname(FRESH_SAVE), exist_ok=True)
    # Through a temporary name, so a run beside this one never reads half a file.
    shutil.copyfile(made[0], FRESH_SAVE + ".part")
    os.replace(FRESH_SAVE + ".part", FRESH_SAVE)
    for path in (out, out + ".game.log"):
        if os.path.exists(path):
            os.remove(path)
    shutil.rmtree(out + ".settings", ignore_errors=True)
    print(f">> fresh save: {FRESH_SAVE}", flush=True)
    return FRESH_SAVE


def read(path):
    with open(path, "rb") as f:
        return f.read()


def show(path):
    copies = list(save_copies(read(path)))
    if not copies:
        sys.exit(f"{path}: no block of the game's save data")
    for sector, plain in copies:
        print(f"sector {sector}: {describe(plain[PAYLOAD:PAYLOAD + SAVE_SIZE])}")


def play(data, disc, dry_run):
    """The maintainer's test: the save and a copy of their a11y.ini into
    PLAY_DIR, and their game started there, or with dry_run what would
    start printed."""
    import drive  # Windows only; the edits above need none of it

    os.makedirs(PLAY_DIR, exist_ok=True)
    region, _ = drive.card_save_place(data)
    # The card's folder emptied first: the save copied in again resets it.
    shutil.rmtree(os.path.join(PLAY_DIR, region, "Card A"), ignore_errors=True)
    save = drive.put_card_save(PLAY_DIR, data)
    print(f"save: {save}")
    appdata = os.environ.get("APPDATA")
    own_ini = os.path.join(appdata, "melee-pc", "a11y.ini") if appdata else None
    ini = os.path.join(PLAY_DIR, "a11y.ini")
    if own_ini and os.path.isfile(own_ini):
        shutil.copyfile(own_ini, ini)
        print(f"accessibility settings: {ini}, copied from {own_ini}")
    else:
        if os.path.exists(ini):
            os.remove(ini)
        print(f"accessibility settings: the defaults, no {own_ini or 'APPDATA'}")

    exe = os.path.join(REPO, "build", "melee.exe")
    command = [exe, os.path.join(REPO, disc)]
    cwd = os.path.dirname(exe)  # resources/ beside melee.exe, as a player's copy has it
    changes = {"MELEE_USER_DIR": PLAY_DIR, "MELEE_CACHE_DIR": PLAY_DIR,
               "MELEE_LOG_FILE": os.path.join(PLAY_DIR, "melee-pc.log")}
    # Each would undo the test: a boot scene skips loading the card and
    # later overwrites its save, and another a11y.ini than the copy.
    removed = ("MELEE_BOOT_SCENE", "MELEE_A11Y_SETTINGS")
    env = {k: v for k, v in os.environ.items() if k not in removed}
    env.update(changes)
    if dry_run:
        print("dry run, the game not started:")
        print("  command: " + subprocess.list2cmdline(command))
        print(f"  folder: {cwd}")
        for k, v in changes.items():
            print(f"  set {k}={v}")
        for k in removed:
            print(f"  unset {k}" + (" (was set)" if k in os.environ else ""))
        return
    if not os.path.isfile(exe):
        sys.exit(f"no {exe}: build the game first")
    # Its own process group, so Ctrl+C in this terminal leaves the game be;
    # its console output beside the log, never in the terminal.
    with open(os.path.join(PLAY_DIR, "console.log"), "wb") as console:
        subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL, stdout=console,
                         stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
    print("game started")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="what", required=True)
    shown = sub.add_parser("show", help="print what the edits touch in a save")
    shown.add_argument("gci", nargs="?", default=FRESH_SAVE, help="the save (default the fresh save)")
    chal = sub.add_parser("challenger", help="NAME comes after the next VS match")
    chal.add_argument("name", help="the challenger, one of the 11 (jigglypuff, mewtwo, ...)")
    # The options every edit takes.
    for p in (chal,):
        p.add_argument("--out", help="the edited save (default build/drive/<edit>.gci)")
        p.add_argument("--fresh", help="the save to edit (default the fresh save, made if missing)")
        p.add_argument("--cache-slot", default="1",
                       help="the drive.py shader cache slot for making the fresh save (default 1)")
        p.add_argument("--play", action="store_true",
                       help="the maintainer's test: the save into build/play and the game started there")
        p.add_argument("--dry-run", action="store_true", help="with --play, print what would start instead")
        p.add_argument("--disc", default="melee.iso", help="with --play, the disc, relative to the repo root")
    args = ap.parse_args()
    sys.stdout.reconfigure(errors="replace")

    if args.what == "show":
        show(args.gci)
        return
    if args.dry_run and not args.play:
        ap.error("--dry-run is --play's")
    if args.play and args.out:
        ap.error("--play writes the save into build/play; --out is for a drive run's save")
    try:
        name = challenger_name(args.name)
    except ValueError as e:
        ap.error(str(e))
    fresh = args.fresh or make_fresh_save(args.cache_slot)
    try:
        data, copies = edit_save(read(fresh), challenger_edit(name))
    except (OSError, ValueError) as e:
        sys.exit(f">> FAILED: {fresh}: {e}")
    copy = next(save_copies(data))[1]
    print(f"{name} comes after the next VS match ({describe(copy[PAYLOAD:PAYLOAD + SAVE_SIZE])}; "
          f"{copies} copies edited)")
    if args.play:
        play(data, args.disc, args.dry_run)
        return
    out = args.out or os.path.join(REPO, "build", "drive", f"{args.what}.gci")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        f.write(data)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
