---
schema: "implement-rfc.card.v1"
id: "BPR-1777"
title: "RFC-BPR-0081's re-run trigger fired: the identifier refinement is 847 lines"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1777--BPR-1777--494d5971aa76.json","origin":"Found during BPR-1445's closeout recheck of RFC-BPR-0081, 2026-10-27","related_rfcs":["RFC-BPR-0081"],"related_cards":["BPR-1445","BPR-1474","BPR-1710","BPR-1711","BPR-1714"],"kind":"owner-decision"}
---

# BPR-1777: RFC-BPR-0081's re-run trigger fired: the identifier refinement is 847 lines

## Finding

RFC-BPR-0081's [report](../../verification/RFC-BPR-0081.md) gives several
conditions for running the parser evaluation again. One of them is "if the
refinement reaches 800 lines". That condition is now met.
`src/core/src/cpp_identifier_context.cpp` is 847 lines at `223098b11`.

| Revision | Lines | What changed |
|---|---|---|
| `530d92eaf` (RFC base) | 539 | adapter location |
| `4add47f63` ([[BPR-1474]]) | 698 | the RFC's own "extend" fixes, inside its 800-line cap |
| `381ec842b` ([[BPR-1619]]) | 698 | moved into core unchanged |
| `9933b54e2` ([[BPR-1710]]) | 776 | `std::` and other evidenced qualifiers read as namespaces |
| `916adaaa3` ([[BPR-1711]]) | 788 | `std` after `->` and in a qualified class head |
| `ff56e0752` ([[BPR-1714]]) | 847 | a Settings list of known C++ namespaces |

The RFC's own work stayed inside the cap. The cap was crossed by three
later cards, outside RFC-BPR-0081's scope, so no RFC criterion fails and
the RFC closed through [[BPR-1445]]. The trigger still stands, and nobody
has decided what to do about it.

One thing has changed since the macOS run: the candidate's contract twin
now fails first on the `std::` namespace assertion that BPR-1710 added
("std:: is a namespace and the grammar's role for the name after it is
kept"), before it reaches the enumerator-attribute case the report names.
T was not updated for these later features. That matters only if the
evaluation is re-run.

## Acceptance

- AC1: The owner answers one question, as Accept/Reject in a short attended
  session or in chat. The question: re-run RFC-BPR-0081's evaluation
  against the current refinement, or keep the heuristic and change the
  trigger (raise or retire the 800-line condition). If the answer is a
  re-run, it measures macOS, Windows x64 and Linux x64. The 2026-10-26
  [amendment](../../rfcs/assets/cpp-identifier-parser-evaluation-v1/proposed-owner-amendment-2026-10-26.md)
  limited only the original run to macOS.
- AC2: The decision is recorded in the report's re-run section and on this
  card. Nothing is implemented, measured or adopted before it.
