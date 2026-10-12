# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **merge-gates-and-review-waiting-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## inspect-checks

A reviewer can identify and inspect a failed check from Overview, with truthful result identity, detail links and availability.

**Work:** `RFC-DRAFT-0007` [AC01, AC02, AC03, AC04, AC05]

**Prerequisites:** None

**Completion:**

- The 16 passed, 1 failed, 7 skipped scenario exposes Branch Freshness and its supplied explanation inside the composed Overview.
- Current check runs and commit statuses, repeated attempts and head/test-merge identity remain distinct; incomplete results never imply merge readiness.
- Native detail controls and admitted link handoff pass the RFC acceptance, with relevant documentation reconciled.

**Boundary:** An integrated check-inspection feature. Requirement classification may remain explicitly unknown until the next milestone; no reruns or repository execution.

## see-review-blockers

Overview explains why merging is blocked and visibly names the users and code-owner teams whose reviews are still requested.

**Work:** `RFC-DRAFT-0008` [AC01, AC02, AC03, AC04, AC05, AC06]

**Prerequisites:** inspect-checks

**Completion:**

- Three provider-marked code-owner teams and ordinary pending users remain visible even when they have never commented or reviewed.
- The composed blocked example shows applicable review requirements and check failures together without guessing remaining approvals, individual team obligations or auto-merge state.
- Available details and true host/access limits are distinguished across the qualified provider paths, with native usability and the RFC acceptance verified.

**Boundary:** A read-only merge-requirements and reviewer explanation, using server facts. No collaboration writes, merge operations or local CODEOWNERS evaluator.

## keep-home-waiting

Authored PRs awaiting review remain discoverable in the correct Home section, with the same waiting identities and blockers as Overview.

**Work:** `RFC-DRAFT-0009` [AC01, AC02, AC03, AC04, AC05]

**Prerequisites:** see-review-blockers

**Completion:**

- Discovery, opening and returning Home preserve pending-review facts, including authored PRs enrolled only through Follow or an observer rule.
- The mixed code-owner/check-failure example stays in Waiting on others, with the existing unread-activity precedence and truthful Quiet-items behavior.
- Bounded background reads, freshness, restart, identity changes and ordinary Home controls satisfy the RFC; one final integrated public-path pass reconciles the complete pack.

**Boundary:** Correct waiting facts, persistence, classification and native explanation under the existing section order. No enrollment expansion, automatic reminders or new notification category.
