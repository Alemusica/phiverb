#!/usr/bin/env python3
import pathlib
import re
import sys
p = pathlib.Path(".codex/NEED-INFO")
if not p.exists():
    print("[ASK-GATE] Nessuna richiesta aperta.")
    sys.exit(0)
open_files = []
for f in sorted(p.glob("*.md")):
    txt = f.read_text(encoding="utf-8", errors="ignore")
    m = re.search(r"##\s*Resolution(.+)", txt, flags=re.S | re.I)
    if not m or len(m.group(1).strip()) < 10 or "RESOLVED" not in m.group(1).upper():
        open_files.append(f)
if open_files:
    print("[ASK-GATE] Richieste da risolvere:")
    for f in open_files:
        print(" -", f)
    sys.exit(2)
print("[ASK-GATE] OK: nessuna richiesta aperta.")
