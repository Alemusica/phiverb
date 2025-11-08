#!/usr/bin/env python3
"""
Compare two comment inventory JSON files and print a summary of additions/removals.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import List, Dict, Tuple


def load(path: Path) -> List[Dict[str, str]]:
    with path.open(encoding="utf-8") as fh:
        return json.load(fh)


def as_key(entry: Dict[str, str]) -> Tuple[str, int, str]:
    return entry["path"], int(entry["line"]), entry["text"]


def main():
    parser = argparse.ArgumentParser(
            description="Diff comment_inventory.json snapshots.")
    parser.add_argument(
            "--old",
            type=Path,
            required=True,
            help="Older snapshot (JSON).",
    )
    parser.add_argument(
            "--new",
            type=Path,
            required=True,
            help="Newer snapshot (JSON).",
    )
    args = parser.parse_args()

    old_entries = load(args.old)
    new_entries = load(args.new)

    old_set = Counter([as_key(e) for e in old_entries])
    new_set = Counter([as_key(e) for e in new_entries])

    added = [entry for entry in new_set.elements() if old_set[entry] == 0]
    removed = [entry for entry in old_set.elements() if new_set[entry] == 0]

    print(f"Old entries: {len(old_entries)}")
    print(f"New entries: {len(new_entries)}")
    print(f"Added: {len(added)}")
    print(f"Removed: {len(removed)}")

    def fmt(entries):
        for path, line, text in entries[:20]:
            print(f"+ {path}:{line} — {text}")
        if len(entries) > 20:
            print(f"... ({len(entries) - 20} more)")

    if added:
        print("\nSample additions:")
        fmt(added)
    if removed:
        print("\nSample removals:")
        for path, line, text in removed[:20]:
            print(f"- {path}:{line} — {text}")
        if len(removed) > 20:
            print(f"... ({len(removed) - 20} more)")


if __name__ == "__main__":
    main()
