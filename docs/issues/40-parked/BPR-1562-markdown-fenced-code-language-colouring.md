---
schema: "implement-rfc.card.v1"
id: "BPR-1562"
title: "Markdown reader: fenced code coloured by language, as GitHub does"
status: "Parked"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request 2026-10-16: GitHub parity for code blocks in Markdown bodies","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 2 (2026-10-16)","criteria":["AM2-01","AM2-02","AM2-05"],"related_cards":["BPR-1563","BPR-1564","BPR-1543","BPR-1544","BPR-1558","BPR-1559"],"reservation":"docs/issues/_reservations/BPR-1562--BPR-1564--cc3f094054ee.json","design_skill":"frontend-design:frontend-design","landing_after":["BPR-1543","BPR-1544","BPR-1559"]}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"command","detail":"core_markdown_code and qt_markdown_code_{1_0,1_25,2_0}: all 147 info strings of code-blocks-v1/languages.md against GitHub's Markdown API: 100 coloured through the packaged classifier in role colours, GitHub-plain ones, unknown languages, indented and inline code plain; uncovered by packaged definitions (stay plain): console, txt, vim, hlsl, wasm, svelte, http, regex, nushell."}},{"criterion":"AC2","outcome":"pass","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"command","detail":"qt_markdown_code_*: arrival leaves scroll, selection, height and text unchanged; one classification per text and definition, the cache serves a second reader; theme change classifies nothing; answers for other text or language colour nothing; no dispatch asks nothing; 58 KB/103-block body renders in 46 ms. application_markdown_code_syntax_host: bounds, failure, shutdown."}},{"criterion":"AC3","outcome":"pending","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"freeform","detail":"fuzz_markdown selector 4: 10 min, 574,890 runs, clean. Pending at landing: rebase onto main after BPR-1544, reseal the round-trip composition contract over the merged tree, full dev ctest once, documentation gate. Done so far: clang-format 22 and clang-tidy 22 clean on touched files (main.cpp's remaining findings are on untouched lines), DOCUMENTATION_OK, 50 related rows plus qt_composed_inline_drawer_* pass."}},{"criterion":"AC4","outcome":"pending","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session 03479a9ac4702b758484f2587c7e35f4 (state /Users/jaeger/.mask/attended-review-sessions/bpr-1562-fenced-code-v001-20261016, cursor 12): candidate BPR-1562-language-colours-v001 (dc4f8a6e...); frozen root /Users/jaeger/bpr-1562-fenced-code-v001.lmt9brah from c7aaec454 (executable 73e6b117...); GitHub-vs-app sheets in Light and Dark; live sample MikeMarcin/BPR-Test#80."}}]
---

# BPR-1562: Markdown reader: fenced code coloured by language, as GitHub does

Under [RFC-BPR-0036 owner amendment 2](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-2--2026-10-16-fenced-code-as-github-draws-it),
a fenced block whose info string names a language GitHub highlights is
coloured by the review panes' classifier and colours. Today every fenced
block is plain monospace.

## Acceptance

- **AC1 (AM2-01):** against GitHub's Markdown API rendering of
  `tests/fixtures/markdown/code-blocks-v1/languages.md`, every block GitHub
  highlights whose language the packaged definitions cover is coloured, and
  every block GitHub leaves plain stays plain; tokens wear the panes' role
  colours in the four themes at 1.0, 1.25 and 2.0; indented code, inline
  code, unknown languages and no info string are unchanged.
- **AC2 (AM2-02):** colour arrives asynchronously with scroll position,
  selection, height and text unchanged; one classification per block text
  and definition, later readers served from the cache; a theme change
  recolours without classifying; no fetch; limits respected (negative
  controls).
- **AC3:** the fence scanner's fuzz campaign is clean; the reader's existing
  rows pass; gates (clang-format 22, tidy on touched files, documentation,
  composition contracts resealed for `src/app`); full dev suite once before
  landing.
