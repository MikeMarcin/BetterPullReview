---
schema: "implement-rfc.card.v1"
id: "BPR-1428"
title: "Replace CLI publication popup with strict exact-operation admission"
status: "Done"
rfcs: ["RFC-BPR-0079"]
depends_on: ["BPR-1427"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"a8d6cfdc3fdb99d63fc1bc26552e070cb38f4a70","evidence":{"kind":"command","detail":"m3-slice1-exact.log: ordinary composed Comment/Approve/Request changes, inline Publish/recovery and tour Publish/Stop/Resume open no CLI modal."}},{"criterion":"AC02","outcome":"pass","revision":"a8d6cfdc3fdb99d63fc1bc26552e070cb38f4a70","evidence":{"kind":"observation","detail":"Exact-current actor/target/head admission checked before one-use issuance, including asynchronous durable holds; frozen-summary negative and existing exact grant/dispatch authority preserved."}},{"criterion":"AC03","outcome":"pass","revision":"a8d6cfdc3fdb99d63fc1bc26552e070cb38f4a70","evidence":{"kind":"command","detail":"6/6 focused: runtime capability cut retains review/inline/tour drafts with zero writes; non-strict provider lease refused; trust/startup/discovery and preview zero-write assertions."}}]
---

# BPR-1428: Replace CLI publication popup with strict exact-operation admission

Imported under `strict-cli-publication-r2`; current acceptance remains open.

RFC baseline: `aa3d71fe470627e1e78488eb51a9114a788f33bd`. Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** Ordinary review/inline/tour gestures with no extra modal.
- **AC02:** Exact actor/target/head/content grant checks.
- **AC03:** Trust/startup/discovery zero writes and strict capability refusal.

## Engineering result and self-review — 2026-10-13

The shared CLI popup is replaced by typed admission checking the exact current
account/login, host, repository, PR and head, then the retained strict wire.
The capability defaults false; provider admission refuses a CLI lease without it.
Whole-review rechecks after pending comments are held, immediately before grant
issuance; inline rechecks after its durable draft answer, before its grant.
The ordinary workflow still checks prepared content and exact one-use grants;
tour retains its immutable preview and separately selected description edit.

Six focused checks pass: architecture, live mutation provider admission, inline
Publish/lost-result recovery/capability cut, Comment/Approve/Request changes with
frozen-summary/capability negatives, tour Publish and explicit Stop/Resume.
Every composed CLI course detects any additional publication QMessageBox.
Trust/discovery/startup and tour preparation issue zero writes. The provider
course refuses a non-strict lease before effects. Exact adapter acquisition and
M2 native evidence are unchanged and are reused only for those unchanged inputs.

Self-review covers all changed composition/lease/workflow/fixture scopes since
`4ae67d6b`; no authority originates in trust, metadata or discovery, production
grants remain disabled in tests-on builds, and default/native behavior is
unchanged. Tripwire pins bind these reviewed composition paths; mutation-branch
pin is unchanged. Final independent critical review and companion RFC judgments
remain M4 obligations. The synthetic CLI fixture supplies capability/operation
cuts, not stock-gh native credential evidence. The tour native CLI variant
covers Publish; Stop/Resume covers restart recovery. The unchanged native
attribution-restart course remains separately enabled for native fixtures.

Private focused result: `/tmp/bpr-strict-cli-run/m3-slice1-final.log` (6/6, 27.50 s).
The initial inline expectation and ordered-read fixture-selection failures were
diagnosed and corrected; the CLI fixtures use independent read replay as the
existing remembered-connection courses do. No gate or grant issuer is weakened.
