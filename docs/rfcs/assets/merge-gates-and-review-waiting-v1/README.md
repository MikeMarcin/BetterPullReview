# Merge gates and review waiting — fixes pack

Draft v1, authored 2026-10-11 from the owner's report and follow-up. The pack
proposes three connected fixes: inspect failing checks, see merge/review
requirements and the people or teams still awaited, and keep that work visible
on Home. The acceptance example is the reported **16 passed, 1 failed,
7 skipped**, with **Branch Freshness** failed and named code-owner teams.

| Milestone | Authoritative RFC | Proposed cards |
|---|---|---|
| Inspect check failures | [RFC-DRAFT-0007](RFC-DRAFT-0007-inspect-commit-checks.md) | [Observations](cards/DRAFT-0001-check-observations.md), [Overview](cards/DRAFT-0002-check-details-overview.md), [closeout](cards/DRAFT-0003-close-check-details.md) |
| See review and merge blockers | [RFC-DRAFT-0008](RFC-DRAFT-0008-reviewers-and-merge-requirements.md) | [Observations](cards/DRAFT-0004-review-requirement-observations.md), [Overview](cards/DRAFT-0005-reviewers-and-merge-overview.md), [closeout](cards/DRAFT-0006-close-review-requirements.md) |
| Keep Home waiting truthful | [RFC-DRAFT-0009](RFC-DRAFT-0009-preserve-home-waiting.md) | [Waiting facts](cards/DRAFT-0007-home-waiting-facts.md), [Home](cards/DRAFT-0008-home-waiting-presentation.md), [closeout](cards/DRAFT-0009-close-home-waiting.md) |

Start implementation intake at [IMPLEMENTATION_PROMPT.md](IMPLEMENTATION_PROMPT.md).
The [canonical milestone plan](milestone-plan.json) owns grouping and order;
[MILESTONES.md](MILESTONES.md) is its generated view. The portable
[plan schema](schemas/milestone-plan.schema.json) and
[card schema](schemas/issue-card.schema.json) are copied from the discovered
implement-rfc package.

[DECISIONS.md](DECISIONS.md#checks-and-confirmation) records structural
validity, design readiness, actual confirmation and unevaluated execution
eligibility separately. It also preserves the owner's follow-up and the API
finding that answered it. [GLOSSARY.md](GLOSSARY.md) keeps review requests,
review dispositions and merge requirements distinct.

[PROPOSED_AMENDMENTS.md](PROPOSED_AMENDMENTS.md) identifies changes to the
existing provider, Overview and Home contracts. Live RFCs, issue lanes,
counters, adopted plans and historical acceptance remain untouched during
authoring. The separate draft-PR chip proposal is not part of this pack.

The UX direction uses the loaded frontend-design skill and existing native
Overview/Home conventions. This is a read-only visibility and state-correction
pack; it does not make BPR merge PRs, request reviews, send reminders, alter
rules or run checks.
