import struct, sys
d = open(sys.argv[1], 'rb').read()
hdr = struct.unpack_from('<H', d, 8)[0] * 16
img = d[hdr:]
egasize, mapsize, audsize = [int(x) for x in sys.argv[2:5]]
def find_all(b):
    i = img.find(b); r = []
    while i >= 0: r.append(i); i = img.find(b, i + 1)
    return r
# EGAHEAD: 3-byte offsets ending in file size
for end in find_all(egasize.to_bytes(3, 'little')):
    # walk back to a 000000 entry on 3-byte stride
    i = end
    while i >= 3 and img[i-3:i] != b'\0\0\0': i -= 3
    if i >= 3:
        n = (end - (i - 3)) // 3 + 1
        print('EGAHEAD candidate at %05X, %d entries' % (i - 3, n))
for p in find_all(audsize.to_bytes(4, 'little')):
    i = p
    while i >= 4 and struct.unpack_from('<I', img, i - 4)[0] != 0: i -= 4
    print('AUDIOHED candidate at %05X, %d entries' % (i - 4, (p - (i - 4)) // 4 + 1))
for p in find_all(b'\xcd\xab'):
    off = struct.unpack_from('<I', img, p + 2)[0]
    if 0 < off < mapsize: print('MAPHEAD candidate at %05X first map ptr %d' % (p, off))
