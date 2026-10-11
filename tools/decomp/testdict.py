import struct, sys
d = open(sys.argv[1], 'rb').read(); img = d[struct.unpack_from('<H', d, 8)[0] * 16:]
g = open(sys.argv[2], 'rb').read()
head = img[0x21080:0x21080 + 4752 * 3]
offs = [int.from_bytes(head[3*i:3*i+3], 'little') for i in range(4752)]
def decode(dictp, src, outlen):
    nodes = [struct.unpack_from('<HH', img, dictp + 4 * i) for i in range(255)]
    node, out, bits = 254, bytearray(), 0
    for byte in src:
        for b in range(8):
            v = nodes[node][(byte >> b) & 1]
            if v < 256:
                out.append(v); node = 254
                if len(out) == outlen: return out, True
            else: node = v - 256
    return out, False
for dp in [int(x, 16) for x in sys.argv[3:]]:
    ok = 0; tried = 0
    for c in range(0, 500, 5):
        if offs[c] == 0xFFFFFF or offs[c+1] == 0xFFFFFF: continue
        blob = g[offs[c]:offs[c+1]]
        if len(blob) < 5: continue
        n = struct.unpack_from('<I', blob, 0)[0]
        if n > 70000: continue
        _, done = decode(dp, blob[4:], n); tried += 1; ok += done
    print('dict %05X: %d/%d chunks decode to full length' % (dp, ok, tried))
