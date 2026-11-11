# Implement this pack

Implement only subsequently authorized scope from the three RFCs listed in
the [overview](README.md), using the
[authored milestone plan](milestone-plan.json). Its
[milestone view](MILESTONES.md) is generated; do not edit it independently.
The pack is a draft, not a current implementation mandate: the owner asked
for it to be authored and imported.

Read the consumer instructions, the [decision record](DECISIONS.md), the
[glossary](GLOSSARY.md), the owner's [source notes](SOURCE_NOTES.md) and the
[proposed amendments](AMENDMENTS.md). Reconcile live source drift against
the inspected revision, and adopt the authored grouping through
implement-rfc before product work. The proposed cards are in
[cards/](cards/).

Four of the decision record's rows are the owner's answers (D01, D02, D03,
D06). Eleven are proposals (P01 to P11). A proposal is not an answer: before
building `agent-fixes`, put P03, P04, P07, P09, P10 and P11 to the owner.

Every milestone waits on adopted local-draft work that is not yet built:
`edit-and-save` on RFC-BPR-0108's durable candidates, `commit-and-push` on
RFC-BPR-0111's pull request creation, `agent-fixes` on RFC-BPR-0104's grant
line, RFC-BPR-0110's local-draft sessions and RFC-BPR-0107's sharing format. `edit-and-save` and
`commit-and-push` are independent of each other; `agent-fixes` follows
`edit-and-save`; `agent-history-operations` follows both of the others.

Each owning milestone applies its amendment to the live RFC text it names,
and the changed exit of the adopted plan's `publish-prepared-draft`
milestone is an explicit plan amendment, not a replayed adoption.

Establish, do not assume: how each installed agent's own permission system
confines writes to one directory, and whether its runtime enforces that or
only follows it; how Git behaves without a terminal for each credential and
signing setup the owner uses; and that a push to a URL moves no branch and
writes no configuration in the clone.

Load frontend-design before any Qt surface work and say so in the report.
Use offscreen verification and frozen builds; never the owner's live
desktop. Live GitHub goes only to `MikeMarcin/BPR-Test`.
