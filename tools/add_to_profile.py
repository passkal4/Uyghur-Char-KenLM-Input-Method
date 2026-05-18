#!/usr/bin/env python3
import re
import sys
from pathlib import Path

profile = Path(sys.argv[1])
text = profile.read_text(encoding="utf-8")

if "Name=uyghur-kenlm" in text:
    raise SystemExit(0)

indices = [int(m.group(1)) for m in re.finditer(r"^\[Groups/0/Items/(\d+)\]$", text, re.M)]
next_index = max(indices, default=-1) + 1
entry = f"""
[Groups/0/Items/{next_index}]
# Name
Name=uyghur-kenlm
# Layout
Layout=cn-ug

"""

marker = "[GroupOrder]\n"
if marker in text:
    text = text.replace(marker, entry + marker, 1)
else:
    text = text.rstrip() + "\n" + entry

profile.write_text(text, encoding="utf-8")
