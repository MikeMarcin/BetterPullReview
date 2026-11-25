---
schema: "implement-rfc.card.v1"
id: "BPR-2371"
title: "Inline code that is not code stays plain"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2371--BPR-2371--0e710506b202.json","found_by":"owner report with screenshot, 2026-11-24, Agent tab transcript","related_cards":["BPR-1753","BPR-2266","BPR-2277"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8773b40f9","evidence":{"kind":"command","detail":"core_markdown_code inline_code_reads_as_code: the owner's betterpullreview-agent-9dbd6144..., commits, a 40-digit hash, paths, file names, URLs, versions, hyphenated names and flags read as not code; Target.resize, node->next, validate(scale, bias, gamma), 0x1F, 1e-5f, n-1 and deadbeef read as code. qt_markdown_code_1_0/1_25/2_0 check_inline_not_code through the packaged C++ grammar: only Target.resize and the prose are held, the workspace name is never requested and its paragraph has no formats; with the store change reverted it fails at the held-text check and, with that skipped, at \"the owner's workspace name is plain monospace\""}},{"criterion":"AC2","outcome":"pass","revision":"8773b40f9","evidence":{"kind":"command","detail":"core_markdown_code: Review this pull request with this coloured gives no spans; const int coloured whole keeps both; Target.resize keeps BPR-2277 member access; qt_markdown_code: the paragraph's only formats are inside Target.resize"}},{"criterion":"AC3","outcome":"pass","revision":"8773b40f9","evidence":{"kind":"command","detail":"qt_markdown_code check_inline_not_code: the cpp fenced block in the same body is coloured; core_markdown_code: its info names code.cpp"}},{"criterion":"AC4","outcome":"pass","revision":"8773b40f9","evidence":{"kind":"command","detail":"qt_workbench_window_scale_100: the Agent tab asks for Target.resize and never for the workspace name or commit; also core_markdown_code, qt_agent_report_view, qt_tour_side_chat, qt_tour_chat_in_tab, coding-format-check, clang-tidy on src/core/src/markdown_code.cpp, tests/core/markdown_code_tests.cpp, src/adapters/qt/src/markdown_code.cpp, tests/adapters_qt/markdown_code_tests.cpp"}}]
---

# BPR-2371: Inline code that is not code stays plain

## Observed

Owner, 2026-11-24, on an Agent tab reply "The workspace at the PR head is
`betterpullreview-agent-9dbd6144...`. Reading the diff.": "Why does the sha
always get weird syntax highlights here". `betterpullreview-agent-` was plain
and `9dbd6144...` was drawn in the number colour.

Owner's answer on the fix: "the UX should look polished and correct and code
should be highlighted when possible."

## Cause

Owner look v004 ([[BPR-1753]]), [[BPR-2266]] and [[BPR-2277]] colour inline
code in the language most of the pull request's files are in (`code.cpp` for a
C++ pull request). `markdown_code_store::place_inline` sent every inline code
text to the packaged C++ grammar, whether or not it was code. The grammar reads
`betterpullreview-agent-9dbd6144...` as `betterpullreview - agent -
9dbd6144...`: two names, two minus operators, and a token that starts with a
digit and runs on through letters and dots, which it colours as a number. The
result was a name coloured in part.

## Fix

- `core::markdown_inline_code_reads_as_code` (pure, in
  `src/core/src/markdown_code.cpp`) keeps a mention out of the classifier when
  it is a URL, a path (a `/` or `\` with no blank), a file's name, a version
  (`1.2.3`, `v6.12`), a run of 7 or more hex digits holding a digit and a
  letter (a commit or other hash), a hyphenated name (`clang-format`,
  `agent-9dbd6144`; not `n-1` or `1e-5f`), or a command-line flag. The
  store neither holds nor asks for such a text, so it stays plain monospace on
  its wash.
- `core::markdown_inline_code_spans` decides what of an answer is drawn: none
  for prose the grammar coloured in part (`this` in `Review this pull
  request`), otherwise the classifier's spans with [[BPR-2277]]'s member access.
- Every reader that colours inline code shares the store, so the fix covers
  the Agent tab, a report's page, tour side chats, the stop reader and the
  tour's proposed text. Fenced blocks are unchanged.
- `docs/architecture/native-markdown.md` no longer says inline code is never
  coloured; it records the rule.

## Acceptance

- AC1. The owner's span, a commit, a path, a URL, a version, a hyphenated name
  and a flag are never classified and draw no syntax formats; code beside them
  (`Target.resize`, `validate(a, b)`, `0x1F`, `1e-5f`, `n-1`) is still asked
  for and coloured.
- AC2. Prose coloured in part draws no formats; code coloured whole keeps them.
- AC3. A fenced block with a language in the same body is still coloured.
- AC4. A Qt check through the packaged C++ grammar and an Agent tab check
  assert the above; focused tests, format, tidy and the documentation gate pass.

## Evidence

Passing at 8773b40f9 (branch commit 8773b40f9 on 9cbc5b604): core_markdown_code;
qt_markdown_code_1_0, _1_25, _2_0; qt_agent_report_view; qt_tour_side_chat;
qt_tour_chat_in_tab; qt_workbench_window_scale_100; coding-format-check;
clang-tidy on the four touched translation units other than
workbench_window_tests.cpp. The documentation gate is still to run on main:
the worktree has no MASK-deployed implement-rfc scripts.
