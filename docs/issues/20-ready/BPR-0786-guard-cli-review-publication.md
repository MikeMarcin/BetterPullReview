---
schema: "implement-rfc.card.v1"
id: "BPR-0786"
title: "Publish CLI-backed reviews and comments with explicit write admission"
status: "Ready"
rfcs: ["RFC-BPR-0015"]
depends_on: ["BPR-0784","BPR-0785"]
priority: "P1"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# Publish CLI-backed reviews and comments with explicit write admission

- [ ] Read opt-in alone cannot publish; each CLI-backed write gets a fresh confirmation naming host, observed login, repository, PR and operation, with the unresolved account-switch risk.
- [ ] Fixture review and inline-comment actions traverse ordinary draft/Submit review admission, exact target, CLI mutation transport, reconciliation and typed terminal projection.
- [ ] Mismatched account/target refuses before POST; ambiguous attempted writes cannot be retried as a second POST.
- [ ] Token and private response content stay out of arguments, environment, logs, diagnostics, settings, SQLite and evidence.
- [ ] No real PR write occurs without a separately designated safe target.
