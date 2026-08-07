---
id: BPR-0185
title: Link the syntax adapter and highlight the diff panes
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# Link the syntax adapter and highlight the diff panes

RFC-BPR-0006's headline feature did not reach a user.
`BetterPullReview::AdapterSyntax` was a correctly declared alias at
`src/adapters/syntax/CMakeLists.txt:13` that the composition never linked, so
`packaged_syntax_classifier` had zero symbols in the shipped binary. The diff
panes contained no highlighting: `diff_workbench.cpp`'s only `Highlight` was
`QPalette::Highlight`, which colours a selection.

## What this slice did

Linked the adapter into the composition, gave the appearance host the real
classifier, and painted the resulting token roles through the profile's
semantic colours.

### The pane paints a foreground, not another selection

BPR-0196 painted the diff with `QTextEdit::ExtraSelection`, and the obvious
move was to add syntax to that list. It is the wrong mechanism twice over.

Syntax is a **foreground** and the diff bands are **backgrounds**, so they must
compose rather than compete — an added line keeps its tint while its keywords
keep their colour. And an extra selection is walked on every paint, so a
screenful of C++ would add several hundred list entries to a list BPR-0196 had
just finished bounding.

Formats go on each block's own `QTextLayout` instead, which is what a
highlighter writes to. `QTextCursor::setCharFormat` was the other candidate and
is worse: it mutates the document once per span — an edit, an undo entry, and a
relayout apiece — to express a change that alters no text.

### Ordering is a precondition, not an assumption

Painting finds the visible window with `lower_bound` over the span list. That
needs the spans ordered by line, which is how the classifier produces them: one
pass over the blob, line by line.

Rather than trust it, `adopt_token_spans` checks it once on adoption and
**refuses the whole list** if it does not hold. Sorting a copy would double the
memory of the one thing this design deliberately shares, and scanning linearly
would reintroduce exactly the unbounded repaint BPR-0196 removed. Refusing
makes a classifier that broke the contract visible instead of slow.

### The path travels beside the key, not inside it

Choosing a definition needs the name a file was given, and
`file_comparison_key` identifies a file by digest. Putting a path into the key
would have given the same file a second identity, so `request_diff` carries it
as a separate argument used for one purpose.

## What was measured

| Claim | Evidence |
|---|---|
| The classifier is in the product | `packaged_syntax_classifier` moved from `NOT_COMPOSED` to `COMPOSED` in `verify_composed_hosts.py`; `COMPOSED_HOSTS_OK: 6 composed, 8 still absent` |
| Its constructor runs | `SYNTAX_CLASSIFIER_COMPOSED_OK` from the running application, because a symbol proves the link and not the constructor |
| A pane took its answer | `SYNTAX_CLASSIFICATION:2:2` — both blobs of the fixture comparison classified, both adopted by the panes |
| A theme change repaints without reparsing | `SYNTAX_THEME_REPAINT_OK` — a mode switch on the running application with a classified comparison on screen leaves `classification_submissions()` where it was |

### Non-vacuity

Each break compiled; the error count was checked before the failure was
believed. Three of this session's earlier break attempts failed to build and
`ctest` reported `Passed` against a stale binary, which is why the count is
checked rather than the exit status.

| Break | Failure |
|---|---|
| Use the byte offset as a character index | `a span behind a two-byte character starts at its character index, not at its byte offset` |
| Scan the span list from the front instead of searching | `a file ten times longer visits no more than a constant factor more classified spans` |
| Do not copy the palette's role table into the pane | `a line the complete-blob pass classified as comment is painted as one` |
| Index spans by window offset instead of blob line | `the marker line is painted at all`, in the composition suite, plus the bounded-formatting guard |
| Stop requesting classification in `request_diff` | `SYNTAX_CLASSIFICATION:0:0` and `SYNTAX_THEME_REPAINT_FAILED` |

### The bound a block count cannot see

`SyntaxBlocksFormatted` alone is satisfied by a painter that walks every span
in the file to find the visible ones: it formats exactly the same blocks and
costs time proportional to the file. `SyntaxSpansVisited` is counted
separately for that reason, and the assertion is taken **at the far end of the
file** — at line zero the visible window begins where the span list begins, so
a scanning painter and a searching one are indistinguishable. That is precisely
how a scan-shaped cost hides from a measurement taken on arrival.

This is the same lesson BPR-0196 recorded, met a second time in a different
place: a counter that moves for the right reason on the easy case still proves
nothing about the hard one.

## What this card does NOT deliver

- **No token colour in the composed capture.** The fixture repository's file is
  `review.txt`, so the packaged catalog selects plain text and every span comes
  back `syntax_normal`, which the pane deliberately leaves unpainted. The
  composed evidence is therefore that the classification *arrives*, not that it
  is colourful. Renaming the fixture file changes every tree and commit OID in
  it, which is a regeneration and belongs to **BPR-0197**.
- **No per-category emphasis.** `appearance_profile::code_font_for` exists and
  the bold and italic switches are honoured nowhere, because the classifier maps
  packaged text styles onto the core's role vocabulary and drops emphasis at
  that boundary. Deciding which roles are bold is a presentation decision
  nobody has made.
- **No definition selection surface.** The definition follows the file name and
  a reviewer cannot override it.

## Required direct evidence

- `nm` on the shipped binary reports `packaged_syntax_classifier` symbols.
  **Met**, and enforced going forward: it is on `verify_composed_hosts.py`'s
  COMPOSED list, so losing the link fails the gate.
- A C++ blob shown in a pane carries token roles, read off the pane's rendered
  formats rather than off the classification that was supposed to reach it.
  **Met** by `qt_syntax_pane_composition`, which links both adapters, runs the
  real `packaged_syntax_classifier` over a C++ blob, and reads the colour off
  `QTextBlock::layout()->formats()`.
- A hunk opening inside a multiline comment is highlighted as the complete blob
  classifies it, on the composed surface. **Met**: the same suite classifies a
  blob whose block comment opens four hundred lines above the assertion, scrolls
  until the opening delimiter is off screen, asserts that it is off screen, and
  then asserts the comment colour.
- Switching theme repaints without advancing the host's classification
  submission count. **Met at the component and at the product**:
  `SYNTAX_THEME_REPAINT_OK` on the running application, and the Qt suite's own
  half — the panes repaint in the new palette while `TokenSpanAdoptions` holds.
- A blob above `maximum_classified_blob_bytes` is refused rather than partially
  classified, and the pane shows unhighlighted text rather than text whose tail
  is silently unclassified. **Met**: the classifier reports `limit_exceeded`
  with no spans, and a pane with no spans paints no token colour.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered. **Met**, tabulated above.

## The placeholder next door

`AdapterAI` is still an empty `INTERFACE` library in the composition's link
list, holding a place for RFC-BPR-0011 while this card's real adapter was
missing. It is now the only placeholder there. Left alone deliberately: RFC-0011
has seven cards in `backlog/` and removing its slot would be churn one of them
immediately undoes.
