"""HSD memory card block crypto (port of src/sysdolphin/baselib/crypt.c)
and a GCI reader/writer for melee-pc's save file."""
import struct

KEYS = [0x26, 0xFF, 0xE8, 0xEF, 0x42, 0xD6, 0x01, 0x54, 0x14, 0xA3, 0x80, 0xFD, 0x6E]
SECTOR = 0x2000


def checksum(data):
    m = bytearray([0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                   0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10])
    for i, b in enumerate(data):
        m[i % 16] = (m[i % 16] + b) & 0xFF
    for i in range(1, 16):
        if m[i - 1] == m[i]:
            m[i] ^= 0xFF
    return bytes(m)


def _enc(prev, cur):
    v = (prev ^ cur ^ KEYS[prev % 13]) & 0xFF
    k = prev % 7
    if k == 0:
        r = (v & 1) | ((v << 3) & 0x10) | ((v >> 1) & 2) | ((v << 2) & 0x20) | ((v >> 2) & 4) | ((v << 1) & 0x40) | ((v >> 3) & 8) | (v & 0x80)
    elif k == 1:
        r = ((v << 3) & 8) | ((v >> 1) & 1) | (v & 4) | ((v << 3) & 0x40) | ((v << 1) & 0x20) | ((v >> 1) & 0x10) | ((v << 1) & 0x80) | ((v >> 6) & 2)
    elif k == 2:
        r = ((v << 6) & 0x40) | ((v << 4) & 0x20) | ((v >> 2) & 1) | ((v >> 2) & 2) | ((v >> 1) & 8) | ((v << 2) & 0x80) | ((v >> 4) & 4) | ((v >> 3) & 0x10)
    elif k == 3:
        r = ((v << 1) & 2) | ((v << 2) & 8) | ((v << 5) & 0x80) | ((v << 1) & 0x10) | ((v >> 4) & 1) | ((v >> 3) & 4) | ((v >> 1) & 0x20) | ((v >> 1) & 0x40)
    elif k == 4:
        r = ((v << 7) & 0x80) | ((v << 1) & 4) | ((v << 3) & 0x20) | ((v >> 3) & 1) | ((v << 2) & 0x40) | ((v >> 4) & 2) | ((v >> 2) & 0x10) | ((v >> 4) & 8)
    elif k == 5:
        r = ((v & 1) << 5) | ((v << 5) & 0x40) | ((v << 2) & 0x10) | (v & 8) | ((v << 3) & 0x80) | ((v >> 5) & 1) | ((v >> 5) & 2) | ((v >> 5) & 4)
    else:
        r = ((v << 2) & 4) | (v & 2) | ((v & 4) << 4) | ((v << 4) & 0x80) | (v & 0x10) | ((v >> 2) & 8) | ((v >> 6) & 1) | ((v >> 2) & 0x20)
    return r & 0xFF


def _dec(prev, c):
    k = prev % 7
    if k == 0:
        c = (c & 1) | ((c << 1) & 4) | ((c << 2) & 0x10) | ((c << 3) & 0x40) | ((c >> 3) & 2) | ((c >> 2) & 8) | ((c >> 1) & 0x20) | (c & 0x80)
    elif k == 1:
        c = ((c << 1) & 2) | ((c << 6) & 0x80) | (c & 4) | ((c >> 3) & 1) | ((c << 1) & 0x20) | ((c >> 1) & 0x10) | ((c >> 3) & 8) | ((c >> 1) & 0x40)
    elif k == 2:
        c = ((c & 1) << 2) | ((c << 2) & 8) | ((c << 4) & 0x40) | ((c << 1) & 0x10) | ((c << 3) & 0x80) | ((c >> 4) & 2) | ((c >> 6) & 1) | ((c >> 2) & 0x20)
    elif k == 3:
        c = ((c << 4) & 0x10) | ((c >> 1) & 1) | ((c << 3) & 0x20) | ((c >> 2) & 2) | ((c >> 1) & 8) | ((c << 1) & 0x40) | ((c << 1) & 0x80) | ((c >> 5) & 4)
    elif k == 4:
        c = ((c << 3) & 8) | ((c << 4) & 0x20) | ((c >> 1) & 2) | ((c << 4) & 0x80) | ((c << 2) & 0x40) | ((c >> 3) & 4) | ((c >> 2) & 0x10) | ((c >> 7) & 1)
    elif k == 5:
        c = ((c & 1) << 5) | ((c << 5) & 0x40) | ((c & 4) << 5) | (c & 8) | ((c >> 2) & 4) | ((c >> 5) & 1) | ((c >> 5) & 2) | ((c >> 3) & 0x10)
    else:
        c = ((c << 6) & 0x40) | (c & 2) | ((c >> 2) & 1) | ((c << 2) & 0x20) | (c & 0x10) | ((c << 2) & 0x80) | ((c >> 4) & 4) | ((c >> 4) & 8)
    c &= 0xFF
    return (c ^ KEYS[prev % 13] ^ prev) & 0xFF


def encrypt(buf):
    """HSD_Encrypt: checksum of [16:] into [0:16], then chain-scramble [16:]."""
    b = bytearray(buf)
    b[0:16] = checksum(b[16:])
    for i in range(16, len(b)):
        b[i] = _enc(b[i - 1], b[i])
    return bytes(b)


def decrypt(buf):
    """HSD_Decrypt: returns (plain, ok)."""
    b = bytearray(buf)
    prev = b[15]
    for i in range(16, len(b)):
        cur = b[i]
        b[i] = _dec(prev, cur)
        prev = cur
    return bytes(b), checksum(b[16:]) == bytes(b[0:16])
