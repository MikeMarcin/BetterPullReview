---
schema: "implement-rfc.card.v1"
id: "BPR-1543"
title: "Markdown reader GitHub parity: comments, line breaks, alerts and footnotes"
status: "Parked"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner decision 2026-10-16: close the GitHub-parity gaps in PR description and comment Markdown, as two sequential cards (this is card A)","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 1 (2026-10-16)","criteria":["AM1-01","AM1-02","AM1-03","AM1-04","AM1-08"],"related_cards":["BPR-1544","BPR-1545","BPR-1548"],"reservation":"docs/issues/_reservations/BPR-1543--BPR-1544--05e6fe38f3d4.json","design_skill":"frontend-design:frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"command","detail":"qt_markdown_github_{1_0,1_25,2_0}: comments.md and comments-unclosed.md against GitHub's Markdown API rendering: hidden text absent in both, kept text (code span, fence, indented code, unclosed inline) present in both, an unclosed comment block hides the rest as on GitHub; source keeps the comment; negative control: Qt's own import shows it."}},{"criterion":"AC2","outcome":"pass","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"command","detail":"qt_markdown_github rows: 7 line separators for GitHub's 7 <br> in breaks.md and 4 for 4 in the PR template; paragraph, list item and quote keep one block; table 3x2, code lines, code span over two lines, setext heading unchanged; tab repair intact; an authored U+2008 turns breaks off."}},{"criterion":"AC3","outcome":"pass","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"command","detail":"qt_markdown_github rows: alert kinds and titles equal GitHub's sequence (note,tip,important,warning,caution,note); marker-with-text and in-list stay quotes; heading and table stay inside; title contrast >=4.5 (>=7 high contrast) in light, dark, high-contrast light and dark; Note bar pixel in its colour; plain quote has no bar."}},{"criterion":"AC4","outcome":"pass","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"command","detail":"qt_markdown_github rows: references [1],[2],[1] as GitHub numbers them; back references and items match GitHub's counts; Tab/Enter and click move between reference and back reference with zero application gestures; authored #fn-1 still dispatches; missing/coded reference literal, unused definition dropped; negative control: Qt alone makes the one-word definition a link."}},{"criterion":"AC5","outcome":"pass","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"command","detail":"Reserved-mark guards and leftover sweep; 64 KiB body with every construct renders in 45 ms; 85 composition rows pass; fuzz_markdown_view 15 min, 360,876 runs clean with the no-mark property."}},{"criterion":"AC6","outcome":"pass","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"command","detail":"clang-format 22.1.8 and clang-tidy 22 clean on touched files; verify_documentation DOCUMENTATION_OK; github_contract_fixture_security_tests OK; round-trip composition contract resealed (960a53b7.../8e2f29b9...) and all nine composition contracts pass."}},{"criterion":"AC7","outcome":"pending","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session 6eb0a38e0bc7f9c5a1261b07f3e47c55, cursor 15: BPR-1543-comments-v001 (45b931cb...), -line-breaks-v001 (a764a2c3...), -alerts-v001 (b908869e...), -footnotes-v001 (19401136...), -underscore-emphasis-v001 (e6618af8...); frozen root /Users/jaeger/bpr-1543-markdown-parity-v001.e6tm3nsa (executable 02d0d2a3...), GitHub page vs app sheets in Light and Dark; live sample MikeMarcin/BPR-Test#72."}}]
---

# BPR-1543: Markdown reader GitHub parity: comments, line breaks, alerts and footnotes

Under [RFC-BPR-0036 owner amendment 1](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-1--2026-10-16-github-parity),
the shared Markdown reader (`markdown_body_view`) should render four things the
way GitHub renders a pull request description or comment. Today, through
`QTextDocument::setMarkdown` with `MarkdownDialectGitHub | MarkdownNoHTML`:

- `<!-- … -->` shows as literal text, including whole PR-template comments.
- A single newline inside a paragraph shows as a space; GitHub shows a line
  break.
- `> [!NOTE]` and the other four alerts show as an ordinary quote whose first
  words are `[!NOTE]`.
- `x[^1]` with a one-word definition becomes a link whose destination is the
  footnote's text, and the `[^1]:` definition disappears.

