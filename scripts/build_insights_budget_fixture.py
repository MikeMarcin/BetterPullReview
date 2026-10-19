#!/usr/bin/env python3
"""Refresh Insights' committed provider recording from overview-labels-v1.

Numeric history is seeded through the real Insights storage port by the fixture
composition; the source recording and nested-folders pack stay unchanged.
"""
from pathlib import Path
import shutil


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    contracts = root / "tests/fixtures/github/contracts/v1"
    shutil.copytree(contracts / "overview-labels-v1", contracts / "insights-budget-v1", dirs_exist_ok=True)


if __name__ == "__main__":
    main()
