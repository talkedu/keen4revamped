#!/usr/bin/env python3
"""Unpack an LZEXE 0.91 compressed DOS MZ executable."""
import struct, sys

def unpack(data):
    hdr = struct.unpack_from('<14H', data, 0)
    assert data[0:2] == b'MZ' and data[0x1C:0x20] == b'LZ91', 'not LZEXE 0.91'
    hdrsize = hdr[4] * 16
    img = data[hdrsize:]
    ip0, cs0 = hdr[10], hdr[11]
    lzbase = cs0 * 16
    ip, cs, sp, ss, cpsize, incsize, decsize, csum = struct.unpack_from('<8H', img, lzbase)
    # relocations
    p = lzbase + 0x158
    relocs, seg, off = [], 0, 0
    while True:
        span = img[p]; p += 1
        if span == 0:
            span = struct.unpack_from('<H', img, p)[0]; p += 2
            if span == 0:
                seg += 0x0FFF; continue
            if span == 1:
                break
        off += span
        seg += (off & ~0x0F) >> 4
        off &= 0x0F
        relocs.append((off, seg))
    # decompress: compressed data sits at (cs0 - cpsize)*16
    src = (cs0 - cpsize) * 16
    out = bytearray()
    state = {'p': src, 'buf': 0, 'cnt': 0}
    def getw():
        v = struct.unpack_from('<H', img, state['p'])[0]; state['p'] += 2; return v
    def getb():
        v = img[state['p']]; state['p'] += 1; return v
    state['buf'] = getw(); state['cnt'] = 16
    def bit():
        b = state['buf'] & 1
        state['cnt'] -= 1
        if state['cnt'] == 0:
            state['buf'] = getw(); state['cnt'] = 16
        else:
            state['buf'] >>= 1
        return b
    while True:
        if bit():
            out.append(getb()); continue
        if not bit():
            ln = (bit() << 1 | bit()) + 2
            span = getb() | 0xFF00
        else:
            span = getb(); ln = getb()
            span |= ((ln & ~7) << 5) | 0xE000
            ln = (ln & 7) + 2
            if ln == 2:
                ln = getb()
                if ln == 0: break
                if ln == 1: continue
                ln += 1
        span -= 0x10000
        for _ in range(ln):
            out.append(out[span])
    # build new MZ
    nrel = len(relocs)
    hdr_bytes = 0x1C + nrel * 4
    hdr_paras = (hdr_bytes + 0x1FF) // 0x200 * 0x20  # pad to 512
    total = hdr_paras * 16 + len(out)
    newh = struct.pack('<14H', 0x5A4D, total % 512, (total + 511) // 512, nrel,
                       hdr_paras, hdr[5] - (incsize + (len(img) - len(out)) // 16) if False else hdr[5],
                       0xFFFF, ss, sp, 0, ip, cs, 0x1C, 0)
    rel = b''.join(struct.pack('<HH', o, s) for o, s in relocs)
    h = (newh + rel).ljust(hdr_paras * 16, b'\0')
    return h + bytes(out), (ip, cs, sp, ss, nrel, len(out))

if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    exe, info = unpack(d)
    open(sys.argv[2], 'wb').write(exe)
    print('ip=%04X cs=%04X sp=%04X ss=%04X relocs=%d image=%d bytes' % info)
