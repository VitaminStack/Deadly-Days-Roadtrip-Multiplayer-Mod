"""Sucht direkte Aufrufe von Funktionen in der Exe: callers.py <exe> <rva> [<rva> ...]"""
import struct, sys
d = open(sys.argv[1],'rb').read()
pe = struct.unpack_from('<I',d,0x3c)[0]
n = struct.unpack_from('<H',d,pe+6)[0]; o = struct.unpack_from('<H',d,pe+20)[0]
for i in range(n):
    s = pe+24+o+40*i
    if d[s:s+5]==b'.text':
        vs,va,rs,rp = struct.unpack_from('<IIII',d,s+8); break
t = d[rp:rp+rs]
for tgt in [int(a,16) for a in sys.argv[2:]]:
    hits=[]; i=t.find(b'\xE8')
    while i>=0 and i+5<=len(t):
        r=struct.unpack_from('<i',t,i+1)[0]
        if va+i+5+r==tgt: hits.append(hex(va+i))
        i=t.find(b'\xE8',i+1)
    print(hex(tgt), hits[:30])
