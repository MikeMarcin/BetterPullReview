#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
PROTOCOL_HEADER = (
    REPOSITORY_ROOT
    / "src"
    / "application"
    / "include"
    / "bpr"
    / "application"
    / "git_protocol_v2.hpp"
)
PROTOCOL_SOURCE = (
    REPOSITORY_ROOT / "src" / "application" / "src" / "git_protocol_v2.cpp"
)
NATIVE_SMART_TRANSPORT_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "github"
    / "src"
    / "native_https_transport.cpp"
)
RAW_HELPERS = (
    "discover_git_protocol_v2",
    "list_git_protocol_v2_refs",
    "fetch_git_protocol_v2",
)


def production_sources() -> list[Path]:
    source_suffixes = {
        ".c",
        ".cc",
        ".cpp",
        ".cxx",
        ".h",
        ".hh",
        ".hpp",
        ".hxx",
    }
    return sorted(
        path
        for path in (REPOSITORY_ROOT / "src").rglob("*")
        if path.suffix in source_suffixes
    )


class GitProtocolBoundaryTests(unittest.TestCase):
    def test_only_complete_acquisition_is_public(self) -> None:
        header = PROTOCOL_HEADER.read_text(encoding="utf-8")
        self.assertRegex(
            header,
            r"\bacquire_git_protocol_v2_pack\s*\(",
        )
        for helper in RAW_HELPERS:
            self.assertNotRegex(header, rf"\b{helper}\s*\(")

    def test_transport_round_helpers_have_internal_linkage(self) -> None:
        source = PROTOCOL_SOURCE.read_text(encoding="utf-8")
        for helper in RAW_HELPERS:
            self.assertRegex(source, rf"\bstatic\s+auto\s+{helper}\s*\(")

    def test_no_other_production_file_calls_raw_smart_rounds(self) -> None:
        named_raw_round = re.compile(
            r"\b[A-Za-z_]*transport\s*(?:\.|->)\s*"
            r"(?:discover|ls_refs|fetch)\s*\("
        )
        raw_round = re.compile(
            r"(?:\.|->)\s*(?:discover|ls_refs|fetch)\s*\("
        )
        private_helper = re.compile(
            r"\b(?:discover_git_protocol_v2|list_git_protocol_v2_refs|"
            r"fetch_git_protocol_v2)\s*\("
        )
        violations: list[str] = []
        for path in production_sources():
            if path == PROTOCOL_SOURCE:
                continue
            text = path.read_text(encoding="utf-8")
            smart_scope = (
                "git_smart_transport" in text
                or "git_protocol_v2" in text
            )
            raw_round_allowed = path == NATIVE_SMART_TRANSPORT_SOURCE
            if (
                (
                    not raw_round_allowed
                    and (
                        named_raw_round.search(text)
                        or (smart_scope and raw_round.search(text))
                    )
                )
                or private_helper.search(text)
            ):
                violations.append(str(path.relative_to(REPOSITORY_ROOT)))
        self.assertEqual(
            violations,
            [],
            "raw smart-transport rounds bypassed complete acquisition in: "
            + ", ".join(violations),
        )


if __name__ == "__main__":
    unittest.main()
