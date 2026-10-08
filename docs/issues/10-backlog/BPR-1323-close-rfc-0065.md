---
schema: "implement-rfc.card.v1"
id: "BPR-1323"
title: "Verify and close RFC-BPR-0065"
status: "Backlog"
rfcs: ["RFC-BPR-0065"]
depends_on: ["BPR-1322"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
closeout_for: ["RFC-BPR-0065"]
---

# Verify and close RFC-BPR-0065

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

RFC base: `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** The selected tool/version-probe facts and matching acquisition failure are established with a composed reproducer, separating observed failure from source/build inference and the later actor-login failures. | Focused public-path and required native/attended evidence | Synthetic separate reproducers pass; affected configured paths/source-build facts pending | 980c08873 |
| **AC02:** Under the chosen support policy, a stable GHES fixture that reproduces the incident renders exact base/head contents for multiple files through the public application; unsupported/probe-failure cases retain a precise actionable refusal without acquiring source under invalid authority. | Focused public-path and required native/attended evidence | Multi-file composed and typed actionable refusal checks pass; actual affected outcome pending | 980c08873 |
| **AC03:** Retry/reopen over previously failed persisted state recovers after qualification is repaired, while head/account/host/selection replacement and cancellation cannot publish stale or mismatched file contents. | Focused public-path and required native/attended evidence | Fixture retry/reopen/persisted failed-state and stale/cancel checks pass; native affected run pending | 980c08873 |
| **AC04:** The acquisition and focused public-path checks retain the defined confinement, bounds, credential isolation and exact-object guarantees; a support change has its own direct installed-tool qualification evidence. | Focused public-path and required native/attended evidence | Local Git-155 native acquisition/confinement proof passes; owner Git-157 qualification pending | 980c08873 |
| **AC05:** On an exact frozen repaired candidate, the owner can open the originally affected enterprise PR read-only without the supported-snapshot rejection and view its intended file contents/diffs. Ask that single rendered-outcome question in a fresh short attended session; record build/tool/transport qualification and keep the private target and log outside source commits. | Focused public-path and required native/attended evidence | Required exact frozen affected enterprise PR owner outcome pending | 980c08873 |
| **AC06:** A synthetic GHES snapshot reproduces the actor-login rejection with attributable observation provenance. The evidence-backed repair admits legitimate input through the composed public PR read path; truly conflicting actor/authority/target observations still reject. Retry/restart and stale completion controls pass without weakening the other relationship checks. | Focused public-path and required native/attended evidence | Attributable synthetic rejection and safe diagnostics pass; legitimate actor representation repair blocked | 980c08873 |

## Current closeout boundary

BPR-1320 retains affected-installation C1 pending and automated C2 pass;
BPR-1321 is blocked by missing legitimate actor representation evidence.
BPR-1322 cannot claim the actual affected rendered outcome and remains in
Backlog. RFC0065 stays Proposed, with the original AC01–AC06 and immutable
baseline intact. Local installed-tool proof is distinct from owner-selected
Git-157/private GHES/native acceptance. See the current [ledger](../../verification/RFC-BPR-0062-0065.md).
