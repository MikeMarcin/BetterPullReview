#!/usr/bin/env python3
"""Every type-read structured binding carries `[[maybe_unused]]`.

The house idiom for reading a record's member types is a structured binding
over a value-initialised record whose names are then used only inside
`decltype` in a `static_assert`. Clang and GCC count an unevaluated use as a
use; MSVC does not, raises C4189, and the Windows tree builds
warnings-as-errors, so each new instance breaks the Windows lane and nothing
on macOS or Linux can see it coming (BPR-0512, then BPR-0518). This gate
finds the idiom by shape — a `const auto[&] [...] = T{ };` declaration — and
refuses any instance that does not carry the attribute, naming the line.

Owned by BPR-0518.
"""

from __future__ import annotations

import pathlib
import re
import sys
import unittest


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
ROOTS = ("src", "tests")
IDIOM = re.compile(r"^\s*(?P<attribute>\[\[maybe_unused\]\]\s+)?const auto&?\s*\[[^\]]+\]\s*=\s*[A-Za-z_][A-Za-z0-9_:<>, ]*\{\s*\};")


def unattributed_type_reads(root: pathlib.Path) -> list[str]:
    """Every idiom instance under `root` lacking the attribute, as path:line."""
    findings: list[str] = []
    for directory in ROOTS:
        for path in sorted((root / directory).rglob("*.cpp")):
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                match = IDIOM.match(line)
                if match is not None and match.group("attribute") is None:
                    findings.append(f"{path.relative_to(root)}:{number}")
    return findings


class TypeReadBindingTests(unittest.TestCase):
    def test_every_type_read_binding_carries_maybe_unused(self) -> None:
        findings = unattributed_type_reads(REPOSITORY_ROOT)
        self.assertEqual(
            findings,
            [],
            "type-read structured bindings without [[maybe_unused]] (MSVC C4189 "
            "under warnings-as-errors): " + ", ".join(findings),
        )

    def test_the_shape_is_recognised_with_and_without_the_attribute(self) -> None:
        self.assertIsNotNone(IDIOM.match("\t\tconst auto& [keyed, typed] = message{ };"))
        self.assertIsNotNone(IDIOM.match("\t\tconst auto [a, b] = ns::record<int>{ };"))
        attributed = IDIOM.match("\t\t[[maybe_unused]] const auto& [keyed, typed] = message{ };")
        self.assertIsNotNone(attributed)
        self.assertIsNotNone(attributed.group("attribute"))
        # A binding over a real value is not the idiom and is left alone.
        self.assertIsNone(IDIOM.match("\t\tconst auto& [session, findings] = payload;"))
        self.assertIsNone(IDIOM.match("\t\tfor (const auto& [name, value] : members) {"))


if __name__ == "__main__":
    unittest.main()
