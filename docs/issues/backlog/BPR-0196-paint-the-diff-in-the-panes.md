---
id: BPR-0196
title: The diff panes paint no diff
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# The diff panes paint no diff

Found by the product owner rejecting all three subjects of the
`selection_contrast_remedy` attended review on 2026-08-06. Their note:

> this looks like a list box not a document view, i'm not sure if that's a
> real issue or just a side effect of the text chosen.

It is not the text.

## What was measured

Against `src/adapters/qt/src/diff_workbench.cpp`:

| Measurement | Result |
|---|---|
| `setBackground` calls in the file | **1** — the selection band |
| `setForeground`, `setFormat`, `mergeCharFormat` calls | **0** |
| `appearance_role::` values referenced | **0** of 45 |

The core's palette declares seven diff-presentation roles —
`diff_added_background`, `diff_removed_background`,
`diff_intraline_added_background`, `diff_intraline_removed_background`,
`diff_moved_background`, `diff_hunk_header_background`,
`diff_hunk_header_text` — and about twenty-three `syntax_*` roles. **The panes
apply none of them.** Even the selection band arrives through
`QPalette::Highlight` rather than through its role.

So the panes render undifferentiated plain text. Added lines, removed lines,
replaced spans, moved code, and hunk headers are all painted identically:
not at all.

## Against the reference the owner supplied

p4merge, comparing two real headers:

| Element | p4merge | Here |
|---|---|---|
| Syntax colouring | yes | none — `packaged_syntax_classifier` has zero symbols in the binary (BPR-0185) |
| Added and removed line tint | yes | none |
| Intraline emphasis on changed spans | yes | none |
| Moved-code indication | — | none |
| Hunk headers | — | none |
| Between-pane connectors | yes | **yes**, `diff_link_map` paints these |
| Selected line | solid band, readable foreground | 1.06:1 wash |

One of seven.

## Why this reopens BPR-0194 rather than sitting beside it

BPR-0194 asked whether the selected line should be an opaque band or a
translucent wash. The whole trade-off in that question is **what the marking
obscures** — a band replaces the line's colouring, a wash tints it.

Right now it obscures nothing, because nothing is painted. The question could
not be answered on the evidence offered, and the owner was right to refuse it.
Decide the selection treatment once a selected line has an added-line tint,
intraline emphasis, and syntax colour underneath it to compete with.

## What this card must not become

Do not paint colours the core did not decide. The semantic model already
computes alignment kinds, change groups, and intraline spans; the pane's job is
to render what `core::diff_semantics` decided, not to re-derive it. A second
opinion about which lines changed is the defect RFC-BPR-0005 already forbids in
its reuse rules.

Do not hard-code colours. Every one of these is a declared
`core::appearance_role` and must come from the composed palette, which BPR-0183
made reachable.

## Required direct evidence

- Added, removed, replaced, and moved lines each carry their role's background
  on the composed surface, read off the rendered pixels rather than off the
  format that was supposed to be applied.
- Intraline emphasis marks exactly the spans `core::diff_semantics` computed,
  compared against the model rather than eyeballed.
- Each background is the composed palette's colour for its role, so a palette
  change moves it and a hard-coded value fails.
- The pane still shows one shared vertical scrollbar, no synthetic rows, and
  the bounded-widget counts RFC-BPR-0005 recorded at 60, 6,000, and 60,000
  lines are re-measured and either hold or are restated.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
