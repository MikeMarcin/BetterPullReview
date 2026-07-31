#!/usr/bin/env python3
"""Build a single Markdown dump of the BetterPullReview project source."""

from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


TEXT_EXTENSIONS = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".css",
    ".h",
    ".hpp",
    ".in",
    ".ipp",
    ".json",
    ".md",
    ".patch",
    ".py",
    ".qrc",
    ".sh",
    ".sql",
    ".toml",
    ".txt",
    ".ui",
    ".xml",
    ".yaml",
    ".yml",
}
TEXT_FILE_NAMES = {
    ".clang-format",
    ".clang-tidy",
    ".editorconfig",
    ".gitattributes",
    ".gitignore",
    "CMakeLists.txt",
    "LICENSE",
    "Makefile",
    "NEXT_ID",
    "NOTICE",
    "README",
}
ALWAYS_SKIP_DIRS = {
    ".git",
    ".pytest_cache",
    "__pycache__",
}
GENERATED_SKIP_DIRS = {
    ".cache",
    "_deps",
    "artifacts",
    "build",
    "CMakeFiles",
    "coverage",
    "dist",
    "out",
    "Testing",
}
GENERATED_SKIP_PREFIXES = ("cmake-build-",)
SKIP_FILE_NAMES = {
    ".DS_Store",
    "CMakeCache.txt",
    "CTestTestfile.cmake",
    "compile_commands.json",
    "cmake_install.cmake",
    "install_manifest.txt",
}
PREFERRED_FRONT = (
    "README.md",
    "AGENTS.md",
    "CONTRIBUTING.md",
    "docs/architecture/product-contract.md",
    "docs/architecture/system-architecture.md",
    "docs/rfcs/README.md",
    "docs/issues/README.md",
    "CMakeLists.txt",
    "CMakePresets.json",
    ".gitignore",
    ".gitattributes",
)


@dataclass(frozen=True)
class DumpFile:
    path: Path
    rel_path: str
    kind: str


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description="Create a single Markdown project-source dump."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=repo_root,
        help="Repository root. Defaults to the parent of this script's directory.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("artifacts/better_pull_review_project_source.md"),
        help="Output Markdown file, relative to --root unless absolute.",
    )
    parser.add_argument(
        "--stdout",
        action="store_true",
        help="Write the dump to stdout instead of --output.",
    )
    parser.add_argument(
        "--include-generated",
        action="store_true",
        help="Include generated directories that are skipped by default.",
    )
    return parser.parse_args(argv)


def resolve_under(base: Path, path: Path) -> Path:
    if path.is_absolute():
        return path.resolve()
    return (base / path).resolve()


def rel_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def is_skipped(
    path: Path,
    root: Path,
    include_generated: bool,
    exclude_paths: set[Path],
) -> bool:
    if path.resolve() in exclude_paths:
        return True

    try:
        parts = path.relative_to(root).parts
    except ValueError:
        parts = path.parts

    if any(part in ALWAYS_SKIP_DIRS for part in parts):
        return True
    if not include_generated and any(
        part in GENERATED_SKIP_DIRS
        or part.startswith(GENERATED_SKIP_PREFIXES)
        for part in parts
    ):
        return True
    return path.name in SKIP_FILE_NAMES


def is_text_source(path: Path) -> bool:
    return path.suffix.lower() in TEXT_EXTENSIONS or path.name in TEXT_FILE_NAMES


def classify(path: Path, root: Path) -> str:
    try:
        parts = path.relative_to(root).parts
    except ValueError:
        return "project"

    if not parts:
        return "project"

    top = parts[0]
    if top == ".agents":
        return "agent"
    if top == "cmake":
        return "build"
    if top == "docs":
        return "docs"
    if top == "packaging":
        return "packaging"
    if top == "scripts":
        return "script"
    if top == "src":
        return "source"
    if top == "tests":
        return "test"
    if len(parts) == 1 and path.name in {
        "AGENTS.md",
        "CONTRIBUTING.md",
        "README.md",
    }:
        return "repository-context"
    if len(parts) == 1:
        return "project-config"
    return "project"


