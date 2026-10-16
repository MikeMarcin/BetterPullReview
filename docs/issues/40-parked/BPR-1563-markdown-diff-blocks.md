---
schema: "implement-rfc.card.v1"
id: "BPR-1563"
title: "Markdown reader: diff and patch blocks coloured by line, as GitHub does"
status: "Parked"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request 2026-10-16: GitHub parity for code blocks in Markdown bodies","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 2 (2026-10-16)","criteria":["AM2-03","AM2-05"],"related_cards":["BPR-1562","BPR-1564"],"reservation":"docs/issues/_reservations/BPR-1562--BPR-1564--cc3f094054ee.json","design_skill":"frontend-design:frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"command","detail":"core_markdown_code compares every line of diff.md and diff-probe.md with GitHub's pl-* spans; qt_markdown_code_* checks each mark's fill, bold and ink contrast (4.5:1, 7:1 high contrast) in four themes and the painted fill pixel; a diff with '-\\t' lines renders (tab repair fix); indented diff plain."}},{"criterion":"AC2","outcome":"pending","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"freeform","detail":"Pending at landing: rebase onto main after BPR-1544, reseal the round-trip composition contract over the merged tree, full dev ctest once, documentation gate. Done so far: clang-format 22 and clang-tidy 22 clean on touched files (main.cpp's remaining findings are on untouched lines), DOCUMENTATION_OK, 50 related rows plus qt_composed_inline_drawer_* pass."}},{"criterion":"AC3","outcome":"pending","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session 03479a9ac4702b758484f2587c7e35f4 (state /Users/jaeger/.mask/attended-review-sessions/bpr-1562-fenced-code-v001-20261016, cursor 12): candidate BPR-1563-diff-blocks-v001 (84fb09a7...); frozen root /Users/jaeger/bpr-1562-fenced-code-v001.lmt9brah from c7aaec454 (executable 73e6b117...); GitHub-vs-app sheets in Light and Dark; live sample MikeMarcin/BPR-Test#80."}}]
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
