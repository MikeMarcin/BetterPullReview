---
schema: "implement-rfc.card.v1"
id: "BPR-1544"
title: "Markdown reader GitHub parity: details, sanitised HTML and emoji shortcodes"
status: "Ready"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-1543"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner decision 2026-10-16: close the GitHub-parity gaps in PR description and comment Markdown, as two sequential cards (this is card B, after BPR-1543)","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 1 (2026-10-16)","criteria":["AM1-05","AM1-06","AM1-07","AM1-08"],"related_cards":["BPR-1543","BPR-1545","BPR-1548"],"reservation":"docs/issues/_reservations/BPR-1543--BPR-1544--05e6fe38f3d4.json","design_skill":"frontend-design:frontend-design"}
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
- **Next:** claim once BPR-1543 is Done, merge this branch onto main, reseal,
  and finish on the owner's verdicts.
