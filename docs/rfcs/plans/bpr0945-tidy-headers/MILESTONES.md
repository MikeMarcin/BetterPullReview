# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **bpr0945-tidy-headers** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## strict-header-diagnostics

Resolve the two BPR-0945 header diagnostics without suppressions or behavior changes and verify their current Qt and application consumers.

**Work:** `BPR-0945`

**Prerequisites:** None

**Completion:**

- AC1: Reconcile both diagnostics with the coding standard without broad rule suppression or behavior changes.
- AC2: Run the canonical helper with warnings as errors on the affected Qt workbench and application translation units, with the proper host SDK.
- AC3: Re-run focused Home/Qt compilation and tests appropriate to any change.

**Boundary:** Standalone BPR-0945 only, adopted from the r4 owner-deferred follow-up under the coordinator unattended Mac assignment dated 2026-09-25, segment r1. Exclusive branch codex/bpr0945-tidy-headers based at 9a0b44d5. No primary-checkout writes or push; Windows/Linux runs explicitly deferred by owner.
