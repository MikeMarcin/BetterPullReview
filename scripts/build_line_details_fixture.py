#!/usr/bin/env python3
"""Writes the constructed Line details pull request the product can serve.

BPR-1334. The owner judges Line details, the strip under the comparison that
shows the current line's base version above its head version, full width and
wrapped. No fixture had a changed line long enough to need it: the longest in
any constructed or recorded pull request was 148 characters. This one is a
small pull request whose changes are long lines, the kind Line details is for:

- `config/flags.json`, a minified one-line JSON document with two values
  changed in the middle of it;
- `docs/export.md`, unwrapped Markdown paragraphs, one with a clause rewritten
  and one removed, one added;
- `src/report/query.cpp`, a long SQL string literal and a long call whose
  arguments changed, with short unchanged context around them;
- `web/static/app.min.js`, one minified line of about twelve thousand
  characters, changed near its start, which Line details shows cut with a
  marker.

Nothing here was fetched, no identity is real, and none of the source is built
or run. The output has the shape `build_recorded_pull_request_contract.py` and
`build_recorded_pull_request_pack.py` read, the same as the nested-folders
construction, whose helpers this reuses.

Usage:
    build_line_details_fixture.py <construction.json>
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_nested_folders_fixture as nested  # noqa: E402

# The commits build_recorded_pull_request_pack.py builds from these files,
# pinned for the same reason the nested-folders builder pins its own.
BASE_OID = "37b3ffda05bf3693487e4f92a5a1118d8647bede"
HEAD_OID = "a7603d7f2fe157ae72a0d4e13dada90fab83b31a"

LOGIN = "fixture-author"
lines = nested.lines


def flags(export_retries: int, preview: str, density: str) -> str:
    document = {
        "schema": "flags/v3",
        "owner": "reporting",
        "flags": {
            "report.export.csv": {"enabled": True, "rollout": 100, "retries": export_retries, "timeoutSeconds": 45},
            "report.export.xlsx": {"enabled": True, "rollout": 100, "retries": export_retries, "timeoutSeconds": 90},
            "report.preview": {"enabled": preview == "on", "rollout": 25 if preview == "on" else 0, "audience": "staff"},
            "report.table.density": {"value": density, "choices": ["compact", "comfortable", "spacious"]},
            "report.share.link": {"enabled": True, "expiresHours": 72, "requiresSignIn": True},
            "report.schedule.weekly": {"enabled": False, "day": "monday", "hour": 9, "timezone": "UTC"},
        },
    }
    return json.dumps(document, separators=(",", ":"), sort_keys=False) + "\n"


def bundle(version: str, retry: str) -> str:
    parts = [f'/*! report-app {version} */"use strict";var R={{version:"{version}",retry:{retry}}};']
    for index in range(260):
        parts.append(f"function r{index}(a,b){{return a.rows[{index}%a.rows.length]+b*{index}}}")
    parts.append("export{R};")
    return "".join(parts) + "\n"


EXPORT_BASE = lines(
    "# Exporting a report",
    "",
    "Choose **Export** above the table and pick CSV or Excel. The file contains the rows the table shows with its current filters and sort, the column headings as they are named in the report, and a first row that says when the export was made and by whom, so a copy that is passed around later can still be traced back to the report and the moment it came from.",
    "",
    "If an export fails, the report shows an error and you can try again. Large reports can take a minute or two to export; the button shows a spinner while the file is prepared, and you can keep working in other reports while it runs in the background.",
    "",
    "Exports older than thirty days are deleted.",
)

EXPORT_HEAD = lines(
    "# Exporting a report",
    "",
    "Choose **Export** above the table and pick CSV or Excel. The file contains the rows the table shows with its current filters and sort, the column headings exactly as they are named in the report, and a first row that says when the export was made and by whom, so a copy that is passed around later can still be traced back to the report and the moment it came from.",
    "",
    "If an export fails, the report says which step failed and why in one sentence, and offers Try again, which retries only that step. Large reports can take a minute or two to export; the button shows a spinner while the file is prepared, and you can keep working in other reports while it runs in the background.",
    "",
    "Exports are kept for ninety days and then deleted; a deleted export can be made again from the report.",
)

QUERY_BASE = lines(
    '#include "report/query.h"',
    "",
    "namespace report {",
    "\tauto weekly_totals(const session& aSession, const range& aRange) -> table",
    "\t{",
    '\t\tconstexpr auto sql = "SELECT region, product, SUM(quantity) AS units, SUM(quantity * unit_price) AS revenue FROM orders WHERE placed_at >= ?1 AND placed_at < ?2 GROUP BY region, product ORDER BY revenue DESC";',
    "\t\tauto statement = aSession.prepare(sql);",
    "\t\tstatement.bind(1, aRange.Begin);",
    "\t\tstatement.bind(2, aRange.End);",
    "\t\treturn collect_rows(statement, column_names{ \"Region\", \"Product\", \"Units\", \"Revenue\" }, row_limit{ 10'000 }, aSession.cancellation());",
    "\t}",
    "} // namespace report",
)

QUERY_HEAD = lines(
    '#include "report/query.h"',
    "",
    "namespace report {",
    "\tauto weekly_totals(const session& aSession, const range& aRange) -> table",
    "\t{",
    '\t\tconstexpr auto sql = "SELECT region, product, COUNT(DISTINCT order_id) AS orders, SUM(quantity) AS units, SUM(quantity * unit_price) AS revenue FROM orders WHERE placed_at >= ?1 AND placed_at < ?2 AND status <> \'cancelled\' GROUP BY region, product ORDER BY region, revenue DESC";',
    "\t\tauto statement = aSession.prepare(sql);",
    "\t\tstatement.bind(1, aRange.Begin);",
    "\t\tstatement.bind(2, aRange.End);",
    "\t\treturn collect_rows(statement, column_names{ \"Region\", \"Product\", \"Orders\", \"Units\", \"Revenue\" }, row_limit{ 50'000 }, aSession.cancellation(), progress_every{ 1'000 });",
    "\t}",
    "} // namespace report",
)

# (path, base, head), ordered by path as Git and GitHub order a comparison.
FILES: tuple[tuple[str, str | None, str], ...] = (
    ("config/flags.json", flags(2, "off", "comfortable"), flags(3, "on", "comfortable")),
    ("docs/export.md", EXPORT_BASE, EXPORT_HEAD),
    ("src/report/query.cpp", QUERY_BASE, QUERY_HEAD),
    ("web/static/app.min.js", bundle("2.4.0", "2"), bundle("2.5.0", "3")),
)


def construction() -> dict:
    nested.FILES = FILES
    nested.BASE_OID = BASE_OID
    nested.HEAD_OID = HEAD_OID
    result = nested.construction()
    result["commits"][0]["message"] = "Explain export failures in one sentence and widen the weekly totals"
    result["constructed"] = "Synthetic BPR-1334 fixture written by scripts/build_line_details_fixture.py; not a recording"
    request = result["pull_request"]
    request["body"] = "Constructed fixture: changed lines long enough to need Line details.\n"
    request["head"]["ref"] = "line-details"
    request["number"] = 34
    request["title"] = "Explain export failures in one sentence and widen the weekly totals"
    result["repository"] = {"name": "line-details", "owner": "bpr-fixtures"}
    return result


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(construction(), indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")
    print(f"CONSTRUCTION_OK:{len(FILES)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
