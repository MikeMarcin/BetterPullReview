"""[[BPR-1160]]. The pack's own offline oracle accepts a bundle this build writes.

Runs the core bundle test, which rebuilds the pack's example round into a
.bpr-review.zip with the product's writer, then validates those bytes with the
retained pack's reference_contract.validate_bundle: container, manifest,
canonical round digest and the derived review.md. This is interoperability
evidence for the written format, not native import or security evidence.
"""
from __future__ import annotations

import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def main() -> int:
    binary, pack = sys.argv[1], pathlib.Path(sys.argv[2])
    sys.path.insert(0, str(pack / "scripts"))
    import reference_contract  # noqa: E402  (the retained pack oracle)

    # A `tempfile` root is owner-only on Windows, which the AppContainer cannot
    # use; see scripts/contained_temporary_support.py.
    with contained_temporary_directory(prefix="bpr-bundle-oracle-") as scratch:
        rebuilt = pathlib.Path(scratch) / "rebuilt.bpr-review.zip"
        subprocess.run([binary, str(pack / "examples"), str(rebuilt)], check=True)
        members = reference_contract.validate_bundle(rebuilt.read_bytes())
    objects = sorted(name for name in members if name.startswith("objects/"))
    if {"manifest.json", "round.json", "review.md"} - set(members) or len(objects) != 4:
        print(f"unexpected members: {sorted(members)}", file=sys.stderr)
        return 1
    print("round-bundle-oracle:pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
