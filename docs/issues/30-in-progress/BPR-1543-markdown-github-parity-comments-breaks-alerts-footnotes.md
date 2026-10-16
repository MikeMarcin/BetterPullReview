---
schema: "implement-rfc.card.v1"
id: "BPR-1543"
title: "Markdown reader GitHub parity: comments, line breaks, alerts and footnotes"
status: "In Progress"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner decision 2026-10-16: close the GitHub-parity gaps in PR description and comment Markdown, as two sequential cards (this is card A)","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 1 (2026-10-16)","criteria":["AM1-01","AM1-02","AM1-03","AM1-04","AM1-08"],"related_cards":["BPR-1544","BPR-1545","BPR-1548"],"reservation":"docs/issues/_reservations/BPR-1543--BPR-1544--05e6fe38f3d4.json","design_skill":"frontend-design:frontend-design"}
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

## Evidence

Implementation is on branch `bpr-1543-gfm-parity`.
