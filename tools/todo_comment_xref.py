#!/usr/bin/env python3
import os
import re
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ["src", "include", "scripts", "metal", "tests", "cmake"]
EXCLUDE = {".git", "build", "out", ".github", ".vscode", "external", "third_party", "vendor", "__pycache__"}
PAT = re.compile(r'\b(TODO|FIXME|TBD|HACK|XXX)\b', re.IGNORECASE)


def scan():
    items = []
    for base in INCLUDE:
        p = ROOT / base
        if not p.exists():
            continue
        for dp, ds, fs in os.walk(p):
            ds[:] = [d for d in ds if d not in EXCLUDE]
            for fn in fs:
                fp = Path(dp) / fn
                try:
                    text = fp.read_text(encoding="utf-8", errors="ignore")
                except Exception:
                    continue
                rel = fp.relative_to(ROOT).as_posix()
                for i, line in enumerate(text.splitlines(), 1):
                    if PAT.search(line):
                        items.append((rel, i, line.strip()))
    return items


def replace_block(text, start, end, payload):
    a = text.find(start)
    b = text.find(end)
    if a == -1 or b == -1 or b < a:
        return text
    return text[: a + len(start)] + "\n" + payload.strip() + "\n" + text[b:]


def main():
    items = scan()
    Path(ROOT / "analysis").mkdir(exist_ok=True)

    summary = ["# Comment/TODO Summary", f"Total: **{len(items)}**", ""]
    for rel, i, line in items[:200]:
        summary.append(f"- `{rel}:{i}` {line}")
    (ROOT / "analysis" / "comment_summary.md").write_text("\n".join(summary), encoding="utf-8")

    arch = ROOT / "docs" / "archeology.md"
    text = arch.read_text(encoding="utf-8")
    table_rows = "\n".join(f"| `{rel}:{i}` | {line} |" for rel, i, line in items[:30] or [("—", 0, "—")])
    table = "| Percorso | Esempio |\n|---|---|\n" + table_rows
    text = re.sub(
        r"(<!--XREF-DATE-->).*(<!--/XREF-DATE-->)",
        r"\g<1>" + time.strftime("%Y-%m-%d %H:%M:%S") + r"\g<2>",
        text,
    )
    text = replace_block(text, "<!-- BEGIN AUTO-XREF -->", "<!-- END AUTO-XREF -->", table)
    arch.write_text(text, encoding="utf-8")

    ap = ROOT / "docs" / "action_plan.md"
    ap_text = ap.read_text(encoding="utf-8")
    inbox = "\n".join(f"- `{rel}:{i}` {line}" for rel, i, line in items[:50]) or "_(vuoto)_"
    ap_text = replace_block(ap_text, "<!-- BEGIN INBOX -->", "<!-- END INBOX -->", inbox)
    ap.write_text(ap_text, encoding="utf-8")

    print(f"[xref] items={len(items)}")


if __name__ == "__main__":
    main()
