---
schema: "implement-rfc.card.v1"
id: "BPR-2099"
title: "Retain removed file lifetimes in the profile reset fixture"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac", "win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2099--BPR-2099--a08e523b5198.json", "source_baseline": "48bd9b3f2b82128e6b43527812f165eacc4f8b47", "milestone_plan": {"id": "bpr-2099-reset-file-lifetime", "path": "docs/rfcs/plans/bpr-2099-reset-file-lifetime/milestone-plan.json"}, "mandate_revision": "release56-profile-file-lifetime-r1", "allocation_main": "65f6be96f", "native_baseline": "ProfileR2 receiptfc9c1a291768a420b2ff4bbc3db3b8312524e54e6a5e2bd1b810e87755d35ed6"}
---

# Retain the old file through reset

The actual original Linux course repeats its settings inequality failure with identical device/inode but different native object generation. Reset successfully unlinked and recreated the file; its old identity number had been recycled before the test inspected it.

Pin only files that the selected reset must remove, using no-follow read-only close-on-exec descriptors. Assert the held original has zero links, then keep the original replacement identity check. Never pin kept-store descriptors across a reopened SQLite runtime, and keep the Windows test unchanged.

## Acceptance

- AC1: Preserve original/diagnostic/operator failures and causal values; implement the bounded POSIX fixture correction without product changes.
- AC2: A private unchanged-file control fails the new unlink assertion; corrected original Release and Debug courses pass, preserving all existing reset, kept-store, logs, sign-in, unacknowledged and damaged-history recovery assertions and budgets.
- AC3: Focused static/docs/tracker gates and intentional main contribution with exact source/evidence and terminal command/lease handoff.