[[BPR-1544]] follows with `<details>`, the sanitised HTML subset and emoji
shortcodes. Images are [RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)'s.

## Acceptance

- **AC1 (AM1-01), comments:** single and multi-line comments are hidden in
  the rendered view; comments in fenced, indented and inline code stay; an
  unclosed comment block hides what follows it, as GitHub does; source and
  copy keep the comment.
- **AC2 (AM1-02), line breaks:** each single newline of a paragraph, list
  item, quote or footnote is a line break in the same block, as many as
  GitHub's `<br>`; code, tables, headings, code spans and the code-tab repair
  are unchanged.
- **AC3 (AM1-03), alerts:** the five kinds render with a coloured left bar and
  an icon-less coloured title, in GitHub's order and words, readable at 4.5:1
  (7:1 in high contrast) in all four themes; a marker with text on its line,
  or inside a list, stays a plain quote; a heading or table inside an alert
  stays inside it.
- **AC4 (AM1-04), footnotes:** raised reference numbers in GitHub's order; a
  footnotes section at the end with back references; Enter or click moves
  between a reference and its back reference inside the body, with no
  application link gesture; a missing or coded reference stays literal; an
  unused definition is dropped.
- **AC5, safety and bounds:** original bytes, source and copy unchanged; a
  body holding one of the reserved mark characters is read without that
  transformation; the preview budgets still hold and a maximal body with every
  construct renders within the one-second local ceiling; existing reader,
  composed, drawer, overview and end-to-end rows pass.
- **AC6, gates:** clang-format 22, tidy on the touched files, documentation
  gate; composition contracts resealed if `src/app` changes.
- **AC7 (AM1-08), owner:** one Accept/Reject per visible construct (comments,
  line breaks, alerts, footnotes, and the discovered `_text_` emphasis) on a
  frozen build, beside GitHub's rendering in Light and Dark.

## Design

The `frontend-design` skill was loaded before the visual choices. The
direction is GitHub's own, inside the reader's inherited palette and fonts,
with no style sheet.

- **Where the work happens.** A private `markdown_github` unit in the Qt
  adapter rewrites the source before `QTextDocument::setMarkdown` and turns
  its marks into native structure afterwards. It is a bounded line and
  code-span scanner, not a Markdown parser; Qt's md4c import stays the parser.
  Code (fenced, indented, inline spans) is never rewritten. Marks are U+2008
  for a line break and the noncharacters U+FDE1 to U+FDEC for alerts and
  footnotes. A body that already holds one of them is read without that
  transformation, and a final sweep removes any mark an unexpected import
  shape left. The original body stays the source of truth.
- **Line breaks.** One line separator per soft break inside the same block,
  which is what GitHub's `<br>` looks like. A written hard break becomes the
  same separator; Qt otherwise split the block, drawing a second bullet in a
  list.
- **Alerts.** GitHub's title words, semibold, in GitHub's colours (Primer
  accent, success, done, attention, danger), moved toward the text colour
  until they read at 4.5:1 on the reader's base, or 7:1 in high contrast. A
  4 px bar in the same colour is painted down the alert's left edge; the
  alert has no quote fill and its text starts 16 px right of the bar. No
  icon, as asked. Alerts stand 8 px apart, so two in a row show two bars.
- **Footnotes.** Raised `[1]` links, as GitHub's stylesheet draws them; the
  section after a rule at the end, at 0.86 of the body size in the quiet ink;
  `↩` back references, the second raised (`↩²`). A character property only
  presentation sets makes them in-body links, so `[x](#fn-1)` written by an
  author still goes to application admission. Enter or click selects the
  other end and scrolls to it.
- **Discovered.** Qt's dialect read `_text_` as underline (md4c's underline
  flag); GitHub reads it as emphasis, so it is italic now. On any dark base,
  not only in place, a link keeps its hue at a readable lightness; a
  footnote's raised number is a link and was too dark in the overview.
- **Limits.** Footnote definitions and alerts are recognized at top level
  only; a reference inside a definition stays literal. In place, a jump
  scrolls a `QScrollArea` holder; the Comments drawer keeps its scroll while
  the selection moves.

## Evidence

Implementation is on branch `bpr-1543-gfm-parity`.

