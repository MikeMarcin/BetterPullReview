#!/usr/bin/env python3
"""[[BPR-0900]]. The three Windows authors of the owner-private descriptor agree
where it matters, and the product is the judge.

The product narrows the storage roots it owns
(`make_storage_root_owner_private`). The harness narrows directories the product
deliberately will not touch, twice over: `tests/end_to_end/bpr_narrow_owner_private.cmake`
for CMake contracts and `tests/end_to_end/bpr_narrow_owner_private.py` for Python
courses. The three are not byte-identical, and that is on purpose:

- the CMake helper writes one inheritable grant for the user and removes
  inheritance. That is enough for the uncontained composition rows it serves;
- the Python helper also grants the process's own AppContainer SID, and removes
  SYSTEM, Administrators and OWNER RIGHTS, because a contained course needs the
  container to reach its own root.

What they must agree on is the question the thirteen rows of [[BPR-0875]]
depend on: may the directory parent an agent workspace neighbourhood? This row
asks the product. It runs `recover_installed_agent_workspaces`, which runs
`workspace_parent_trusted`, on a directory narrowed by each author.

Each author's directory starts inside a root that grants Authenticated Users an
inheritable Modify, which is what a shared build tree hands a course. Narrowing
must take that away. A control directory nobody narrows keeps it, and must be
refused. So the row fails if any author stops removing the shared grant. It
also fails if the product's rule stops refusing it, or starts refusing what the
authors write.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests" / "end_to_end"))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from bpr_narrow_owner_private import make_root_private  # noqa: E402

AUTHENTICATED_USERS = "*S-1-5-11"


def judge(tool: pathlib.Path, directory: pathlib.Path) -> str:
    completed = subprocess.run([str(tool), "judge", str(directory)], capture_output=True, text=True, check=False, timeout=60)
    if completed.returncode != 0:
        raise SystemExit(f"judge failed for {directory.name} ({completed.returncode}): {completed.stdout}{completed.stderr}")
    return completed.stdout.strip()


def acl(directory: pathlib.Path) -> str:
    return subprocess.run(["icacls", str(directory)], capture_output=True, text=True, check=False).stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=pathlib.Path, required=True)
    parser.add_argument("--cmake", type=pathlib.Path, required=True)
    parser.add_argument("--helper", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    if os.name != "nt":
        print("OWNER_PRIVATE_DESCRIPTORS_AGREE (Windows only; nothing to compare here)")
        return 0

    failures: list[str] = []
    with contained_temporary_directory(prefix="bpr-0900-") as raw:
        root = pathlib.Path(raw).resolve()
        # The shared build tree's grant, made explicit and inheritable.
        subprocess.run(["icacls", str(root), "/grant", f"{AUTHENTICATED_USERS}:(OI)(CI)M"], check=True, capture_output=True)
        directories = {name: root / name for name in ("control", "product", "cmake", "python")}
        for directory in directories.values():
            directory.mkdir()

        product = subprocess.run([str(arguments.tool), "narrow", str(directories["product"])], capture_output=True, text=True, check=False, timeout=60)
        if product.returncode != 0:
            failures.append(f"the product could not narrow its directory: {product.stdout.strip()}")
        cmake = subprocess.run(
            [str(arguments.cmake), f"-DBPR_NARROW_HELPER={arguments.helper.as_posix()}", f"-DBPR_NARROW_DIRECTORY={directories['cmake'].as_posix()}",
             "-P", str(ROOT / "tests" / "scripts" / "owner_private_descriptor_agreement.cmake")],
            capture_output=True, text=True, check=False, timeout=60)
        if cmake.returncode != 0:
            failures.append(f"the CMake helper could not narrow its directory: {cmake.stdout}{cmake.stderr}")
        make_root_private(directories["python"])

        expected = {"control": False, "product": True, "cmake": True, "python": True}
        for name, directory in directories.items():
            verdict = judge(arguments.tool, directory)
            trusted = verdict == "TRUSTED"
            print(f"OWNER_PRIVATE_DESCRIPTOR {name}: {verdict}")
            for line in acl(directory).splitlines():
                print(f"    {line.strip()}")
            if trusted != expected[name]:
                failures.append(f"{name}: expected {'trusted' if expected[name] else 'refused'}, the product said {verdict}")

    if failures:
        for failure in failures:
            print(f"OWNER_PRIVATE_DESCRIPTORS_DISAGREE: {failure}")
        return 1
    print("OWNER_PRIVATE_DESCRIPTORS_AGREE")
    return 0


if __name__ == "__main__":
    sys.exit(main())
