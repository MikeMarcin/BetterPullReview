# Address review feedback v1: live import

Imported 2026-11-11 at the owner's request, relayed by the authoring
session: "/author-rfc a pack for this and message the other session to add
it to it's work". This is an import, not a mandate to implement: every card
is Backlog and no milestone is started. RFC_BASE_SHA:
`91cce126bf211865298f58c79ba86e09bc2b077a`. Imported on lane
`claude/v0-5-0-prerelease-targets-52265d`, which holds the RFCs this pack
extends (RFC-BPR-0098, 0108 to 0115), and mirrored to `main`. The authored
branch `claude/address-review-feedback-v1` (`b1e5e6fdb`, the pack folder
only) was merged into the lane first (`881778d5b`). The original pack bytes
are kept under [address-review-feedback-v1](address-review-feedback-v1/README.md)
with their provisional IDs.

## Mapping

| Draft | Live |
|---|---|
| RFC-DRAFT-0001 | [RFC-BPR-0122](../RFC-BPR-0122-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md) |
| RFC-DRAFT-0002 | [RFC-BPR-0123](../RFC-BPR-0123-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md) |
| RFC-DRAFT-0003 | [RFC-BPR-0124](../RFC-BPR-0124-one-press-publishes-the-response-push-then-replies.md) |
| DRAFT-0001 | [BPR-2105](../../issues/10-backlog/BPR-2105-address-feedback-and-the-pull-request-association.md) |
| DRAFT-0002 | [BPR-2106](../../issues/10-backlog/BPR-2106-feedback-items-placement-and-dispositions.md) |
| DRAFT-0003 | [BPR-2107](../../issues/10-backlog/BPR-2107-close-rfc-0122.md) (closeout) |
| DRAFT-0004 | [BPR-2108](../../issues/10-backlog/BPR-2108-evaluate-feedback.md) |
| DRAFT-0005 | [BPR-2109](../../issues/10-backlog/BPR-2109-fix-accepted-items-in-one-run.md) |
| DRAFT-0006 | [BPR-2110](../../issues/10-backlog/BPR-2110-draft-replies-and-the-commit-reference-check.md) |
| DRAFT-0007 | [BPR-2111](../../issues/10-backlog/BPR-2111-close-rfc-0123.md) (closeout) |
| DRAFT-0008 | [BPR-2112](../../issues/10-backlog/BPR-2112-publish-response-in-order.md) |
| DRAFT-0009 | [BPR-2113](../../issues/10-backlog/BPR-2113-home-reads-waiting-after-a-response.md) |
| DRAFT-0010 | [BPR-2114](../../issues/10-backlog/BPR-2114-close-rfc-0124.md) (closeout) |

The machine map is [address-review-feedback-v1.id-map.json](address-review-feedback-v1.id-map.json)
and [the plan's source record](../plans/address-review-feedback-v1/source.json).
Card IDs were allocated on `main` by `scripts/allocate_issue_id.py` in the
main checkout (counter BPR-2105 to BPR-2115), receipt
`docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json`, committed
there at once (`42fba4488`, together with BPR-2115 for the lane's session
naming card) and verified on `main` right after: only the two receipts and
the counter, no card or reservation in BPR-2105..2115 before. The lane took
main's counter and receipts (`91cce126b`). RFC numbers have no counter:
0122 to 0124 were free on every local and remote branch at import (highest
seen RFC-BPR-0121).

Closeout cards take the board's required name and title
(`BPR-NNNN-close-rfc-NNNN`, "Verify and close RFC-BPR-NNNN"); the draft
title is kept in card metadata as `draft_title`. Each closeout carries a
pending checklist with every canonical criterion. RFC acceptance sections are
renamed "Acceptance criteria" for the documentation gate; their text is
unchanged. Links and IDs in the live RFCs and cards are rewritten to live
paths; the retained pack keeps the draft ones.

## Lanes and plan

All ten cards are Backlog. The authored three-milestone grouping
(`feedback-items`, then `agent-feedback-work` and `publish-response`, each
independent of the other) is adopted without scope or exit changes under
[plans/address-review-feedback-v1](../plans/address-review-feedback-v1/MILESTONES.md)
(`milestone_plan.py adopt`). Every milestone waits on adopted work that is
not yet built: BPR-2021 and BPR-2030 (local draft shell and association),
BPR-2026 (local-draft sessions), BPR-2068 (change runs) and BPR-2066
(push).

## Amendments to adopted RFCs

The pack's [proposed amendments](address-review-feedback-v1/AMENDMENTS.md)
change one rule of RFC-BPR-0098 (A1, next move Waiting for a
changes-requested pull request once the reviewers have been asked again)
and extend RFC-BPR-0107, 0108, 0114 and 0115 without changing what they
require. None is applied at import, and no live RFC text outside the three
new RFCs changed. Each owning milestone reconciles its own; A1 belongs to
BPR-2113 and the `publish-response` exit "RFC-BPR-0098's live text is
reconciled with that change".

## Decisions carried

D01 and D02 of the pack's [decision record](address-review-feedback-v1/DECISIONS.md)
are the owner's. **P01 to P09 are the pack author's proposals and are
unconfirmed.** P03, P07 and P08 were shown to the owner as defaults before
the pack was requested, without comment. **P09 changes an adopted rule of
RFC-BPR-0098 and must be asked of the owner before BPR-2113 is built**; the
card says so. The pack was not reviewed or grilled before import.

## Drift

The pack inspected lane `b68e572fea0dac33c2164fdf2eea2be8e1fa8eb0`. Between
it and the baseline the lane changed RFC-BPR-0104's agent launch (the pull
request's own agent settings never load, `059e191fb`), its ledger and
amendment, RFC-BPR-0100's ledger, BPR-1995/1996 records, the counter and
reservations. None of it touches what the pack's RFCs read (Home's next
move, local drafts, publication, prompts).

## Exclusions

No credentials, logs, screenshots or build output were imported. Adoption is
not implementation acceptance.
