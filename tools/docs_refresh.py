#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

def run(*args):
    try:
        subprocess.run(args, check=True)
    except subprocess.CalledProcessError as exc:
        print(f"[docs_refresh] command failed: {exc}", file=sys.stderr)
        return False
    return True

script = Path(__file__).resolve().parents[1] / "tools" / "todo_comment_xref.py"
if not script.exists():
    print(f"[docs_refresh] missing {script}", file=sys.stderr)
    sys.exit(1)

sys.exit(0 if run(sys.executable, str(script)) else 1)
