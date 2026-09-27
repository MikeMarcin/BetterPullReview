"""Exercise the built terminal entry without display or test-only application modes."""
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    binary = str(Path(sys.argv[1]).resolve())
    environment = dict(os.environ, QT_QPA_PLATFORM="bpr-no-display", NO_COLOR="1")
    environment.pop("DISPLAY", None)
    environment.pop("WAYLAND_DISPLAY", None)
    result = subprocess.run([binary, "tour", "capabilities", "--json"], env=environment,
                            capture_output=True, text=True, check=True, timeout=15)
    value = json.loads(result.stdout)
    assert set(value) == {"schema", "command", "request_id", "status", "data", "warnings", "error"}
    assert value["schema"] == "bpr.tour.cli-result.v1"
    assert value["data"]["gui_required"] is False
    assert value["data"]["ai_required"] is False
    assert value["status"] == "ok" and value["error"] is None
    human = subprocess.run([binary, "tour", "capabilities"], env=environment,
                           capture_output=True, text=True, check=True, timeout=15)
    assert human.stdout.startswith("bpr tour") and not human.stdout.startswith("{")
    refused = subprocess.run([binary, "tour", "capabilities", "--yes", "--json"],
                             env=environment, capture_output=True, text=True, timeout=15)
    assert refused.returncode == 2 and json.loads(refused.stdout)["status"] == "error"
    print("tour-cli-public-entry:pass")


if __name__ == "__main__":
    main()