def discover_project_files(
    root: Path,
    include_generated: bool,
    exclude_paths: set[Path],
) -> list[Path]:
    files: list[Path] = []
    for current_dir, dirnames, filenames in os.walk(root):
        current_path = Path(current_dir)
        dirnames[:] = [
            dirname
            for dirname in dirnames
            if not (current_path / dirname).is_symlink()
            and not is_skipped(
                current_path / dirname,
                root,
                include_generated,
                exclude_paths,
            )
        ]

        for filename in filenames:
            path = current_path / filename
            if path.is_symlink() or is_skipped(
                path,
                root,
                include_generated,
                exclude_paths,
            ):
                continue
            if path.is_file() and is_text_source(path):
                files.append(path.resolve())

    file_set = set(files)
    ordered: list[Path] = []
    seen: set[Path] = set()
    for relative in PREFERRED_FRONT:
        path = (root / relative).resolve()
        if path in file_set:
            ordered.append(path)
            seen.add(path)

    ordered.extend(
        sorted(file_set - seen, key=lambda path: rel_path(path, root))
    )
    return ordered


def language_for(path: Path) -> str:
    return {
        ".c": "c",
        ".cc": "cpp",
        ".cmake": "cmake",
        ".cpp": "cpp",
        ".css": "css",
        ".h": "cpp",
        ".hpp": "cpp",
        ".in": "text",
        ".ipp": "cpp",
        ".json": "json",
        ".md": "markdown",
        ".patch": "diff",
        ".py": "python",
        ".qrc": "xml",
        ".sh": "bash",
        ".sql": "sql",
        ".toml": "toml",
        ".txt": "text",
        ".ui": "xml",
        ".xml": "xml",
        ".yaml": "yaml",
        ".yml": "yaml",
    }.get(path.suffix.lower(), "text")


def fence_for(text: str) -> str:
    longest = 0
    current = 0
    for character in text:
        if character == "`":
            current += 1
            longest = max(longest, current)
        else:
            current = 0
    return "`" * max(3, longest + 1)


def line_count(text: str) -> int:
    if not text:
        return 0
    return text.count("\n") + (0 if text.endswith("\n") else 1)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def render_dump(files: list[DumpFile], root: Path) -> str:
    generated_at = datetime.now().astimezone().isoformat(timespec="seconds")
    total_bytes = sum(file.path.stat().st_size for file in files)
    counts_by_kind: dict[str, int] = {}
    for file in files:
        counts_by_kind[file.kind] = counts_by_kind.get(file.kind, 0) + 1
    kind_summary = ", ".join(
        f"{kind}: {count}" for kind, count in sorted(counts_by_kind.items())
    )

    lines = [
        "# BetterPullReview Project Source",
        "",
        f"Generated: {generated_at}",
        f"Repository root: `{root}`",
        f"Included files: {len(files)} ({kind_summary})",
        f"Included bytes: {total_bytes}",
        "",
        "This dump is intended for ChatGPT project sources. It contains the current",
        "BetterPullReview source, tests, documentation, and project tooling while",
        "excluding generated output, caches, and binary artifacts.",
        "",
        "## File Manifest",
        "",
        "```text",
    ]
    lines.extend(f"{file.rel_path} [{file.kind}]" for file in files)
    lines.extend(["```", "", "## Files", ""])

    for file in files:
        text = read_text(file.path)
        fence = fence_for(text)
        lines.extend(
            [
                f"### {file.rel_path}",
                "",
                f"Kind: `{file.kind}`",
                f"Lines: {line_count(text)}",
                "",
                f"{fence}{language_for(file.path)}",
                text.rstrip("\n"),
                fence,
                "",
            ]
        )

    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    output = resolve_under(root, args.output)

    if not root.is_dir():
        print(f"error: repository root not found: {root}", file=sys.stderr)
        return 2
    if not (root / "CMakeLists.txt").is_file() or not (root / "src").is_dir():
        print(
            f"error: expected a BetterPullReview repository under: {root}",
            file=sys.stderr,
        )
        return 2

    try:
        source_paths = discover_project_files(
            root,
            include_generated=args.include_generated,
            exclude_paths={output},
        )
        if not source_paths:
            raise ValueError(f"no project source files found under {root}")
        files = [
            DumpFile(
                path=path,
                rel_path=rel_path(path, root),
                kind=classify(path, root),
            )
            for path in source_paths
        ]
        dump = render_dump(files, root)

        if args.stdout:
            sys.stdout.write(dump)
            sys.stdout.write("\n")
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(dump + "\n", encoding="utf-8")
            print(f"wrote {output}")
            print(f"included {len(files)} files")
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
