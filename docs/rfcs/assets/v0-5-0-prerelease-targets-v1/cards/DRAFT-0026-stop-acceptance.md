---
schema: "implement-rfc.card.v1"
id: "DRAFT-0026"
title: "Accept a tour stop the way a file is accepted"
status: "Backlog"
rfcs: ["RFC-DRAFT-0009"]
depends_on: ["DRAFT-0025"]
---

# Accept a tour stop the way a file is accepted

## Scope and acceptance

Implements
[RFC-DRAFT-0009](../RFC-DRAFT-0009-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md).

Record in the core what the author accepted for a stop (its text, its
destination, and the code there and at each reference in its text) and decide
Accepted, Not accepted yet and Changed since accepted from it. Make Accept on
the current stop, Save of an edit and Accept on the stop's own proposal accept
it, and nothing else. Keep an unchanged stop Accepted across a new head and
turn yellow only the stops whose material changed. Require every stop Accepted
to publish, replacing the Ready mark, the Head changed chip and the separate
confirmation of lines. Carry saved drafts across: a stop that was Ready and
reaffirmed stays Accepted, and every other stop is Not accepted yet.

Covers AC02 to AC05 and the ring states of the owner's look in AC08. The owner
accepted these rules as drawn in the stop mockup on 2026-11-10; the look at
the running stop is still owed. This amends
publication eligibility under RFC-BPR-0042, 0043 and 0096: give it critical
review, since it decides what may be published.

This is part of `tour-stops` in the [plan](../milestone-plan.json).