- **AC4 (AM2-05), owner:** Accept/Reject on a frozen build beside GitHub in
  Light and Dark.

## Coordination

The panes' classification lane (`appearance_application_host`, which
[[BPR-1558]] is repairing) is not touched: the reader uses its own
`syntax_classification_application_host` over the same packaged
classifier, definitions and appearance effect lane, as the Review Cycle's
code views do ([[BPR-1161]]). [[BPR-1559]]'s classifier fix reaches the
reader with no change here. Landing waits for [[BPR-1543]], [[BPR-1544]] and
[[BPR-1559]] on main; this branch is stacked on BPR-1544's.

## Design

The `frontend-design` skill was loaded before the visual choices. The brief
pins the direction: GitHub's own rendering, in the app's theme roles, with no
new colour. The full description is the reader design's
[Fenced code](../../architecture/native-markdown.md#fenced-code-amendment-2).

- **Core decides** (`core/markdown_code.*`): the fence scan (CommonMark
  fences in quotes and list items, bounded), GitHub's language table checked
  against its Markdown API, GitHub's diff grammar, the suggestion rows.
- **Host performs** (`application/markdown_code_syntax.*`): a host of its own
  over a `syntax_classification_application_host`, one classification at a
  time, newest first, cached by text and definition; the composition gives it
  a packaged classifier over the shared definitions on the appearance effect
  lane. The panes' `appearance_application_host` ([[BPR-1558]]) is untouched.
- **Qt presents** (`adapters/qt/markdown_code.*`): finds each block by its
  lines in the imported document, draws spans as layout formats in the panes'
  role colours, lays out suggestions, paints their box. The window relays the
  overview's and the open thread's requests and hands every answer back.
- **Found while building:** Qt's import keeps a tab after a code line's first
  non-blank character, while the tab repair expanded all of them, so any diff
  block with `-\t` sent the whole body to source. Fixed in `expanded_tabs`.
  Qt also lays a document's first block at its top whatever its margin, so a
  body that opens with a suggestion keeps the box's gap on its root frame.

## Evidence (branch `bpr-1562-fenced-code`, stacked on BPR-1544's)

- `core_markdown_code`: scan shapes and bounds; all 147 info strings of
  `code-blocks-v1/languages.md` against GitHub's decision; every line of the
  diff fixtures against GitHub's `pl-*` spans; suggestion rows against the
  BPR-Test#80 review.
- `application_markdown_code_syntax_host`: once per text and definition,
  newest first, cache answers a second reader, plain/oversized/after-shutdown
  refused, a failure ends only its text, waiting bounded.
- `qt_markdown_code_{1_0,1_25,2_0}` through the composed classifier (packaged
  definitions, the host, the reader): 100 of GitHub's coloured info strings
  coloured in role colours; GitHub-plain ones and indented code plain;
  arrival leaves scroll, selection, height and text unchanged; one
  classification per text, the cache serves a second reader; a theme change
  classifies nothing; negative controls (other text or language, no
  dispatch); a 58 KB body of 103 blocks renders in 46 ms and is coloured by
  359 ms (Debug -O0).
- `application_github_host` (StartLine projection, negative control shown
  failing), `qt_conversation_cards_view` (locator lines, outdated fallback,
  pull request comment plain, code dispatched by file path).
- `github_contract_fixture_security` with `markdown-code-v1`; the app renders
  the fixture's description coloured offscreen (`--render-evidence`).
- Related rows pass: reader, GitHub parity, composed markdown, drawer, long
  thread, overview page, workbench window, syntax pane composition (50 rows).
  `qt_composed_github_scale_*` fails on this base: BPR-1544's HTML subset
  draws `<b>`, which that course expects literal (not this change).
- `fuzz_markdown` selector 4: 10 minutes, 574,890 runs, clean.
- clang-format 22, clang-tidy 22 clean on touched files; documentation gate OK.
