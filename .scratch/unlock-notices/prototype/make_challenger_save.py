"""Edit a melee-pc memory card save (GCI) so the next VS match brings a
challenger.

  python make_challenger_save.py FRESH.gci OUT_DIR [--challenger NAME]
         [--matches N] [--vs-seconds S] [--show]

--challenger NAME picks which of the 11 comes (default jigglypuff): every
other unlockable character is marked unlocked and the VS match count is set
to NAME's threshold. gmVsMelee_ExitResults checks the count after every VS
match with a human, even one quit from the pause (No contest), which leaves
the count unchanged: so every VS match brings NAME, run after run, until
the challenger is beaten. --matches N sets the count instead.

FRESH.gci is a save the game made itself (Create Game Data on an empty
card). OUT_DIR gets USA/Card A/<same file name>, ready to be copied into a
MELEE_USER_DIR (or used as DRIVE_CARD_SEED).

Layout (src/sysdolphin/baselib/card.c, crypt.c; src/melee/lb/lbcardgame.c):
- GCI = 0x40-byte directory entry, then the file's 0x2000-byte sectors.
- Sector 0 holds the comment, banner and icons. Each data sector is one HSD
  block: a 0x20-byte header ([0:16] checksum, [0x10:0x12] block id,
  [0x12] sequence number, [0x13:0x1F] a table of 3 file entries) then the
  payload. The whole block (from its header to the sector's end) is
  scrambled by HSD_Encrypt, whose checksum is a 16-byte running byte sum
  seeded with the MD5 init words, over bytes [16:].
- Block id 1 carries file 1, the GmSaveData (0x1790 bytes), at payload
  offset 0; the PC port writes it in host byte order (little-endian).
  A fresh save holds two copies of block 1 (sectors 1 and 10); both are
  edited.
GmSaveData fields used (src/melee/gm/types.h):
- 0x00 u16 unlocked_characters
- 0x08 u32 unk_8.x0  VS matches played; gm_80172DD4 compares it with
  lbl_803B7AD0 (gm_16F1.c): 50 matches brings Jigglypuff.
- 0x0C u32 unk_8.x4  combined VS play time (s); 72000 brings Mewtwo
  (gm_80172D78).
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hsdcard import SECTOR, decrypt, encrypt  # noqa: E402

SAVE_SIZE = 0x1790
PAYLOAD = 0x20

# unlock bit (lbl_803B78C8, gm_1601.c) and VS match threshold (lbl_803B7AD0,
# gm_16F1.c); gm_80172DD4 brings the locked character with the lowest
# threshold at or below the count.
CHALLENGERS = {
    'gameandwatch': (0, 1000), 'luigi': (1, 800), 'marth': (2, 400),
    'mewtwo': (3, 700), 'jigglypuff': (4, 50), 'falco': (5, 300),
    'younglink': (6, 500), 'drmario': (7, 100), 'roy': (8, 900),
    'pichu': (9, 200), 'ganondorf': (10, 600),
}


def blocks(gci):
    data = gci[0x40:]
    for s in range(len(data) // SECTOR):
        plain, ok = decrypt(data[s * SECTOR:(s + 1) * SECTOR])
        if ok:
            yield s, plain


def show(save, label):
    chars, stages, feats = struct.unpack_from('<HHB', save, 0)
    vs_matches, vs_time = struct.unpack_from('<II', save, 8)
    print('%s: chars %04x stages %04x features %02x vs matches %d vs time %d s'
          % (label, chars, stages, feats, vs_matches, vs_time))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('fresh')
    ap.add_argument('out_dir')
    ap.add_argument('--challenger', default=None, choices=sorted(CHALLENGERS))
    ap.add_argument('--matches', type=int, default=None)
    ap.add_argument('--vs-seconds', type=int, default=None)
    ap.add_argument('--show', action='store_true', help='only print the save')
    a = ap.parse_args()

    chars = None
    if a.challenger is None and a.matches is None:
        a.challenger = 'jigglypuff'
    if a.challenger is not None:
        bit, threshold = CHALLENGERS[a.challenger]
        chars = 0x7FF & ~(1 << bit)
        if a.matches is None:
            a.matches = threshold

    gci = bytearray(open(a.fresh, 'rb').read())
    found = 0
    for s, plain in blocks(gci):
        if (plain[0x10] << 8 | plain[0x11]) != 1:
            continue
        found += 1
        block = bytearray(plain)
        save = block[PAYLOAD:PAYLOAD + SAVE_SIZE]
        show(save, 'sector %d before' % s)
        if a.show:
            continue
        if chars is not None:
            struct.pack_into('<H', block, PAYLOAD, chars)
        if a.matches is not None:
            struct.pack_into('<I', block, PAYLOAD + 8, a.matches)
        if a.vs_seconds is not None:
            struct.pack_into('<I', block, PAYLOAD + 0x0C, a.vs_seconds)
        show(block[PAYLOAD:PAYLOAD + SAVE_SIZE], 'sector %d after ' % s)
        enc = encrypt(bytes(block))
        dec, ok = decrypt(enc)
        assert ok and dec[16:] == bytes(block[16:])
        off = 0x40 + s * SECTOR
        gci[off:off + SECTOR] = enc
    if not found:
        sys.exit('no block 1 (save data) found')
    if a.show:
        return
    name = os.path.basename(a.fresh)
    if not name.startswith('01-GALE-'):
        name = '01-GALE-SuperSmashBros0110290334.gci'
    out = os.path.join(a.out_dir, 'USA', 'Card A')
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, name), 'wb').write(gci)
    print('wrote', os.path.join(out, name))


if __name__ == '__main__':
    main()
