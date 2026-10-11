#!/usr/bin/env python3
"""Disassemble the code segments of the unpacked Keen 4 exe into per-segment listings.

Function starts come from far-call relocations, near-call targets and `push bp; mov bp,sp`
prologues. Immediate operands that point at C strings in DGROUP are annotated, and
`Quit("XX_Name: ...")` style messages are used to suggest function names.
"""
import json, os, re, struct, sys, collections
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

exe, outdir = sys.argv[1], sys.argv[2]
d = open(exe, 'rb').read()
img = d[struct.unpack_from('<H', d, 8)[0] * 16:]
nrel, relo = struct.unpack_from('<H', d, 6)[0], struct.unpack_from('<H', d, 0x18)[0]
relocs = [struct.unpack_from('<HH', d, relo + 4 * i) for i in range(nrel)]
DGROUP = 0x2EE7            # from c0 startup: mov dx,2EE7h
CODE_END = 0x1ED3          # first non-code segment value referenced
dbase = DGROUP * 16

# far-call targets -> code segments
far = collections.defaultdict(set)
reloc_lin = set()
for off, seg in relocs:
    lin = seg * 16 + off
    reloc_lin.add(lin)
    if lin >= 3 and img[lin - 3] == 0x9A:
        far[struct.unpack_from('<H', img, lin)[0]].add(struct.unpack_from('<H', img, lin - 2)[0])
segs = sorted(s for s in far if s < CODE_END)

# C strings in DGROUP
strings = {}
dg = img[dbase:]
for m in re.finditer(rb'[\x20-\x7e\n\r\t]{4,}\x00', dg):
    if m.start() == 0 or dg[m.start() - 1] == 0:
        strings[m.start()] = m.group()[:-1].decode('latin1')

md = Cs(CS_ARCH_X86, CS_MODE_16)
md.skipdata = True
names = {}   # (seg, off) -> name
funcs = {}
os.makedirs(outdir, exist_ok=True)

def imm_refs(ins):
    # only `push imm16` and `mov reg16, imm16` can carry a string offset; skip tiny values
    if ins.mnemonic == 'push' or (ins.mnemonic == 'mov' and re.match(r'(ax|bx|cx|dx|si|di), 0x', ins.op_str)):
        for v in re.findall(r'0x([0-9a-f]+)$', ins.op_str):
            v = int(v, 16)
            if v >= 0x100: yield v

def str_at(v):
    if v in strings: return strings[v]
    return None

for i, seg in enumerate(segs):
    end = (segs[i + 1] if i + 1 < len(segs) else CODE_END) * 16
    base = seg * 16
    code = img[base:end]
    starts = set(far[seg])
    for m in re.finditer(rb'\x55\x8b\xec', code):
        starts.add(m.start())
    insns = list(md.disasm(code, 0))
    for ins in insns:
        if ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
            t = int(ins.op_str, 16)
            if t < len(code): starts.add(t)
    starts = sorted(s for s in starts if s < len(code))
    funcs[seg] = starts
    # name suggestions from "XX_Name:" strings referenced inside each function
    for ins in insns:
        for v in imm_refs(ins):
            s = str_at(v)
            if s:
                m = re.match(r'([A-Za-z][A-Za-z0-9_]{3,})\s*(?:\(\))?\s*(?::|-)', s)
                if m and ('_' in m.group(1) or sum(c.isupper() for c in m.group(1)) >= 2) and not m.group(1).isupper():
                    fs = max([f for f in starts if f <= ins.address], default=None)
                    if fs is not None: names.setdefault((seg, fs), m.group(1))

# hand-assigned names override the automatic ones: lines of `SSSS:OOOO Name`
symfile = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'symbols.txt')
if os.path.exists(symfile):
    for line in open(symfile):
        line = line.split('#')[0].split()
        if len(line) >= 2:
            sg, of = line[0].split(':')
            names[(int(sg, 16), int(of, 16))] = line[1]

def fname(seg, off):
    return names.get((seg, off), 'sub_%04X_%04X' % (seg, off))

summary = []
for i, seg in enumerate(segs):
    end = (segs[i + 1] if i + 1 < len(segs) else CODE_END) * 16
    base = seg * 16
    code = img[base:end]
    starts = set(funcs[seg])
    refs = collections.Counter()
    lines = ['; segment %04X  linear %05X-%05X  (%d bytes, %d functions)' % (seg, base, end, len(code), len(starts)), '']
    for ins in md.disasm(code, 0):
        if ins.address in starts:
            lines.append('')
            lines.append('%s:%s' % (fname(seg, ins.address), '   ; far entry' if ins.address in far[seg] else ''))
        op = ins.op_str
        note = ''
        lin = base + ins.address
        if ins.mnemonic == 'lcall' and ins.size == 5 and (lin + 3) in reloc_lin:
            o, s = struct.unpack_from('<HH', code, ins.address + 1)
            op = fname(s, o); note = 'far'
        elif ins.mnemonic == 'call' and op.startswith('0x'):
            op = fname(seg, int(op, 16))
        for v in imm_refs(ins):
            s = str_at(v)
            if s and len(s) >= 4:
                note = (note + ' ' if note else '') + repr(s[:60]); refs[s[:40]] += 1
        lines.append('  %04X  %-22s %-7s %s%s' % (ins.address, ins.bytes.hex(), ins.mnemonic, op,
                                                 ('  ; ' + note) if note else ''))
    open(os.path.join(outdir, 'seg_%04X.asm' % seg), 'w').write('\n'.join(lines) + '\n')
    named = sorted(set(names.get((seg, f)) for f in starts if (seg, f) in names))
    prefixes = collections.Counter(n.split('_')[0] for n in named)
    summary.append({'seg': '%04X' % seg, 'linear': '%05X' % base, 'bytes': len(code),
                    'functions': len(starts), 'far_entries': len(far[seg]),
                    'named': named, 'prefixes': dict(prefixes),
                    'sample_strings': [s for s, _ in refs.most_common(6)]})
json.dump({'DGROUP': '%04X' % DGROUP, 'segments': summary,
           'names': {'%04X:%04X' % k: v for k, v in sorted(names.items())}},
          open(os.path.join(outdir, 'summary.json'), 'w'), indent=1)
for s in summary:
    print(s['seg'], s['bytes'], s['functions'], s['prefixes'], s['sample_strings'][:3])
