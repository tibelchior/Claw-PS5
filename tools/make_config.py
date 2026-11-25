#!/usr/bin/env python3
"""Writes the port's first-run config.xml from OpenClaw's stock config (usage: make_config.py stock.xml out.xml)."""
import re, sys
s = open(sys.argv[1]).read()
def tag(name, value):
    global s
    s, n = re.subn(rf'<{name}>[^<]*</{name}>|<{name}>\s*</{name}>', f'<{name}>{value}</{name}>', s)
    assert n, name
s = re.sub(r'<Size width="\d+" height="\d+" />', '<Size width="1728" height="972" />', s)
# 16:9 like the community widescreen mod: the game's coordinates are 640x480 per scale unit, so 1728x972 at scale 2 shows 864x486
# (use 864x486 at scale 1 for the cheapest rendering, 1920x1080 at 2.2222222 for a native 1080p frame)
tag('Scale', '2')
tag('AssetsFolder', '/app0/Claw/')
tag('TempDir', '/app0/data/tmp/')
tag('MusicOn', 'true')
tag('MusicVolume', '20')    # the engine caps it at 20 (percent of the mixer range)
tag('SoundVolume', '80')
open(sys.argv[2], 'w').write(s)
