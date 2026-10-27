#!/usr/bin/env python3
"""Locate the CLI once; callers retain its absolute path for the session."""
from pathlib import Path
import os
import shutil
import sys


def locate() -> Path | None:
    candidate = shutil.which("bpr")
    if candidate is not None:
        return Path(candidate).resolve(strict=True)
    if sys.platform == "darwin":
        bundled = Path("/Applications/BetterPullReview.app/Contents/MacOS/bpr")
        if bundled.is_file() and os.access(bundled, os.X_OK):
            return bundled.resolve(strict=True)
    return None


def main() -> int:
    candidate = locate()
    if candidate is None:
        print('bpr was not found. Install a macOS BetterPullReview app with local automation in '
              '/Applications, or provide the absolute path to its Contents/MacOS/bpr. '
              'For a terminal: export PATH="/Applications/BetterPullReview.app/Contents/MacOS:$PATH". '
              'See references/setup.md for other locations and the developer fallback.', file=sys.stderr)
        return 1
    print(candidate)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
