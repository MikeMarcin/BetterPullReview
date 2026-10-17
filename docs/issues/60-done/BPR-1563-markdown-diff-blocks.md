---
schema: "implement-rfc.card.v1"
id: "BPR-1563"
title: "Markdown reader: diff and patch blocks coloured by line, as GitHub does"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request 2026-10-16: GitHub parity for code blocks in Markdown bodies","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 2 (2026-10-16)","criteria":["AM2-03","AM2-05"],"related_cards":["BPR-1562","BPR-1564"],"reservation":"docs/issues/_reservations/BPR-1562--BPR-1564--cc3f094054ee.json","design_skill":"frontend-design:frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"425ea6835f10949a99271b3f8b4f0ec8101a5b66","evidence":{"kind":"command","detail":"core_markdown_code compares every line of diff.md and diff-probe.md with GitHub's pl-* spans; qt_markdown_code_* checks each mark's fill, bold and ink contrast (4.5:1, 7:1 high contrast) in four themes and the painted fill pixel; a diff with '-\\t' lines renders (tab repair fix); indented diff plain."}},{"criterion":"AC2","outcome":"pass","revision":"425ea6835f10949a99271b3f8b4f0ec8101a5b66","evidence":{"kind":"command","detail":"Rebased onto main at c9ff3bf50 (BPR-1544 landed); round-trip composition contract resealed (github_application.cpp 686df60d..., production app source ef36523d...); full dev ctest: 762 of 764 passed, then architecture_boundaries passed after the shell guard admitted the exact Linguist alias rows (negative control: a quoted shell elsewhere still fails) and application_inline_publish_cli_consent passed alone (load timing under -j6); application_composition_contract passed; DOCUMENTATION_OK; clang-format 22 and clang-tidy 22 clean on touched lines."}},{"criterion":"AC3","outcome":"pass","revision":"425ea6835f10949a99271b3f8b4f0ec8101a5b66","evidence":{"kind":"observation","detail":"Owner accepted BPR-1563-diff-blocks-v001 (84fb09a7) with no note in session 03479a9ac4702b758484f2587c7e35f4 on 2026-10-16 (decisions at sequences 13-15, handed back at 16, cursor 16 consumed), on the frozen build from c7aaec454 beside GitHub in Light and Dark."}}]
---

# BPR-1563: Markdown reader: diff and patch blocks coloured by line, as GitHub does

Under [RFC-BPR-0036 owner amendment 2](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-2--2026-10-16-fenced-code-as-github-draws-it),
`diff`, `patch` and `udiff` blocks mark each line as GitHub's diff
grammar does, in the theme's diff colours.

## Acceptance

- **AC1 (AM2-03):** every line of `code-blocks-v1/diff.md` has the kind
  GitHub's rendering gives it (added, removed, range with its exact extent,
  from/to file headers, git header, separators, plain); colours come from the
  theme's diff roles and read at 4.5:1 (7:1 high contrast) in the four
  themes; indented diff and inline code unchanged.
- **AC2:** gates and the reader's rows, as on [[BPR-1562]].
- **AC3 (AM2-05), owner:** Accept/Reject beside GitHub in Light and Dark.

## Design and evidence

Shared with [[BPR-1562]] (design and evidence there). GitHub's `source.diff`
grammar, rule for rule (`core::markdown_diff_marks`), observed through its
Markdown API on `code-blocks-v1/diff.md` and `diff-probe.md`. Added/removed
lines and file headers: the diff roles' fills behind the text, the intraline
hue held to 4.5:1 (7:1 high contrast) on the fill; `!` lines the moved fill
and attention ink; the range bold in the hunk header ink; git/command headers
and separators in the constant colour. `core_markdown_code` compares every
diff line with GitHub's spans; `qt_markdown_code_*` checks each mark's fill and
ink contrast in four themes and the painted fill's pixel. A diff block whose
lines hold `-\t` no longer sends the body to source (the tab repair fix).
