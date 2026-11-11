# Implement this pack

Import this pack, then implement only subsequently authorized scope from the
three RFCs listed in the [overview](README.md), using the
[authored milestone plan](milestone-plan.json). Its
[milestone view](MILESTONES.md) is generated; do not edit it independently.
The pack is a draft with provisional IDs: allocate live RFC and card IDs,
write the live RFCs and Backlog cards, adopt the plan and record the import
through implement-rfc before product work.

Read the consumer instructions, the [decision record](DECISIONS.md), the
owner's [source notes](SOURCE_NOTES.md) and the
[proposed amendments](AMENDMENTS.md). Reconcile live source drift against
the inspected revision. The proposed cards are in [cards/](cards/).

Two rows of the decision record are the owner's (D01, D02). The rest are
proposals (P01 to P09). P03, P07 and P08 are the three defaults the owner
was shown before asking for this pack; they are recorded as shown, not as
separately confirmed. Put P09, the change to Home's next move, to the owner
before building it.

Every milestone waits on local-draft work that is not built yet:
`feedback-items` on BPR-2021 and BPR-2030, `agent-feedback-work` on BPR-2026
and BPR-2068, `publish-response` on BPR-2066. `agent-feedback-work` and
`publish-response` are independent of each other.

Establish, do not assume: how the existing reply publication sends an
author's replies (singly or as one review) and what a reviewer then sees;
which facts GitHub exposes about a review request made after a review, so
that Home's Waiting can be decided from them; and how each installed agent
returns a verdict per item for many items in one run.

Load frontend-design before any Qt surface work and say so in the report.
Use offscreen verification and frozen builds; never the owner's live
desktop. Live GitHub goes only to `MikeMarcin/BPR-Test`, with the test
account as the reviewer.
