---
schema: "implement-rfc.card.v1"
id: "BPR-1544"
title: "Markdown reader GitHub parity: details, sanitised HTML and emoji shortcodes"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-1543"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner decision 2026-10-16: close the GitHub-parity gaps in PR description and comment Markdown, as two sequential cards (this is card B, after BPR-1543)","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 1 (2026-10-16)","criteria":["AM1-05","AM1-06","AM1-07","AM1-08"],"related_cards":["BPR-1543","BPR-1545","BPR-1548"],"reservation":"docs/issues/_reservations/BPR-1543--BPR-1544--05e6fe38f3d4.json","design_skill":"frontend-design:frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f8194005e","evidence":{"kind":"command","detail":"qt_markdown_github_html rows: details summaries equal GitHub's in order, only the open one open; closed content hidden; Tab/Enter and click toggle with accessible 'Collapsed/Expanded section'; Tab skips closed links; hidden tables lose borders; zero application gestures. Owner accepted (d37ed8c5)."}},{"criterion":"AC2","outcome":"pass","revision":"f8194005e","evidence":{"kind":"command","detail":"HTML sample formats, alignment, structure and links match GitHub's output; hostile body and 300 generated bodies: no resource load, no javascript/data/file/credential link, no handler/src/background/style attribute in the document, no leaked mark; core markdown_html_link_kept tested. Owner accepted inline (5096ee4f), structure (805b703d), <u> plain (e7746fdc) and unsafe HTML (1e47cfa0)."}},{"criterion":"AC3","outcome":"pass","revision":"f8194005e","evidence":{"kind":"command","detail":"Emoji line identical to GitHub's; unknown, coded, boundary-less and URL shortcodes stay literal; table generated from pinned gemoji 4.1.0 (MIT, retained). Owner accepted (490f9cbf)."}},{"criterion":"AC4","outcome":"pass","revision":"f8194005e","evidence":{"kind":"command","detail":"Full dev suite 752/759 on 5e64e9879, the seven others passing serially (composed <b> expectation updated); 123 focused rows and nine composition contracts on the final tree; contract resealed over main 7132f6356; clang-format 22 and tidy clean; documentation gate OK; fuzz campaign 6 clean."}},{"criterion":"AC5","outcome":"pass","revision":"fbdb893e2458ea372c3add20c345c08c0b5d1a39","evidence":{"kind":"observation","detail":"Owner accepted all six questions in attended session 09a88455155e35e5b06c2df811844364, handed back at cursor 25 on 2026-10-16; the details note about arrow size was answered by measurement (proportion matches GitHub's within 0.01-0.07 of cap height)."}}]
---

# BPR-1544: Markdown reader GitHub parity: details, sanitised HTML and emoji shortcodes

Card B of [RFC-BPR-0036 owner amendment 1](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-1--2026-10-16-github-parity),
after [[BPR-1543]].

## Scope

- `<details>`/`<summary>`: a collapsible section, closed by default and
  keyboard accessible. If a true collapsible block is infeasible in
  `QTextDocument`, the reader's own block model carries it; the choice is
  decided and documented here.
- GitHub's sanitised HTML subset: `br`, `sub`, `sup`, `kbd`, `b`, `strong`,
  `i`, `em`, `u`, `s`/`del`, `code`, `pre`, `p`/`div` with `align`, `a href`
  (only through the existing core admission), `hr`, `blockquote`,
  `ul`/`ol`/`li`, `h1`–`h6` and table tags. Everything else stays literal or is
  dropped per a documented policy. Never a resource load, script, style, event
  attribute, form or frame. `<img>` and `<picture>` belong to
  [RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md): leave its
  hooks, do not implement them.
