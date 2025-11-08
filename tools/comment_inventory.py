#!/usr/bin/env python3
"""
Collect all source-code comments within the repository and emit a location-index
report. The script understands a handful of common comment syntaxes so we can
build an “archeology” map for the revived Wayverb codebase.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, List, Sequence, Tuple


# Extensions that use C-like // and /* */ comments.
C_LIKE_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".m",
    ".mm",
    ".metal",
    ".cl",
    ".glsl",
    ".js",
    ".ts",
    ".tsx",
    ".cs",
    ".java",
    ".swift",
    ".rs",
    ".cu",
    ".inl",
}

# Extensions where # starts a comment.
HASH_EXTENSIONS = {
    ".py",
    ".pyi",
    ".sh",
    ".bash",
    ".zsh",
    ".ps1",
    ".rb",
    ".pl",
    ".pm",
    ".pm6",
    ".t",
    ".yml",
    ".yaml",
    ".toml",
    ".ini",
    ".cfg",
    ".conf",
    ".cmake",
    ".txt",
    ".rst",
    ".md",
    ".dockerfile",
    ".mk",
    ".am",
    ".ninja",
}

# TeX / LaTeX percentage comments.
PERCENT_EXTENSIONS = {
    ".tex",
    ".cls",
    ".sty",
}

# Lisp / Scheme style semicolon comments.
SEMICOLON_EXTENSIONS = {
    ".scm",
    ".lisp",
    ".el",
}


@dataclass
class CommentEntry:
    path: Path
    line: int
    text: str


def run_git_ls(root: Path) -> List[Path]:
    """Return all tracked files under the repository root."""
    try:
        result = subprocess.run(
                ["git", "ls-files"],
                check=True,
                capture_output=True,
                cwd=root,
                text=True,
        )
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f"git ls-files failed: {exc}") from exc
    files = [root / Path(line.strip()) for line in result.stdout.splitlines() if line.strip()]
    return files


def is_binary(path: Path) -> bool:
    """Heuristic binary detection (large amount of zero bytes)."""
    try:
        with path.open("rb") as fh:
            chunk = fh.read(8192)
    except OSError:
        return True
    text_chars = bytearray({7, 8, 9, 10, 12, 13, 27} | set(range(0x20, 0x100)))
    if b"\0" in chunk:
        return True
    if chunk.translate(None, text_chars):
        return False
    return False


def detect_styles(path: Path) -> Sequence[str]:
    ext = path.suffix.lower()
    styles: List[str] = []
    if ext in C_LIKE_EXTENSIONS:
        styles.append("c_like")
    if ext in HASH_EXTENSIONS or path.name.lower() == "cmakelists.txt":
        styles.append("hash")
    if ext in PERCENT_EXTENSIONS:
        styles.append("percent")
    if ext in SEMICOLON_EXTENSIONS:
        styles.append("semicolon")
    if not styles:
        # Fallback for scripts without extension (e.g. Dockerfile, Makefile).
        basename = path.name.lower()
        if basename in {"makefile", "dockerfile"}:
            styles.append("hash")
    return styles


def iter_comments(path: Path) -> Iterator[CommentEntry]:
    styles = detect_styles(path)
    if not styles:
        return
    try:
        with path.open(encoding="utf-8", errors="ignore") as fh:
            lines = fh.readlines()
    except (OSError, UnicodeDecodeError):
        return

    in_block = False
    block_start = 0
    block_buffer: List[str] = []

    for idx, line in enumerate(lines, start=1):
        stripped = line.rstrip("\n")

        if "c_like" in styles:
            # Handle block comments.
            if in_block:
                block_buffer.append(stripped)
                if "*/" in stripped:
                    before, _, after = stripped.partition("*/")
                    block_buffer[-1] = before
                    comment_text = " ".join(seg.strip() for seg in block_buffer if seg.strip())
                    if comment_text:
                        yield CommentEntry(path, block_start, comment_text)
                    in_block = False
                    block_buffer = []
                    # There might be a trailing // comment after */ so continue scanning.
                    trailing = after.strip()
                    if trailing.startswith("//"):
                        yield CommentEntry(path, idx, trailing[2:].strip())
                continue
            if "/*" in stripped:
                before, _, after = stripped.partition("/*")
                if after:
                    block_start = idx
                    in_block = True
                    block_buffer = [after]
                    if "*/" in after:
                        # Single-line block comment.
                        head, _, tail = after.partition("*/")
                        block_buffer = [head]
                        comment_text = head.strip()
                        if comment_text:
                            yield CommentEntry(path, idx, comment_text)
                        in_block = False
                        block_buffer = []
                        trailing = tail.strip()
                        if trailing.startswith("//"):
                            yield CommentEntry(path, idx, trailing[2:].strip())
                    continue
            # Line comments.
            pos = stripped.find("//")
            if pos >= 0 and not stripped.startswith("#include") and not stripped.startswith("#define"):
                if pos > 0:
                    prefix = stripped[:pos]
                    if prefix.strip().endswith("http:") or prefix.strip().endswith("https:"):
                        pass
                comment = stripped[pos + 2 :].strip()
                if comment:
                    yield CommentEntry(path, idx, comment)
        if "hash" in styles:
            stripped_ws = stripped.lstrip()
            if stripped_ws.startswith("#"):
                comment = stripped_ws.lstrip("#").strip()
                if comment:
                    yield CommentEntry(path, idx, comment)
        if "percent" in styles:
            stripped_ws = stripped.lstrip()
            if stripped_ws.startswith("%"):
                comment = stripped_ws.lstrip("%").strip()
                if comment:
                    yield CommentEntry(path, idx, comment)
        if "semicolon" in styles:
            stripped_ws = stripped.lstrip()
            if stripped_ws.startswith(";"):
                comment = stripped_ws.lstrip(";").strip()
                if comment:
                    yield CommentEntry(path, idx, comment)


def format_markdown(entries: Sequence[CommentEntry]) -> str:
    lines: List[str] = []
    lines.append("# Comment Inventory")
    lines.append("")
    lines.append(f"Generated on demand from {len(entries)} comment entries.")
    lines.append("")
    current_path: Path | None = None
    for entry in entries:
        if entry.path != current_path:
            current_path = entry.path
            rel_path = entry.path.as_posix()
            lines.append(f"## `{rel_path}`")
        lines.append(f"- L{entry.line}: {entry.text}")
    lines.append("")
    return "\n".join(lines)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
            description="Build a repository-wide index of source comments.")
    parser.add_argument(
            "--root",
            type=Path,
            default=Path(os.getcwd()),
            help="Repository root (default: cwd).",
    )
    parser.add_argument(
            "--output",
            type=Path,
            default=Path("analysis/comment_inventory.md"),
            help="Destination file for the comment inventory.",
    )
    args = parser.parse_args(argv)

    root = args.root.resolve()
    files = run_git_ls(root)
    entries: List[CommentEntry] = []
    for path in files:
        if not path.exists() or path.is_dir():
            continue
        if is_binary(path):
            continue
        for entry in iter_comments(path):
            entries.append(entry)

    # Sort to keep things deterministic.
    entries.sort(key=lambda e: (str(e.path), e.line))

    output_text = format_markdown(entries)
    output_path = args.output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(output_text, encoding="utf-8")
    print(f"Wrote {len(entries)} entries to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
