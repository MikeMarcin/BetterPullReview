---
schema: "implement-rfc.card.v1"
id: "BPR-1388"
title: "Verify and close RFC-BPR-0066"
status: "Ready"
rfcs: ["RFC-BPR-0066"]
depends_on: ["BPR-0776","BPR-0783","BPR-0990"]
closeout_for: ["RFC-BPR-0066"]
priority: "P1"
metadata: {"mandate_revision":"home-authored-r1","rfc_base_sha":"b87b7e4ae8f62696c3af88abe3c943eaef65c6f5","reservation":"docs/issues/_reservations/BPR-1388--BPR-1388--5e9c0a9a9497.json","milestone_plan":{"id":"home-authored-v1","path":"docs/rfcs/plans/home-authored-v1/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0066

## Outcome and scope

Implement RFC-BPR-0066 AC01–AC05, preserving the owner's settled waiting placement. The core supplies a row-scoped Home section alongside unchanged attention; the host carries accepted rows and persists independent per-window collapse; Qt presents existing panels and row state. Adopt the named live prose amendments without altering historical Done cards, plan exits or unrelated human/native obligations.

## Acceptance

- **AC01:** Author-role and authored-discovery rows with unread activity are absent from Needs me; another account's membership is independent.
- **AC02:** Actionable, waiting, quiet and unknown authored rows occupy one correct section and retain activity; notifications and repository attention remain unchanged.
- **AC03:** Rendered grouping, headers, filtered counts, collapse and keyboard navigation remain accurate, including authored-only and empty queues.
- **AC04:** Upgrade and two-window restart preserve view choices; My pull requests starts expanded and persists separately from Waiting on others; named group collapse stays session-only.
- **AC05:** Composed accepted refresh preserves exact selection and normal opening destination, adding no provider work; existing recent/onboarding/signed-out courses pass.

## Checkpoint

Initial import. Candidate and verification pending; exact RFC baseline above. No task-owned background commands or review server.

RFC base: `b87b7e4ae8f62696c3af88abe3c943eaef65c6f5`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** With an authored PR and another person's PR both carrying unread activity, only the latter appears or counts in Needs me. Author-role and authored-discovery inputs produce the same separation, scoped to each row's account identity. | Pending implementation and focused checks | Pending | — |
| **AC02:** Changing an authored row between actionable, waiting, quiet and unknown activity produces the placement defined under Section membership, with one active row and no loss or fabrication of its activity explanation. In particular, unread activity moves it from Waiting on others to My pull requests while preserving its unread count. Its Home placement does not change notification eligibility or repository-drawer attention. | Pending implementation and focused checks | Pending | — |
| **AC03:** The rendered Home preserves the separation, accurate filtered counts and readable activity through every Group value, both State headers settings, collapse and keyboard navigation. An authored-only queue remains visible; empty sections add no empty panels. | Pending implementation and focused checks | Pending | — |
| **AC04:** After upgrading and restarting two windows with different saved Home views, My pull requests initially appears expanded and subsequently retains each window's own collapse choice. Unaffected view choices and the documented lifetime of named group collapse remain intact. | Pending implementation and focused checks | Pending | — |
| **AC05:** Through the composed application's accepted refresh and ordinary row actions, the same exact PR remains selected and opens in the intended window after its activity changes; section arrangement adds no provider work. Existing Recently viewed, onboarding and signed-out courses remain valid for their unchanged behavior. | Pending implementation and focused checks | Pending | — |