- `qt_markdown_github_{1_0,1_25,2_0}` (new): every fixture under
  `tests/fixtures/markdown/github-parity-v1` is compared with GitHub's own
  rendering of it (`*.github.html`, Markdown API in gfm mode, context
  MikeMarcin/BPR-Test): hidden and kept comment text, `<br>` count against
  line separators, alert kinds and titles in order, footnote numbers and back
  references, and the PR template's headings, tasks, alerts and footnotes.
  Negative controls: Qt's own import shows the comment and makes the
  footnote a link to its text; an authored U+2008 or U+FDE1 turns its
  transformation off. Keyboard and click navigation with no application
  gesture; an authored `#fn-1` link still dispatches. Four themes: title
  contrast (light 4.86 to 5.24, dark 5.64 to 7.49, high contrast 7.02 to
  8.31) and the Note bar's pixel colour. A 64 KiB body with every construct
  inside the budgets renders in 45 ms locally (ceiling 1 s).
- 85 composition rows pass on the final reader (markdown, overview,
  conversation, composed, drawer, Discuss, review submission, destructive
  question, feedback labels, tour editor, workbench window, public preview).
- clang-format 22.1.8 and clang-tidy 22 clean on the touched files;
  `verify_documentation` OK; `github_contract_fixture_security_tests` OK with
  the new `markdown-parity-v1` pack.
- `markdown-parity-v1` is served in test builds like `intraline-v1`
  (`src/app` fixture tables); the round-trip composition contract is resealed
  for the two app digests (`960a53b7...`, `8e2f29b9...`), and all nine
  composition contracts pass, `application_composition_contract` included.
  The merge must reseal again on the merged tree if main's app digests moved.
- `fuzz_markdown_view` campaign 5: 15 minutes, 360,876 runs, clean, with the
  target now also requiring that no mark reaches the document.
- Live sample: github.com/MikeMarcin/BPR-Test/pull/72 carries the same
  bodies; GitHub's page was captured headlessly (fresh profile) for the
  review sheets.

## Continuation

- **Candidate:** branch `bpr-1543-gfm-parity` at `cb73ad000`, not merged.
  Frozen root `/Users/jaeger/bpr-1543-markdown-parity-v001.e6tm3nsa`
  (executable `02d0d2a3...`), launched by `python3
  /Users/jaeger/bpr-1543-markdown-parity-v001.e6tm3nsa/launch.py`; it opens
  `markdown-parity-v1` on its Overview.
- **Owner review:** session `6eb0a38e0bc7f9c5a1261b07f3e47c55`, state
  `/Users/jaeger/.mask/attended-review-sessions/bpr-1543-markdown-parity-v001-20261016`,
  cursor 15; one Accept/Reject each for comments, line breaks, alerts,
  footnotes and underscore emphasis, each with GitHub's page beside the app
  in Light and Dark.
- **Owner verdicts (2026-10-16):** all five accepted with no notes —
  comments `45b931cb`, line breaks `a764a2c3`, alerts `b908869e`, footnotes
  `19401136`, underscore emphasis `e6618af8` — handed back at cursor 21, which
  is consumed. The review host is stopped, the frozen root removed, and
  BPR-Test#72 closed with its branch deleted.
- **Landing (coordinator order):** after the inline-images work
  (`bpr-markdown-images`, RFC-BPR-0084) is on main. The resolution is ready on
  branch `bpr-1543-landing-trial` (`6d077f528`, this branch merged onto the
  images branch at `7d2295c85`): parity presentation runs before images are
  placed, `focus_link` keeps the image description, and the fixture tables,
  docs and fuzz ledger keep both sides. Its full dev suite: 745 of 755, the 10
  others being the stale `NEXT_ID` base (3), the unresealed contracts (2), a
  load flake that passes alone (`application_diff_host`) and the four rows
  main fixed in BPR-1552. `fbdb893e2` (the fuzz target's helper renamed apart
  from `bpr::application`, which the images headers made ambiguous) must come
  along.
- **Next:** when `7d2295c85` is an ancestor of main, merge main into
  `bpr-1543-landing-trial`, take the fuzz fix, reseal the round-trip
  composition contract on that tree, run the full dev suite once and the
  documentation gate, land it on main and finish this card. That makes
  [[BPR-1544]] eligible.
