"""Find EGADICT and dump EGAHEAD/AUDIOHED/MAPHEAD/EGADICT tables from an unpacked Keen 4 exe."""
import struct, sys, os
d = open(sys.argv[1], 'rb').read(); out = sys.argv[2]
img = d[struct.unpack_from('<H', d, 8)[0] * 16:]
def isdict(p):
    seen_leaf = set()
    for i in range(255):
        for v in struct.unpack_from('<HH', img, p + 4 * i):
            if v < 256: seen_leaf.add(v)
            elif not (256 <= v < 256 + i): return False
    return len(seen_leaf) == 256
cands = [p for p in range(0, len(img) - 1020, 2) if isdict(p)]
print('EGADICT candidates:', ['%05X' % p for p in cands])
tables = {'EGAHEAD.CK4': (0x21080, 4752 * 3), 'AUDIOHED.CK4': (0x20DF0, 163 * 4),
          'MAPHEAD.CK4': (0x24830, 0x2A3A0 - 0x24830)}
tables['EGADICT.CK4'] = (0x358F6, 1024)  # verified: decodes 100/100 length-prefixed chunks
os.makedirs(out, exist_ok=True)
for name, (p, n) in tables.items():
    open(os.path.join(out, name), 'wb').write(img[p:p + n]); print(name, '%05X' % p, n)
