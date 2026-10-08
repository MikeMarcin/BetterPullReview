"""Check actual drawer gestures and durable rows across independent processes."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from bpr_store_connection import store_connection  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    arguments = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-0708-layout-") as temporary:
        # [[BPR-0875]]. `os.chmod(root, 0o700)` is POSIX's way of saying "only
        # me"; on Windows it only clears the read-only attribute, so the root
        # kept entries the stores refuse. The shared helper writes the
        # descriptor the product defines, on either platform.
        root = Path(temporary).resolve() / "data"
        make_root_private(root)

        def run(phase):
            result = subprocess.run([arguments.application, phase, str(root)],
                                    capture_output=True, text=True, timeout=120, check=False)
            if result.returncode or f"profile-window-layout:{phase}:pass" not in result.stdout:
                raise SystemExit(f"{phase} failed ({result.returncode})\n{result.stdout}\n{result.stderr}")

        run("seed")
        first, second, width_a, width_b, summary_a, summary_b = map(int, (root / "layout-expectations.txt").read_text().split())
        geometry = [list(map(int, row.split())) for row in
                    (root / "geometry-expectations.txt").read_text().splitlines()]
        settings = root / "state/v1/workbench-settings.sqlite3"
        if not settings.is_file():
            raise SystemExit("actual Default layout store was not created")
        with store_connection(settings) as database:
            rows = database.execute("SELECT window_id, files_width FROM window_layout").fetchall()
            if dict(rows) != {str(first): width_a, str(second): width_b}:
                raise SystemExit(f"chosen layout rows disagree: {rows}")
            summaries = dict(database.execute("SELECT window_id,summary_fraction FROM window_layout"))
            if summaries != {str(first): summary_a, str(second): summary_b}:
                raise SystemExit(f"per-window summary preferences disagree: {summaries}")
            panes = dict(database.execute("SELECT window_id, one_pane FROM window_layout"))
            if panes != {str(first): 1, str(second): 0}:
                raise SystemExit(f"one-pane choices disagree: {panes}")
            # [[BPR-0783]]. Home's arrangement is the window's, in the same row:
            # A groups by organization with the state headers off, B keeps the
            # defaults, collapses Recently viewed and owns a Requested filter.
            home = {window: values for window, *values in database.execute(
                "SELECT window_id,home_grouping,home_state_headers,home_collapsed,home_filter_owned,home_role"
                " FROM window_layout")}
            if home != {str(first): [1, 0, 0, 0, 0], str(second): [0, 1, 8, 1, 4]}:
                raise SystemExit(f"per-window Home views disagree: {home}")
            placements = {window: values for window, *values in database.execute(
                "SELECT window_id,geometry_x,geometry_y,geometry_width,geometry_height FROM window_layout")}
            if placements != {str(first): geometry[0], str(second): geometry[1]}:
                raise SystemExit(f"native geometry does not match observed windows: {placements}")
            tables = [name for (name,) in database.execute(
                "SELECT name FROM sqlite_schema WHERE type='table' AND name!='window_layout' ORDER BY name")]
            defaults = {name: database.execute(f'SELECT * FROM "{name}"').fetchall() for name in tables}
            # Simulate interrupted post-Close cleanup: this ID is absent from the registry.
            database.execute("INSERT INTO window_layout(window_id,files_state,conversations_state,files_width,conversations_width,line_numbers,repositories_state,repositories_width,one_pane) SELECT '18446744073709551614',files_state,conversations_state,files_width,conversations_width,line_numbers,repositories_state,repositories_width,one_pane FROM window_layout WHERE window_id=?", (str(first),))
        run("restore")
        with store_connection(settings) as database:
            rows = database.execute("SELECT window_id, files_width FROM window_layout").fetchall()
            if dict(rows) != {str(second): width_b}:
                raise SystemExit(f"explicit Close/orphan reconciliation changed the wrong rows: {rows}")
            if dict(database.execute(
                    "SELECT window_id,home_collapsed FROM window_layout")) != {str(second): 8}:
                raise SystemExit("the surviving window lost its own Home view")
            if {name: database.execute(f'SELECT * FROM "{name}"').fetchall() for name in tables} != defaults:
                raise SystemExit("window gestures changed profile defaults or behaviour")
        run("verify")
        registry = root / "state/v1/profile-registry.sqlite3"
        with store_connection(registry) as database:
            high_water = database.execute("SELECT last_window_id FROM profile_registry_meta").fetchone()
        with store_connection(settings) as database:
            database.execute("PRAGMA user_version=999")
        run("broken")
        with store_connection(registry) as database:
            if database.execute("SELECT last_window_id FROM profile_registry_meta").fetchone() != high_water:
                raise SystemExit("optional layout failure reset or advanced the allocator")
        with store_connection(settings) as database:
            if database.execute("PRAGMA user_version").fetchone() != (999,):
                raise SystemExit("future optional layout schema was silently reset")
    print("PROFILE_WINDOW_LAYOUT_OK")


if __name__ == "__main__":
    main()
