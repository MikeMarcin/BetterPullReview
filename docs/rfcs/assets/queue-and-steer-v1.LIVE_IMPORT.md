# Queue and steer v1: live import

Imported 2026-11-12 at the owner's request: "Add to queue /implement-rfc
Implement the RFC at
docs/rfcs/assets/queue-and-steer-v1/RFC-DRAFT-0001-queue-and-steer.md".
The mandate is queued, not started: every card is Backlog and no product
code was written at import. RFC_BASE_SHA:
`dbdecf9a32c9fc7cdd28122721dc61011236d60e` (lane
`claude/v0-5-0-prerelease-targets-52265d`, which holds RFC-BPR-0101's
fresh and resumed rule and the tour chat code this RFC extends). Mirrored to
`main`.

The draft was written as a single untracked file in the main checkout
(`docs/rfcs/assets/queue-and-steer-v1/RFC-DRAFT-0001-queue-and-steer.md`,
SHA-256 `87dcfed506cec06969f04c21ba0d4d87d5ca7f4c38b66a3826c75a42df6f26d3`).
Its bytes are kept unchanged on the lane at that same path (tracked there;
named here, not linked, because on `main` the file is still untracked);
the main checkout's untracked copy was left where it is, untouched. A later
merge of the lane into that checkout meets an identical untracked file at
the same path; remove it there first (its bytes are the tracked ones).

## Mapping

| Draft | Live |
|---|---|
| RFC-DRAFT-0001 | [RFC-BPR-0126](../RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md) |

The draft defined no cards and no plan. Cut at import, one milestone:

| Card | What | Criteria |
|---|---|---|
| [BPR-2151](../../issues/60-done/BPR-2151-queue-follow-ups-core-and-host.md) | Core queue state and host release after real completion | AC02, AC03, AC04 |
| [BPR-2152](../../issues/60-done/BPR-2152-queue-and-steer-durable-handoff.md) | Durable handoff and recovery | AC06 |
| [BPR-2153](../../issues/60-done/BPR-2153-native-steering-and-accounting.md) | Native steering (Codex App Server, Claude stream JSON) and accounting | AC05, AC07 |
| [BPR-2154](../../issues/30-in-progress/BPR-2154-queue-and-steer-composer.md) | Qt composer and pending list | AC01, AC08 |
| [BPR-2155](../../issues/10-backlog/BPR-2155-close-rfc-0126.md) | Closeout | all |

Card IDs were allocated on `main` with `scripts/allocate_issue_id.py` in the
main checkout (counter BPR-2151 to BPR-2156), receipt
`docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json`, committed
there at once (`9d899bb35`) and verified after: a dry run on `main` then
offered BPR-2156 with high water BPR-2156, so no other allocation overlaps.
The lane took main's counter and receipt. RFC numbers have no counter:
0126 was free on every local and remote branch (highest seen RFC-BPR-0125).

## Changes at import

- Links rewritten to live paths; front matter given the live ID, baseline,
  plan and dependencies (RFC-BPR-0092, 0096, 0101, 0102, 0103).
- Added "Reconciliation with the fresh and resumed rule": how the RFC fits
  RFC-BPR-0101's amendment of 2026-11-12 and the Done work of BPR-2138,
  BPR-2140 and BPR-2141, and the one conflict, the BPR-1930 whole-tour
  feedback buffer.
- Added "Open owner questions" Q1 to Q18: every default the owner did not
  state, each with the draft's default. The draft's closing sentence that no
  owner question remained is kept and marked superseded.
- No acceptance criterion was changed.

## Plan

[plans/queue-and-steer-v1](../plans/queue-and-steer-v1/MILESTONES.md), one
milestone `queue-and-steer`, written at import (the draft had none).
