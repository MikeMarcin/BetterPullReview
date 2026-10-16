---
schema: "implement-rfc.card.v1"
id: "BPR-1543"
title: "Markdown reader GitHub parity: comments, line breaks, alerts and footnotes"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner decision 2026-10-16: close the GitHub-parity gaps in PR description and comment Markdown, as two sequential cards (this is card A)","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 1 (2026-10-16)","criteria":["AM1-01","AM1-02","AM1-03","AM1-04","AM1-08"],"related_cards":["BPR-1544","BPR-1545","BPR-1548"],"reservation":"docs/issues/_reservations/BPR-1543--BPR-1544--05e6fe38f3d4.json","design_skill":"frontend-design:frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"11c131da0","evidence":{"kind":"command","detail":"qt_markdown_github rows compare comments.md and comments-unclosed.md with GitHub's Markdown API output: hidden text absent, code comments kept, an unclosed block hides the rest; negative control: Qt alone shows the comment. Passed on the merged tree."}},{"criterion":"AC2","outcome":"pass","revision":"11c131da0","evidence":{"kind":"command","detail":"Line separators equal GitHub's <br> count in breaks.md (7) and the PR template (4); lists, quotes and tables keep their structure; code, code spans, headings and the tab repair are unchanged."}},{"criterion":"AC3","outcome":"pass","revision":"11c131da0","evidence":{"kind":"command","detail":"Alert kinds and titles in GitHub's order; marker-with-text and in-list stay quotes; heading/table stay inside; title contrast >=4.5 (>=7 high contrast) in four themes; bar pixel colour checked."}},{"criterion":"AC4","outcome":"pass","revision":"11c131da0","evidence":{"kind":"command","detail":"References [1],[2],[1] as GitHub numbers them; back references and items match; Tab/Enter and click move within the body with zero application gestures; negative control: Qt alone links the one-word definition."}},{"criterion":"AC5","outcome":"pass","revision":"11c131da0","evidence":{"kind":"command","detail":"Reserved-mark guards and leftover sweep; 64 KiB body in 45 ms; full dev suite 749/755 on e37dbe99f with the six load-sensitive rows (large review x5, hide-drawers key) passing alone; 138 focused rows and all nine composition contracts on the final tree; fuzz campaign 5 clean."}},{"criterion":"AC6","outcome":"pass","revision":"11c131da0","evidence":{"kind":"command","detail":"clang-format 22 check passed on 1569 files; clang-tidy clean on touched files; verify_documentation DOCUMENTATION_OK; round-trip composition contract resealed over main ec097d8dc."}},{"criterion":"AC7","outcome":"pass","revision":"cb73ad0001560adb0b650a50764f6dc6378e3a23","evidence":{"kind":"observation","detail":"Owner accepted all five questions with no notes in attended session 6eb0a38e0bc7f9c5a1261b07f3e47c55 (comments 45b931cb, line breaks a764a2c3, alerts b908869e, footnotes 19401136, underscore emphasis e6618af8), handed back at cursor 21 on 2026-10-16."}}]
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
- **Landed (2026-10-16):** `11c131da0` merges `bpr-1543-landing-trial` onto
  main at `ec097d8dc` (after RFC-BPR-0084 at `5e340ee21` and BPR-1558). Full
  dev suite on `e37dbe99f`: 749 of 755, the six others
  (`application_large_review_{responsiveness,launch,pathological,history}`,
  `large_review_replay_contract`, `qt_hide_drawers_native_menu_key`) passing
  when rerun serially; they are load-sensitive budget rows untouched by this
  card. On the final tree after main moved: the round-trip contract resealed
  (`87ad560e...`, `465b7af3...`), all nine composition contracts and 138
  focused rows pass, documentation gate OK.
