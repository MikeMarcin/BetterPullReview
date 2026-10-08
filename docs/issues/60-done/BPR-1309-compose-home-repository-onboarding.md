---
schema: "implement-rfc.card.v1"
id: "BPR-1309"
title: "Compose visible Home onboarding and discovery after save"
status: "Done"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1308"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"8e2f855e9061b4ce9cc150c1668356c8301b9b01","evidence":{"kind":"command","detail":"Qt Home discovery courses at 100%, 125% and 200% pass no-saved/populated/filtered/running/unavailable cases and signed-out/public-preview behavior. Shared Home status/action row is independent of filtered rows and retains Add in every accepted account state."}},{"criterion":"C2","outcome":"pass","revision":"8e2f855e9061b4ce9cc150c1668356c8301b9b01","evidence":{"kind":"command","detail":"Composed real persistence/discovery course passes 9.55s: first Home Add without PR/Settings; invalid/foreign-host/cancelled setup; duplicate; real SQLite write-lock refusal; explicit retry; empty/offline/secondary-limited discovery; sibling windows; profile restart after retiring desktop lease drains; open-PR direct save uses typed path; zero provider mutations. Host simultaneous equal-number sibling gestures pass 24.61s."}}]
---

# Compose visible Home onboarding and discovery after save

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Add repository is visible with no saved repositories, populated/filtered rows and unavailable/running discovery; signed-out Home remains focused on sign-in/public reading.
- C2: A composed persistence/discovery course covers AC01–AC05, same-profile windows/restart, duplicate/refusal, offline/limited/empty discovery and direct open-PR shortcut; zero provider mutations.
