# Implement this pack

Implement only subsequently authorized scope from the six RFCs listed in the
[overview](README.md), using the
[authored milestone plan](milestone-plan.json). Its
[milestone view](MILESTONES.md) is generated; do not edit it independently.
The pack is a draft, not a current implementation mandate: the owner asked
for it to be authored and imported.

Read the consumer instructions, the [decision record](DECISIONS.md), the
owner's [source notes](SOURCE_NOTES.md) and the
[proposed amendments](AMENDMENTS.md). Reconcile live source drift against
the inspected revision, and adopt the authored grouping through
implement-rfc before product work. The proposed cards are in
[cards/](cards/).

One row of the decision record is the owner's answer (D01): these six ideas
are wanted. Every mechanic is a proposal (P01 to P08). Put P02, one built-in
prompt per operation, to the owner before the first milestone, because all
six RFCs carry it and the owner has since merged the three Enhance prompts
into one.

The six milestones are independent. `returning-pull-request`,
`explain-file`, `comment-to-suggested-change`, `failing-check-question` and
`agent-insights` build on shipped code. `agent-run-checks` waits on
RFC-BPR-0109's evidence record and RFC-BPR-0115's change runs.

Three entry points change their subject rule in core
(`evaluate_proposed_fix`, `analyze_current_file`) or gain a subject kind (a
check). Core's switches have no default label on purpose: let the build find
every place that must decide.

Establish, do not assume: that each installed agent can return the
structured replacement RFC-DRAFT-0003 needs, and what it does when it
cannot; and whether each installed agent reports a cost that can be summed.

Load frontend-design before any Qt surface work and say so in the report.
Use offscreen verification and frozen builds; never the owner's live
desktop. Live GitHub goes only to `MikeMarcin/BPR-Test`.
