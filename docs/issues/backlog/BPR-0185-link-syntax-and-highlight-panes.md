---
id: BPR-0185
title: Link the syntax adapter and highlight the diff panes
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# Link the syntax adapter and highlight the diff panes

RFC-BPR-0006's headline feature does not reach a user.
`BetterPullReview::AdapterSyntax` is a correctly declared alias at
`src/adapters/syntax/CMakeLists.txt:13` that the composition never links, so
`packaged_syntax_classifier` has zero symbols in the shipped binary. The diff
panes contain no highlighting: `diff_workbench.cpp`'s only `Highlight` is
`QPalette::Highlight`, which colours a selection.

## What this slice does

Link the adapter into the composition, hand it to the appearance host as the
classification port, and paint the resulting token roles in the panes through
the profile's semantic colours.

Beside the omitted adapter in the link list sits `AdapterAI`, an empty
`INTERFACE` library declared at `src/adapters/CMakeLists.txt:6`. It is a
placeholder for RFC-BPR-0011 and is out of scope here, but a link list that
carries a placeholder while omitting a real adapter is worth one look while
this file is open.

## What the classification must not become

The host caches spans keyed by blob identity and definition version alone, so
that switching theme, palette, mode, or font cannot re-run lexical
classification. Painting must read that cache, not classify on its own. A pane
that reclassifies to repaint would silently defeat the cache the RFC requires
and the ledger already claims.

Classification is a complete-blob pass. A hunk beginning inside a multiline
comment or string is classified by the state that opened the construct, which
is why it cannot be done hunk-locally — that is already proven at the component
and must survive composition.

## Required direct evidence

- `nm` on the shipped binary reports `packaged_syntax_classifier` symbols.
- A C++ blob shown in a pane carries token roles, read off the pane's rendered
  formats rather than off the classification that was supposed to reach it.
- A hunk opening inside a multiline comment is highlighted as the complete blob
  classifies it, on the composed surface.
- Switching theme repaints without advancing the host's classification
  submission count. The counter already exists;
  `classification_submissions()` advances exactly when lexical work was
  unavoidable.
- A blob above `maximum_classified_blob_bytes` is refused rather than partially
  classified, and the pane shows unhighlighted text rather than text whose tail
  is silently unclassified.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
