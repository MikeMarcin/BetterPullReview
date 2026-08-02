#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shutil
import stat
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Optional, Sequence


HOOK_MARKER = "coding-standard managed pre-commit hook"


def git_output(args: Sequence[str], cwd: Path) -> str:
    completed = subprocess.run(["git", *args], cwd=str(cwd), check=True, stdout=subprocess.PIPE, text=True)
    return completed.stdout.strip()


def existing_backup_line(hook_path: Path) -> Optional[str]:
    if not hook_path.exists():
        return None
    text = hook_path.read_text(encoding="utf-8", errors="replace")
    for line in text.splitlines():
        if line.startswith("BACKUP_HOOK="):
            return line
    return None


def hook_text(backup_line: Optional[str]) -> str:
    if backup_line is None:
        backup_line = 'BACKUP_HOOK=""'

    return f"""#!/bin/sh
# {HOOK_MARKER}
set -eu

{backup_line}
if [ -n "$BACKUP_HOOK" ] && [ -f "$BACKUP_HOOK" ]; then
    "$BACKUP_HOOK" "$@"
fi

REPO_ROOT=$(git rev-parse --show-toplevel)
PYTHON=${{PYTHON:-python3}}
FORMAT_CHECK="$REPO_ROOT/../_coding-standard/scripts/format_check.py"

if [ ! -f "$FORMAT_CHECK" ]; then
    echo "format_check.py not found: $FORMAT_CHECK" >&2
    exit 1
fi

git diff --cached --name-only -z --diff-filter=ACMR -- \\
    '*.h' '*.hpp' '*.hh' '*.hxx' \\
    '*.c' '*.cc' '*.cpp' '*.cxx' \\
    '*.inl' '*.ipp' \\
    | "$PYTHON" "$FORMAT_CHECK" --from-stdin
"""


def install_hook(repo_root: Path) -> Path:
    git_dir_raw = git_output(["rev-parse", "--git-dir"], repo_root)
    git_dir = Path(git_dir_raw)
    if not git_dir.is_absolute():
        git_dir = repo_root / git_dir

    hooks_dir = git_dir / "hooks"
    hooks_dir.mkdir(parents=True, exist_ok=True)
    hook_path = hooks_dir / "pre-commit"

    backup_line = existing_backup_line(hook_path)
    if hook_path.exists():
        current = hook_path.read_text(encoding="utf-8", errors="replace")
        if HOOK_MARKER not in current:
            timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
            backup_path = hooks_dir / f"pre-commit.coding-standard-backup-{timestamp}"
            shutil.copy2(hook_path, backup_path)
            backup_line = f'BACKUP_HOOK="{backup_path}"'

    hook_path.write_text(hook_text(backup_line), encoding="utf-8")
    mode = hook_path.stat().st_mode
    hook_path.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return hook_path


def parse_args(argv: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Install the optional local pre-commit formatting hook.")
    parser.add_argument("--repo", type=Path, default=Path.cwd(), help="Repository root or any path inside it.")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    try:
        repo_root = Path(git_output(["rev-parse", "--show-toplevel"], args.repo)).resolve()
    except (subprocess.CalledProcessError, FileNotFoundError):
        print(f"Not a Git repository: {args.repo}")
        return 2

    hook_path = install_hook(repo_root)
    print(f"Installed local pre-commit hook: {hook_path}")
    print("Uninstall by deleting .git/hooks/pre-commit and restoring any pre-commit.coding-standard-backup-* file if needed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
