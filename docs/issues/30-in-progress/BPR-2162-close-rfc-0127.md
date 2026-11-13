---
schema: "implement-rfc.card.v1"
id: "BPR-2162"
title: "Verify and close RFC-BPR-0127"
status: "In Progress"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2157", "BPR-2158", "BPR-2159", "BPR-2160", "BPR-2161"]
closeout_for: ["RFC-BPR-0127"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"}}
---

# BPR-2162: Verify and close RFC-BPR-0127

RFC base: `f85b216ecc7d4a7df2c7c5c263a0cf8be1f3d93a`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** The right drawer's tabs are Comments, Tour and Agent, and the Overview drawer has no Notes tab; N on a selected line opens the composer on Local in Comments. | Pass (engineering); owner look accepted (12534e15) | `d85b98576` |
| **AC02:** On a line that has a published thread, a Local reply and a draft reply of mine, one card shows all three in the order defined, each with its state's mark, and the line has one gutter mark whose accessible text names all three. | Pass (engineering); owner look accepted (12534e15) | `d85b98576` |
| **AC03:** With Local comments present on lines, in threads, on a file and on the pull request, Add single comment, the review's Submit, Add all, Send and Publish response write nothing of them: the fixture provider receives no Local body, and each is still Local afterwards. | Pass | `b79461670`; rerun at `a7db82152` |
| **AC04:** Keep local on a draft in the review lowers the review's pending count by one, removes it from every "not in this review" list, and keeps its text, line or range and reply target; Make draft on it returns a draft of the same kind at the same place, not in the review. | Pass | `64476cd4d`; rerun at `a7db82152` |
| **AC05:** Keep local on a draft held by a submitted review or being published, and Make draft where my draft already stands, are refused with their reasons and change nothing; no action offers a state change on a published comment. | Pass | `d85b98576`; rerun at `a7db82152` |
| **AC06:** A Local comment on the pull request, on a whole file, on another file, and on a line the shown comparison does not place each appears where "Where a Local comment can be" says, the last with its reason; none is shown at a line it was not written on, and Make draft on it is offered only as Make draft on line N with a line selected. | Pass (engineering; other file and outdated in Qt only); owner look v002 Reject reworked, v003 pending | `b92a35101` |
| **AC07:** Threads lists Local comments in their files' groups and its state filter's four choices show exactly the rows of that state with matching counts; the filter is not restored on reopening; under Local, Hide resolved and Only new hide no Local comment. | Pass (engineering); owner look v004 pending | `b0ea75ce8`; rerun at `a7db82152` |
| **AC08:** A file's Comments cell on Overview shows a Local chip with its count beside the drafts chip, a folder shows the sum, the chip's activation opens Threads on Local at that file's first, and Overview has no Note column or Private notes row. | Pass (engineering); owner look v004 pending | `b0ea75ce8`; rerun at `a7db82152` |
| **AC09:** Finish your review never lists a Local comment as pending or as not in the review; Keep Local on a not-in-review row removes that row; the Local line shows the count and is absent at zero. | Pass (engineering); owner look v004 pending | `b0ea75ce8`; rerun at `a7db82152` |
| **AC10:** The Agent answer footer, finding card, finding queue and Explain this file card each offer one keep action; it makes a draft where a line can take one and a Local comment where none can, and its follow-up goes to what it made. | Pass (engineering); owner look v005 pending | `a7db82152` |
| **AC11:** A profile whose ledger holds notes written before and after the path and line side tables opens after migration with one Local comment for each: equal text bytes, equal anchor facts, equal order; a note-era Go to note arrives at its comment; a migration interrupted at any statement and run again gives the same result. | Pass | `72a0d55af` |
| **AC12:** An agent request carries a Local comment only when the session's Local opt-in is set and a draft only when its drafts opt-in is set, each independently of the other. | Pass | `a7db82152` |
| **AC13:** Label counters, thread counts, the Comments tab's new count and Home are unchanged by creating, moving or deleting Local comments. | Pass | `b0ea75ce8`; rerun at `a7db82152` |
| **AC14:** Each state is announced in words to assistive technology ("Local comment, only on this computer", "Draft, not in your review", "Draft, in your review"), and the state control, Make draft and Keep local are reachable and operable by keyboard alone. | Pass (engineering); owner look accepted (12534e15) | `d85b98576` |

## Outcome and work

Reconcile the live documents the RFC's owner amendments name (RFC-BPR-0007, 0011, 0012, 0023, 0049, 0085, 0086, 0094, 0101, 0102, 0107, 0108, 0110, 0111, 0113, 0115, 0116, 0117, 0118, 0121, 0125) with what was built; update `docs/architecture/product-contract.md` and the drawer and private-material sections of `docs/architecture/system-architecture.md`; record the three owner looks; run the documentation gate.

## Progress (2026-11-13)

Evidence by criterion: the [RFC-BPR-0127 ledger](../../verification/RFC-BPR-0127.md) and the cards [BPR-2157](../60-done/BPR-2157-comments-absorb-notes-model-store-migration.md), [BPR-2158](../60-done/BPR-2158-comments-drawer-holds-local-comments.md), [BPR-2159](../60-done/BPR-2159-local-comments-found-and-counted.md), [BPR-2160](../60-done/BPR-2160-one-keep-action-on-agent-surfaces.md), [BPR-2161](../60-done/BPR-2161-local-pr-draft-notes-become-local-comments.md).

Reconciled:
- Each amended RFC (0007, 0011, 0012, 0023, 0049, 0085, 0086, 0094, 0101, 0102, 0107, 0108, 0110, 0111, 0113, 0115, 0116, 0117, 0118, 0121, 0125) carries its RFC-BPR-0127 owner amendment from the import.
- `docs/architecture/product-contract.md` (private workflow state names Local comments) and `docs/architecture/system-architecture.md` (three states, the drawer's tabs, the ledger's Local comments, `decide_make_draft`, `decide_keep`, the two opt-ins, one Ask agent path) follow what was built. `docs/ux/ai-experience.md`, `docs/ux/workbench-specification.md`, `docs/ux/shared-action-buttons.md` and `docs/architecture/ai-agent-boundary.md` were updated with BPR-2160.
- The local PR draft's vocabulary (BPR-2161) is shown in a rendered local draft (`local-draft-local-comments.png`, from `application_local_draft_desktop_public_path`).
- The documentation gate is OK.

**Not closed.** Every criterion has direct engineering evidence. The RFC's closeout waits on three owner looks, filed and not waited on:
- v003 (session `acaf71a2…`, the v002 rework: AC06 and the drawer);
- v004 (session `8ce7e455…`, AC07 to AC09);
- v005 (session `4329741d…`, AC10 and Ask agent).

On their verdicts, record them here and on the ledger. If all are Accept, finish this card and mark RFC-BPR-0127 closed; a Reject is a new rework round.
