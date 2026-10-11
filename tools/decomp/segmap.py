"""Recover code segments and far-call targets of an unpacked MZ exe from its relocations."""
import struct, sys, collections
d = open(sys.argv[1], 'rb').read()
nrel, hp, = struct.unpack_from('<H', d, 6)[0], struct.unpack_from('<H', d, 8)[0]
relo = struct.unpack_from('<H', d, 0x18)[0]
img = d[hp * 16:]
relocs = [struct.unpack_from('<HH', d, relo + 4 * i) for i in range(nrel)]
segvals = collections.Counter()
targets = collections.defaultdict(set)
for off, seg in relocs:
    lin = seg * 16 + off
    v = struct.unpack_from('<H', img, lin)[0]
    segvals[v] += 1
    if lin >= 3 and img[lin - 3] == 0x9A:
        targets[v].add(struct.unpack_from('<H', img, lin - 2)[0])
code = sorted(targets)
print('distinct segment values referenced:', len(segvals))
print('code segments (far-call targets): %d, far-called functions: %d' % (len(code), sum(len(t) for t in targets.values())))
for i, s in enumerate(code):
    nxt = code[i + 1] if i + 1 < len(code) else None
    size = (nxt - s) * 16 if nxt else 0
    print('seg %04X  linear %05X  size~%6d  far entries %3d' % (s, s * 16, size, len(targets[s])))
data = sorted(set(segvals) - set(code))
print('non-code segment values:', ' '.join('%04X' % s for s in data))
