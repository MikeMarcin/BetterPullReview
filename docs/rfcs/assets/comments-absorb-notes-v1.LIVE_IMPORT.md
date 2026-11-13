# Comments absorb notes v1: live import

Imported 2026-11-13 on lane `claude/v0-5-0-prerelease-targets-52265d`
(mandate `v0-5-0-prerelease-targets-r1`), mirrored to `main`. The owner
accepted the design and all fourteen recommended defaults on 2026-11-13,
except the lock for Local comments, which is a computer icon with the tooltip
"Only on this computer". No product code was written at import; every card is
Backlog. Baseline `f85b216ec` (the lane, after the counter and receipts).

## Sources

Written in a design session's scratch space and brought in unchanged:

| File | SHA-256 | Kept at |
|---|---|---|
| `RFC-DRAFT-comments-absorb-notes.md` (RFC-DRAFT-0001) | `785f99c5fd92cc76ef55d2b0d43c6cc4837ef25829da86c94e9d6ea7a3de1f21` | [comments-absorb-notes-v1/](comments-absorb-notes-v1/RFC-DRAFT-comments-absorb-notes.md) |
| `inventory.md` | `88f1e3beba45bc20573dc99b0364cbbea81d7c5c7ed617bd319608a1dffb1151` | [comments-absorb-notes-v1/](comments-absorb-notes-v1/inventory.md) |
| `index.html` and its 18 renders in `png/`, with the four `ref/` renders it embeds | `index.html`: `140fb22cb351d4b5bdfda2624d1614c892ab01f6c596e815a5b752ec57c43431` | [docs/design/comments-absorb-notes-v1](../../design/comments-absorb-notes-v1/README.md) |

Excluded: the session's generator scripts (`tools/`) and its other `ref/`
renders, made only for inspection.

## Mapping

| Draft | Live |
|---|---|
| RFC-DRAFT-0001 | [RFC-BPR-0127](../RFC-BPR-0127-comments-absorb-notes.md) |

Cards were cut at import from the draft's suggested milestones:

| Card | What | Criteria |
|---|---|---|
| [BPR-2157](../../issues/60-done/BPR-2157-comments-absorb-notes-model-store-migration.md) | Model, store and migration | AC03, AC11, AC13 |
| [BPR-2158](../../issues/60-done/BPR-2158-comments-drawer-holds-local-comments.md) | Comments absorbs Notes | AC01, AC02, AC04, AC05, AC06, AC14 |
| [BPR-2159](../../issues/60-done/BPR-2159-local-comments-found-and-counted.md) | Threads, Overview, My review, Finish your review | AC07, AC08, AC09 |
| [BPR-2160](../../issues/30-in-progress/BPR-2160-one-keep-action-on-agent-surfaces.md) | One keep action, wording | AC10, AC12 |
| [BPR-2161](../../issues/30-in-progress/BPR-2161-local-pr-draft-notes-become-local-comments.md) | Local PR drafts' vocabulary (Q8) | none |
| [BPR-2162](../../issues/10-backlog/BPR-2162-close-rfc-0127.md) | Closeout | all |

IDs were allocated on `main` with `scripts/allocate_issue_id.py` in the main
checkout and committed there at once (`eaf625a0f`, receipt
`docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json`); a dry
run after offered BPR-2170. The lane took main's counter and receipts
(`f85b216ec`). RFC numbers have no counter: 0127 and 0128 were free on every
local and remote branch.

## Changes at import

- Front matter: live ID, baseline, plan, accepted design; links rewritten to
  live paths; the "Acceptance" heading named as the documentation gate reads.
- The open questions became the owner's answers; Q1's lock became the
  computer icon in the decision table, the drawn-state rule, Threads' rows,
  Overview's chip and the RFC-BPR-0085 amendment.
- "Proposed amendments" became amendments the owner accepted, applied to the
  named RFCs as owner amendments (2026-11-13) without rewriting a closed RFC's
  acceptance criteria.
- No acceptance criterion was changed.

## Plan

[plans/comments-absorb-notes-v1](../plans/comments-absorb-notes-v1/MILESTONES.md):
five milestones, the draft's suggested ones.
