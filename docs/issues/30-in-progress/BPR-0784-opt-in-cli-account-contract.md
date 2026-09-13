---
schema: "implement-rfc.card.v1"
id: "BPR-0784"
title: "Expose an honest opt-in GitHub CLI account profile"
status: "In Progress"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# Expose an honest opt-in GitHub CLI account profile

RFC baseline: `6c9ef203` (after issue-ID reservation, before RFC-0015 source changes).

- [ ] A CLI account row requires explicit opt-in and shows exact hostname and observed `/user` login without replacing a native profile.
- [ ] The row names configured-CLI executable/config/CA/redirect/account-switch/process limitations as trust risks, not contained guarantees.
- [ ] Read connection does not grant publishing; a changed or unavailable identity fails visibly.
- [ ] The rendered app and account UI tests exercise the selection and warning; no token or private response is displayed or persisted.

The original deferred RFC contract remains preserved as historical evidence. No
live PR write is authorized. This card joins BPR-0785 in the first milestone.
