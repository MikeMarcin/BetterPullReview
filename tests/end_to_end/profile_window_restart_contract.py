"""Exercise durable desktop window identities across four actual processes."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    arguments = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-0707-restart-") as temporary:
        root = Path(temporary).resolve()
        os.chmod(root, 0o700)
        for phase in ("seed", "restore", "close", "verify"):
            result = subprocess.run(
                [arguments.application, phase, str(root)],
                capture_output=True, text=True, timeout=60, check=False,
            )
            marker = f"profile-window-restart:{phase}:pass"
            if result.returncode != 0 or marker not in result.stdout:
                raise SystemExit(f"{phase} failed ({result.returncode})\n{result.stdout}\n{result.stderr}")
    print("PROFILE_WINDOW_RESTART_OK")


if __name__ == "__main__":
    main()
