"""BPR-1132: real composition, file-menu gesture, distinct pairs and process restart."""
import argparse
from pathlib import Path

from profile_window_review_restoration_contract import (
    contained_temporary_directory, make_root_private, run, seed_auth,
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    arguments = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-1132-tabs-") as temporary:
        root = Path(temporary).resolve()
        make_root_private(root)
        data = root / "data"
        make_root_private(data)
        vault = seed_auth(arguments.application, data)
        vault = run(arguments.application, "tabs-edit", data, vault)
        run(arguments.application, "tabs-restore", data, vault)
    print("DOCUMENT_TABS_RESTORATION_OK")


if __name__ == "__main__":
    main()
