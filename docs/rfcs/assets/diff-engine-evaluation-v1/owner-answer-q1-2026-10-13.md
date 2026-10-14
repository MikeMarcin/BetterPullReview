# RFC-BPR-0082 owner answer to Q1, 2026-10-13

Live source: [RFC-BPR-0082](../../RFC-BPR-0082-diff-engine-evaluation.md) at
mandate `diff-engine-evaluation-r2`, after the
[Q2 answer](owner-answer-2026-10-13.md). Given by the owner in the coordinating
session on 2026-10-13.

## Answer

**Q1, Accept with a condition:** "yes, if xdiff is dynamically linked as its
own shared library, as Qt is". The owner also asked for shape L to be
re-measured with corrected whitespace flags (the CRLF and trailing-whitespace
mapping behind its two correctness refusals) and adopted only if every gate
passes.

## Affected text

Only Q1 changes. Before:

> **Q1. Is linking LGPL xdiff acceptable?** Statically linking LGPL-2.1
> code into a distributed desktop app brings relinking and source-offer duties
> beyond the current Qt (dynamic) and KSyntaxHighlighting arrangements. Shape L
> is measured but cannot be adopted until the owner answers.

After:

> **Q1. Is linking LGPL xdiff acceptable?** Statically linking LGPL-2.1
> code into a distributed desktop app brings relinking and source-offer duties
> beyond the current Qt (dynamic) and KSyntaxHighlighting arrangements. Shape L
> is measured but cannot be adopted until the owner answers. *Answered
> 2026-10-13: Accept, if xdiff is dynamically linked as its own shared
> library, as Qt is* (see "Owner answer" above). Static linking stays
> refused. Shape L is re-measured with corrected whitespace flags and is
> adopted only if every gate passes.

## Preserved and consequential

No threshold, decision rule or acceptance criterion changes. Q3 stays open.
Shape L stays in an adapter behind a host port, as adopted, with xdiff built as
its own shared library that the adapter links dynamically. Adoption still
needs every gate to pass on the corrected-flag re-measurement, and the
LGPL-2.1 notice, source offer and corresponding-source arrangements that
dynamic linking keeps (as for Qt).
