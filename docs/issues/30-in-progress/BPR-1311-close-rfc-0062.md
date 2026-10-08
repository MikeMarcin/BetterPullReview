---
schema: "implement-rfc.card.v1"
id: "BPR-1311"
title: "Verify and close RFC-BPR-0062"
status: "In Progress"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1310"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
closeout_for: ["RFC-BPR-0062"]
---

# Verify and close RFC-BPR-0062

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

RFC base: `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** In the composed application, a newly signed-in profile adds its first repository from Home without visiting Settings or opening a PR; a returning reviewer can find the same action with populated or filtered rows. | Public-path/setup/Qt and source-specific evidence in [ledger](../../verification/RFC-BPR-0062-0065.md) | Pass on exact implementation candidate | `8e2f855e9061b4ce9cc150c1668356c8301b9b01` |
| **AC02:** The selected repository-entry method reaches an exact displayed account/host destination; invalid input, cancelled setup and stale sign-in completions cannot create a record under another authority. | Public-path/setup/Qt and source-specific evidence in [ledger](../../verification/RFC-BPR-0062-0065.md) | Pass on exact implementation candidate | `8e2f855e9061b4ce9cc150c1668356c8301b9b01` |
| **AC03:** Saving, duplicate entry and storage refusal produce the defined result states through the real persistence path, with successful records surviving restart and appearing in other windows of the same profile. | Public-path/setup/Qt and source-specific evidence in [ledger](../../verification/RFC-BPR-0062-0065.md) | Pass on exact implementation candidate | `8e2f855e9061b4ce9cc150c1668356c8301b9b01` |
| **AC04:** Discovery after a save shows personal results or an accurate empty/offline/limited/unavailable state without undoing the save, opening a review, expanding discovery beyond saved repositories or mutating GitHub. | Public-path/setup/Qt and source-specific evidence in [ledger](../../verification/RFC-BPR-0062-0065.md) | Pass on exact implementation candidate | `8e2f855e9061b4ce9cc150c1668356c8301b9b01` |
| **AC05:** Home and Settings invoke the same addition flow, and the existing open-PR shortcut retains its direct save behavior without a competing store or contradictory account prerequisite. | Public-path/setup/Qt and source-specific evidence in [ledger](../../verification/RFC-BPR-0062-0065.md) | Pass on exact implementation candidate | `8e2f855e9061b4ce9cc150c1668356c8301b9b01` |
| **AC06:** The real Home and dialog support keyboard completion and cancel, announced progress/refusal, long identities and narrow layouts at 100–200% scale in the supported appearances; the owner judges each changed visual surface through a fresh short attended session with a frozen build. | Public-path/setup/Qt and source-specific evidence in [ledger](../../verification/RFC-BPR-0062-0065.md) | Automated pass; owner Home/dialog verdicts pending | `8e2f855e9061b4ce9cc150c1668356c8301b9b01` |

## Current closeout boundary

BPR-1308–1309 automation passes on the implementation candidate above.
BPR-1310 still requires the owner's separate Home and dialog visual judgments.
This card remains dependency-ineligible until BPR-1310 is Done; RFC0062 stays
Proposed. Original AC01–AC06 are retained. No owner/native judgment is inferred
from offscreen evidence. Live operator/UX documentation now describes the shared
flow; RFC0028/RFC0032 supersession notes retain their historical acceptance.
