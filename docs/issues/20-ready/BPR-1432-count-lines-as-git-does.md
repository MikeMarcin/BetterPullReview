---
schema: "implement-rfc.card.v1"
id: "BPR-1432"
title: "Count source lines as Git does, so a lone CR stays inside its line"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
---

# Count source lines as Git does, so a lone CR stays inside its line

The 2026-10-13 audit found that the decoded diff ended a line at LF, CRLF
and a lone CR ([diff_compute.cpp](../../../src/core/src/diff_compute.cpp)),
while Git and GitHub end a line only at LF. A comment anchor sends the
decoded line index plus one as its GitHub `line`
([github_mutation_contract.cpp](../../../src/adapters/github/src/github_mutation_contract.cpp)),
so in a file with a lone CR every comment after it named the wrong GitHub
line. The suggestion editor and the Review Cycles feedback, share and
suggestion splitters split the same way, and Qt's document starts a block at
a lone CR, which is why the views agreed with the wrong count.

## Acceptance

- AC1: The decoded diff ends a line only at LF; a CR immediately before it
  forms a CRLF ending and a lone CR is line content. The `cr` line ending no
  longer exists, so no producer can emit it.
- AC2: The suggestion editor and the Review Cycles feedback, share and
  suggestion line counts follow the same rule, so a note's or suggestion's
  line agrees with the diff's line.
- AC3: Source panes, the unified view, line details and the suggestion
  editor show a lone CR as U+240D inside its line. Pane block numbers remain
  the decoded line numbers, so anchors and reveals are unchanged elsewhere.
- AC4: Find reads CRLF as LF and keeps a lone CR as content, in documents
  and in queries.

The diff algorithm revision is unchanged: it is part of the persisted file
comparison digest, and only files containing a lone CR change shape.
Copying from a pane copies the U+240D symbol, not the CR.

## Evidence

Pending.
