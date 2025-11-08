#!/usr/bin/env python3
import os
import re
import subprocess
import sys

branch = os.environ.get("GITHUB_HEAD_REF") or subprocess.check_output(
    ["git", "rev-parse", "--abbrev-ref", "HEAD"], text=True).strip()
allowed_common = [
    "include/common/",
    "cmake/",
    "docs/",
    ".github/",
    "scripts/",
    "CMakeLists.txt",
]
rules = {
    r"^rt/": ["src/raytracer/"] + allowed_common,
    r"^dwm/": ["src/waveguide/"] + allowed_common,
}
base = os.environ.get("GITHUB_BASE_REF", "origin/main")
files = subprocess.check_output(
        ["git", "diff", "--name-only", f"{base}...HEAD"],
        text=True).splitlines()
allow = []
for pat, dirs in rules.items():
    if re.search(pat, branch):
        allow = dirs
        break
if not allow:
    sys.exit(0)
for path in [f for f in files if f.strip()]:
    if not any(path == pref or path.startswith(pref) for pref in allow):
        print(f"[scope-check] Branch '{branch}' ha toccato percorso non consentito: {path}")
        sys.exit(1)
print("[scope-check] OK")
