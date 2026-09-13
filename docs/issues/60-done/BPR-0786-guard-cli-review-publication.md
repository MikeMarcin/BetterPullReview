---
schema: "implement-rfc.card.v1"
id: "BPR-0786"
title: "Publish CLI-backed reviews and comments with explicit write admission"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: ["BPR-0784","BPR-0785"]
priority: "P1"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
acceptance: [{"criterion":"Read opt-in alone cannot publish; each CLI-backed write gets a fresh confirmation naming host, observed login, repository, PR and operation, with the unresolved account-switch risk.","outcome":"pass","revision":"df458d30","evidence":{"kind":"observation","detail":"Tests-disabled macOS fixture UI showed read opt-in followed by a separate exact-target/account/operation warning for each review and inline write; Cancel retained the review draft. build/rfc0015-r2-d4-publication-evidence.json"}},{"criterion":"Fixture review and inline-comment actions traverse ordinary draft/Submit review admission, exact target, CLI mutation transport, reconciliation and typed terminal projection.","outcome":"pass","revision":"df458d30","evidence":{"kind":"observation","detail":"Observed held gh POST to /reviews and /comments for fixture PR17, one transmitted recovery row and receipt each; whole-review New review reenabled and final inline UI displayed Published PRRC_fixture9002; refreshed 1 comment(s). build/rfc0015-r2-d4-publication-evidence.json"}},{"criterion":"Mismatched account/target refuses before POST; ambiguous attempted writes cannot be retried as a second POST.","outcome":"pass","revision":"df458d30","evidence":{"kind":"command","detail":"Six focused CTests passed, including actual gh_cli_transport mutation session mismatch, pre-POST witness, timeout and recreated-session GET reconciliation without second POST; see build/rfc0015-r2-d4-final-focused-ctest-2.log"}},{"criterion":"Token and private response content stay out of arguments, environment, logs, diagnostics, settings, SQLite and evidence.","outcome":"pass","revision":"df458d30","evidence":{"kind":"observation","detail":"Scoped source/test and synthetic-log audit: POST body traveled by stdin, not arguments; fixture body and receipt were absent from application and diagnostic logs; fixed redacted errors, no token extraction or global gh auth/configuration mutation in changed path. Target-native credential-helper exchange remains BPR-0787."}},{"criterion":"No real PR write occurs without a separately designated safe target.","outcome":"pass","revision":"df458d30","evidence":{"kind":"observation","detail":"All UI publication runs used the task-owned fake-gh fixture with isolated home and fixture PR17; no safe live target was named and no live PR write was attempted. build/rfc0015-r2-d4-publication-evidence.json"}}]
---

# Publish CLI-backed reviews and comments with explicit write admission

- [x] Read opt-in alone cannot publish; each CLI-backed write gets a fresh confirmation naming host, observed login, repository, PR and operation, with the unresolved account-switch risk.
- [x] Fixture review and inline-comment actions traverse ordinary draft/Submit review admission, exact target, CLI mutation transport, reconciliation and typed terminal projection.
- [x] Mismatched account/target refuses before POST; ambiguous attempted writes cannot be retried as a second POST.
- [x] Token and private response content stay out of arguments, environment, logs, diagnostics, settings, SQLite and evidence.
- [x] No real PR write occurs without a separately designated safe target.

Accepted for the macOS fixture publication boundary at `df458d30`. The
tests-disabled app displayed a separate write warning and issued one observed
`gh api` POST to each exact fixture endpoint. Recovery recorded one durable
transmission and a terminal receipt for the review and inline comment. The
whole-review UI returned to its enabled New review action; the inline UI
displayed `Published PRRC_fixture9002; refreshed 1 comment(s).` Focused
transport/provider/composition tests cover mismatched identity and target,
timeout, cancellation, durable pre-POST witness and no second POST after an
ambiguous attempt. Source and fixture-log inspection found body transport on
stdin, no token extraction or global `gh` auth change, redacted diagnostics,
and no synthetic body or raw response in logs. Evidence is in ignored
`build/rfc0015-r2-d4-publication-evidence.json`,
`build/rfc0015-r2-d4-final-focused-ctest-2.log`, and
`build/rfc0015-r2-d4-final-fixture-tests.log`. No live pull request was
written. The configured account-switch and helper/process risks remain
disclosed; target-native qualification and RFC-wide evidence remain open.
