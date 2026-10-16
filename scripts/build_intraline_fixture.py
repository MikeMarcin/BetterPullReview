#!/usr/bin/env python3
"""Writes the constructed intraline pull request the product can serve.

BPR-1537. The owner judges how a changed line's emphasis reads: which words of
a similar pair the comparison marks as changed. No fixture had more than a
handful of similar pairs, and none had the kinds of change a reviewer meets
every day side by side. This is a small pull request made of them, each
changed line a one-for-one replacement between unchanged lines so it pairs as
similar:

- `src/review/comparison.cpp`: renamed identifiers and parameters, an inserted
  and a removed argument, edited string literals, a changed operator, spacing
  realigned, numbers changed, operands swapped, a line rewritten;
- `docs/review-guide.md`: prose with a word changed, a clause rewritten, a
  phrase added, a link's folder changed and a sentence replaced;
- `config/review.json`: a number, a revision hash and a string value changed;
- `tools/report.py`: a renamed function, an argument added, an f-string edited;
- `locale/messages.txt`: changes in accented, CJK, symbol and emoji text.

Nothing here was fetched, no identity is real, and none of the source is built
or run. The output has the shape `build_recorded_pull_request_contract.py` and
`build_recorded_pull_request_pack.py` read, the same as the nested-folders
construction, whose helpers this reuses.

Usage:
    build_intraline_fixture.py <construction.json>
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_nested_folders_fixture as nested  # noqa: E402

# The commits build_recorded_pull_request_pack.py builds from these files,
# pinned for the same reason the nested-folders builder pins its own.
BASE_OID = "a3f915b5baa6b05b2ffc1ac6502b218b5d16bec7"
HEAD_OID = "d7e2f04de93e5e51097627e58093a42b68de36ce"

lines = nested.lines

COMPARISON_BASE = lines(
    '#include "review/comparison.h"',
    "",
    '#include "review/diff_engine.h"',
    "#include <QKeyEvent>",
    "",
    "namespace review {",
    "\t// Compares one file of the pull request and keeps the result for the panes.",
    "\tauto compare_file(const file_input& input, const components& parts, stop_token stop) -> comparison",
    "\t{",
    "\t\tconst auto result = compute_file_diff(input, parts);",
    "",
    "\t\tauto conversationCount = model->rowCount();",
    "",
    "\t\tconst auto leftScalars = decode(input.LeftBytes);",
    "",
    "\t\tconst auto outcome = run_tool(process, configuration, std::move(invocation), stop);",
    "",
    "\t\tsetSizePolicy(QSizePolicy::Expanding, expanded ? QSizePolicy::Expanding : QSizePolicy::Fixed);",
    "",
    "\t\tlabel->setText(tr(\"Show the conversation drawer at its saved width.\"));",
    "",
    "\t\tview->setToolTip(QStringLiteral(\"Only conversations with a label\"));",
    "",
    "\t\tif (result.Publication != published && observed) {",
    "",
    "\t\tif (index != 0U && match.Left <= previous.Left) {",
    "",
    "\t\tstd::uint64_t     LargestPollDelay = 0U;",
    "",
    "\t\tconstexpr auto StoreFiles = std::array<std::string_view, 11U>{",
    "",
    "\t\treturn a.width() < b.width() && a.height() < b.height();",
    "",
    "\t\tconst auto [left, right] = std::pair{ first, second };",
    "",
    "\t\t// The checks that need no text, then a fresh progress holding copies",
    "",
    "\t\tif (pattern.front() == '/') {",
    "",
    "\t\tauto recieved = queue.take();",
    "",
    "\t\tusing size_type = std::size_t;",
    "",
    "\t\ttransport{ *cloud, *configuration, process };",
    "",
    "\t\treturn comparison{ result, conversationCount };",
    "\t}",
    "",
    "\tvoid set_dispatch(private_dispatch dispatch) { Dispatch = std::move(dispatch); }",
    "} // namespace review",
)

COMPARISON_HEAD = lines(
    '#include "review/comparison.h"',
    "",
    '#include "review/diff_engine.h"',
    "#include <QKeySequence>",
    "",
    "namespace review {",
    "\t// Compares one file of the pull request and keeps the result for the panes.",
    "\tauto compare_file(const file_input& input, const components& parts, stop_token stop) -> comparison",
    "\t{",
    "\t\tconst auto outcome = compute_file_diff(input, parts);",
    "",
    "\t\tauto threadCount = model->rowCount();",
    "",
    "\t\tconst auto leftValues = decode(input.LeftBytes);",
    "",
    "\t\tconst auto outcome = run_tool(process, configuration, std::move(invocation), stop, account);",
    "",
    "\t\tsetSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);",
    "",
    "\t\tlabel->setText(tr(\"Show the comments drawer at its saved width.\"));",
    "",
    "\t\tview->setToolTip(QStringLiteral(\"Only threads with a label\"));",
    "",
    "\t\tif (result.Publication == published && observed) {",
    "",
    "\t\tif (index != 0U && match.Left < previous.Left) {",
    "",
    "\t\tstd::uint64_t LargestPollDelay = 0U;",
    "",
    "\t\tconstexpr auto StoreFiles = std::array<std::string_view, 12U>{",
    "",
    "\t\treturn a.height() < b.height() && a.width() < b.width();",
    "",
    "\t\tconst auto [right, left] = std::pair{ second, first };",
    "",
    "\t\t// The checks that need no text. A comparison they finish yields its",
    "",
    "\t\tif (path.Pattern.empty()) {",
    "",
    "\t\tauto received = queue.take();",
    "",
    "\t\tusing size_type = std::uint64_t;",
    "",
    "\t\ttransport{ *cloud, scoped(*configuration, *cloud), process, account() };",
    "",
    "\t\treturn comparison{ outcome, threadCount };",
    "\t}",
    "",
    "\tvoid set_dispatch(private_dispatch aDispatch) { Dispatch = std::move(aDispatch); }",
    "} // namespace review",
)

GUIDE_BASE = lines(
    "# Reviewing a pull request",
    "",
    "Open the pull request from Home and choose a file in the Overview.",
    "",
    "If an export fails, the report shows an error and you can try again.",
    "",
    "Pending drafts are notes at their lines named \"Pending in your review\".",
    "",
    "See [the card](../issues/30-in-progress/BPR-1188-close-rfc-0055.md) for the remaining steps.",
    "",
    "Exports older than thirty days are deleted.",
    "",
    "This is the last change in this comparison.",
    "",
    "Keyboard: Next change (⌘↓) and Previous change (⌘↑).",
)

GUIDE_HEAD = lines(
    "# Reviewing a pull request",
    "",
    "Open the pull request from Home and choose a file in the Overview.",
    "",
    "If an export fails, the report says which step failed and why, and you can try again.",
    "",
    "Pending drafts are notes at their lines tagged \"Pending in your review\" in the writing blue.",
    "",
    "See [the card](../issues/60-done/BPR-1188-close-rfc-0055.md) for the remaining steps.",
    "",
    "Exports older than ninety days are deleted.",
    "",
    "Last change in the pull request.",
    "",
    "Keyboard: Next change (⌥↓) and Previous change (⌥↑).",
)

SETTINGS_BASE = lines(
    "{",
    '\t"schema": "review/v2",',
    '\t"retries": 2,',
    '\t"timeoutSeconds": 45,',
    '\t"source_revision": "9013e16833a6076e26120a12e5e98dd119b2ac39",',
    '\t"theme": "comfortable",',
    '\t"message": "My pull request; Nothing about it has been read yet."',
    "}",
)

SETTINGS_HEAD = lines(
    "{",
    '\t"schema": "review/v2",',
    '\t"retries": 3,',
    '\t"timeoutSeconds": 45,',
    '\t"source_revision": "fb2e72778d3c61de6ec6723dffc16a1212ab5c31",',
    '\t"theme": "compact",',
    '\t"message": "My pull request; nothing about it has been read yet."',
    "}",
)

REPORT_BASE = lines(
    "import sys",
    "",
    "",
    "def summarize_counts(rows, limit):",
    "    total = sum(row.count for row in rows)",
    "    print(f\"Read {total} rows from {len(rows)} files\")",
    "    return total",
    "",
    "",
    "def main():",
    "    rows = load_rows(sys.argv[1])",
    "    return summarize_counts(rows, 100)",
)

REPORT_HEAD = lines(
    "import sys",
    "",
    "",
    "def summarise_counts(rows, limit):",
    "    total = sum(row.count for row in rows)",
    "    print(f\"Read {total} rows across {len(rows)} changed files\")",
    "    return total",
    "",
    "",
    "def main():",
    "    rows = load_rows(sys.argv[1], encoding=\"utf-8\")",
    "    return summarise_counts(rows, 100)",
)

MESSAGES_BASE = lines(
    "greeting = Bonjour, voulez-vous un café crème ?",
    "",
    "saved_view = 保存されたビュー",
    "",
    "status = done 👩‍💻 today",
    "",
    "arrow = Next change (⌘↓)",
)

MESSAGES_HEAD = lines(
    "greeting = Bonjour, voulez-vous un café noir ?",
    "",
    "saved_view = 保存した表示",
    "",
    "status = done 🧑‍🔧 today",
    "",
    "arrow = Next change (⌥↓)",
)

# (path, base, head), ordered by path as Git and GitHub order a comparison.
FILES: tuple[tuple[str, str | None, str], ...] = (
    ("config/review.json", SETTINGS_BASE, SETTINGS_HEAD),
    ("docs/review-guide.md", GUIDE_BASE, GUIDE_HEAD),
    ("locale/messages.txt", MESSAGES_BASE, MESSAGES_HEAD),
    ("src/review/comparison.cpp", COMPARISON_BASE, COMPARISON_HEAD),
    ("tools/report.py", REPORT_BASE, REPORT_HEAD),
)

TITLE = "Rename the comparison helpers and tidy their messages"


def construction() -> dict:
    nested.FILES = FILES
    nested.BASE_OID = BASE_OID
    nested.HEAD_OID = HEAD_OID
    result = nested.construction()
    result["commits"][0]["message"] = TITLE
    result["constructed"] = "Synthetic BPR-1537 fixture written by scripts/build_intraline_fixture.py; not a recording"
    request = result["pull_request"]
    request["body"] = "Constructed fixture: everyday changed lines, for judging intraline emphasis.\n"
    request["head"]["ref"] = "intraline"
    request["number"] = 37
    request["title"] = TITLE
    result["repository"] = {"name": "intraline", "owner": "bpr-fixtures"}
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
