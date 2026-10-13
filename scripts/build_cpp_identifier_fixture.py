#!/usr/bin/env python3
"""Write RFC-BPR-0072's synthetic C++ comparison (no remote data).

Usage: build_cpp_identifier_fixture.py <construction.json>
Regenerate its contract and pack with the recorded-pull-request builders.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import build_nested_folders_fixture as nested

BASE_OID = "c587f5b879dc20b319b5de8fb75f512d493bf36b"
HEAD_OID = "7b0b05bac636051f80198ef1ad6c52aff0aedf36"
SOURCE = """#define GCT_ASSERT(value) ((void)(value))
namespace GCT::Network {
enum class Reason {
    Node = 9,
    ID = 1,
    Player = 64,
    DNA
};
enum class preview_emit_mode { Full, Partial };
constexpr auto reason = Reason::ID;
constexpr auto output = preview_emit_mode::Full;
}
namespace MixedCase { enum Colour { Red, Blue }; }
constexpr auto selected = GCT::Network::Reason::DNA;
constexpr auto colour = MixedCase::Colour::Red;
void verify() { GCT_ASSERT(selected == GCT::Network::Reason::ID); }
constexpr int changed = VALUE;
"""


def construction() -> dict:
    nested.FILES = (("review.cpp", SOURCE.replace("VALUE", "1"), SOURCE.replace("VALUE", "2")),)
    nested.BASE_OID = BASE_OID
    nested.HEAD_OID = HEAD_OID
    result = nested.construction()
    result["constructed"] = "Synthetic BPR-1412 C++ identifier comparison; no recording or remote data"
    result["repository"] = {"name": "cpp-identifiers", "owner": "bpr-fixtures"}
    result["commits"][0]["message"] = "Update C++ identifier example"
    result["pull_request"].update(number=72, title="C++ namespaces and enum members", body="Constructed complete-blob declarations, qualified uses and a genuine macro.\n")
    result["pull_request"]["head"]["ref"] = "cpp-identifiers"
    return result


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(construction(), indent="\t", sort_keys=True) + "\n", encoding="utf-8")
    print("CONSTRUCTION_OK:1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
