# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **qt-6-12-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## qt612-upgrade

The existing Qt module and package closure uses official Qt6.12.0 with verified source/ICU pins and available-host runtime evidence.

**Work:** `BPR-1665`, `RFC-BPR-0088`

**Prerequisites:** None

**Completion:**

- Default provider and distribution pins consistently select Qt6.12.0, preserving refusal gates and module closure.
- Downloaded source and Linux ICU identities match the inventory; focused package/source/security contracts pass.
- Available-host app build and composed offscreen checks use Qt6.12.0; document unavailable native/package evidence and retain existing release obligations.
- Documentation gate passes; explicit-path commits and a durable checkpoint bind all results to the upgrade mandate.

**Boundary:** Dependency upgrade, associated tests and documentation only; no release, push, owner desktop or new UI design.
