#!/usr/bin/env python3
"""Splits the CD's MOVIES/CLAW.FEC into its Smacker movies (movie00.smk ...). Usage: extract_fec.py CLAW.FEC out_dir
The FEC container holds plain Smacker 2 streams, each preceded by a small encrypted header record (not needed)."""
import mmap, struct, sys, pathlib
src, out = sys.argv[1], pathlib.Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
with open(src, 'rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
    pos, n = 0, 0
    while True:
        i = m.find(b'SMK2', pos)
        if i < 0: break
        w, h, frames = struct.unpack('<3I', m[i+4:i+16])
        if not (16 <= w <= 1024 and 16 <= h <= 800 and 1 <= frames < 100000):
            pos = i + 4; continue
        trees = struct.unpack('<I', m[i+52:i+56])[0]
        sizes = struct.unpack('<%dI' % frames, m[i+104:i+104+4*frames])
        total = 104 + 5*frames + trees + sum(s & ~3 for s in sizes)
        (out / f'movie{n:02d}.smk').write_bytes(m[i:i+total])
        print(f'movie{n:02d}.smk {w}x{h} {frames} frames, {total} bytes')
        n += 1; pos = i + total
print(n, 'movies')