- Emoji shortcodes (`:+1:` and the rest of GitHub's common set) from a
  generated static data file; no runtime fetch.

## Acceptance

- **AC1 (AM1-05):** details render closed, open and close by keyboard and
  pointer, and are announced as collapsible.
- **AC2 (AM1-06):** the subset renders with its meaning; other HTML is literal
  or dropped as documented; hostile-HTML security tests prove RFC-BPR-0036
  AC-03 still holds.
- **AC3 (AM1-07):** shortcodes render from the generated table; unknown and
  coded ones stay literal.
- **AC4:** gates and the preserved reader rows, as on [[BPR-1543]].
- **AC5 (AM1-08), owner:** one Accept/Reject per visible construct on a
  frozen build beside GitHub's rendering in Light and Dark.

## Design

The `frontend-design` skill was loaded first. Direction: GitHub's own
rendering, inside the reader's inherited palette and fonts.

- **`<details>`:** real collapsing through Qt's block visibility, not cut and
  paste: a section's lists, code and tables keep their structure, and a hidden
  table gives up its borders. The summary leads with ▶/▼ at 0.7 of the text,
  in the text colour, and is an in-body toggle: Tab, Enter and click, with
  "Collapsed section"/"Expanded section" as its accessible description. Tab
  skips links in a closed section. GitHub's "Details" when no summary.
- **HTML:** a bounded tag recognizer and GitHub's allow-list in a private
  `markdown_github_html` unit. Inline formatting travels as the reader's
  marks, so Markdown inside it still reads; top-level structure blocks are
  sanitized (only `align`, `colspan`, `rowspan`) and placed through Qt's HTML
  import. `<a href>` keeps its link only when core's new
  `markdown_html_link_kept` says it could be admitted. The tag filter's tags
  stay text; `<img>`, `<picture>` and an `<a>` around one stay for
  RFC-BPR-0084. `<mark>` uses GitHub's attention tint; `<kbd>` is a key cap.
- **Emoji:** gemoji 4.1.0's 1,913 aliases (MIT, `third_party/gemoji`, in the
  dependency inventory and notices), generated into a compiled table.
- **Found against GitHub:** GitHub strips `<u>` (the amendment named it); the
  reader matches GitHub and asks the owner. A credential-bearing link stays
  text (core refuses it; GitHub keeps it). An unclosed tag ends with its
  paragraph.

## Evidence (branch `bpr-1544-gfm-html`, stacked on BPR-1543's landing branch)

- `qt_markdown_github_html_{1_0,1_25,2_0}`: details summaries, open states,
  visibility, keyboard/click toggling, Tab skipping and table borders;
  the HTML sample's formats, alignment, structure and links against GitHub's
  output; the emoji line identical to GitHub's; the HTML PR template's
  sections, keys, tables and emoji; hostile HTML (scripts, frames, forms,
  handlers, styles, `javascript:`/`data:`/`file:`/credential links, encoded
  markup) and 300 generated hostile bodies: no resource load, no forbidden
  link, no handler or resource attribute in the document, no leaked mark;
  a 62 KB body of 204 sections in 147 ms. `core_markdown_navigation` covers
  `markdown_html_link_kept`.
- clang-format 22 and clang-tidy 22 clean; package and fixture-security
  tests pass; fuzz campaign 6 clean.
- **Owner review (pending):** session `09a88455155e35e5b06c2df811844364`,
  state `/Users/jaeger/.mask/attended-review-sessions/bpr-1544-markdown-html-v001-20261016`,
  cursor 18; six questions (details, inline HTML, HTML structure, `<u>` plain,
  unsafe HTML, emoji) on candidate `fbdb893e2`, frozen root
  `/Users/jaeger/bpr-1544-markdown-html-v001.hyrv2qbm` (executable
  `93239c1f...`, fixture `markdown-html-v1`); live sample BPR-Test#77.
- **Owner verdicts (2026-10-16):** all six accepted (details `d37ed8c5`,
  inline HTML `5096ee4f`, HTML structure `805b703d`, `<u>` plain `e7746fdc`,
  unsafe HTML `1e47cfa0`, emoji `490f9cbf`), handed back at cursor 25. Notes:
  on details, "Do our disclosure arrows seem small? Is that just a font
  difference?"; on emoji, "yes keep these", with a separate request (the
  comment editor's label codes) that BPR-1565 carried out.
- **The arrows, measured:** GitHub's marker (BPR-Test#77, headless Chrome)
  is 8×9 px beside a 10 px capital at 100% and 16×18 beside 20 at 200%: 0.90
  of the cap height. The reader's ▶ is 8×8 beside 9 (0.89) and 16×15 beside
  18 (0.83). The proportion matches; the absolute difference is the reader's
  smaller body text. The review sheets were composed before the triangle
  became ▶ at 0.7 of the text (the frozen build already had it), so the sheet
  showed the smaller ▸. No enlargement was needed.
- **Landed:** `f8194005e` merges `bpr-1544-gfm-html` onto main at `7132f6356`.
  Full dev suite on `5e64e9879`: 752 of 759; the seven others passed when
  rerun serially, three of them (`qt_composed_github_scale_*`) after their
  literal-`<b>` expectation was updated to the accepted rendering
  (`2c38bd0fe`) and four load-sensitive budget rows. On the final tree:
  round-trip contract resealed (`88e47223...`, `8702dac0...`), all nine
  composition contracts and 123 focused rows pass, documentation and format
  gates OK.
