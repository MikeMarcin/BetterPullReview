---
schema: "implement-rfc.card.v1"
id: "BPR-0700"
title: "Separate profile behaviour preferences and window layout persistence"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696","BPR-0698"]
acceptance: [{"criterion":"Bounded preference sharing, keyed layout migration, compatibility and refusal acceptance","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Host0.43s, consumers0.31s, storage0.47s pass with migration rollback/preflight/cap32/independent window rows and foreign/full/closed zero-write regressions. All7TUs plus final corrections tidy pass. Rebuilt composed workbench25.18s and source-mutation inventory0.727s pass; documentation gate passes. Medium review blocker corrected; actual multiwindow app binding remains later."}}]
---

# Separate profile behaviour preferences and window layout persistence

## Scope

Separate shared review-behaviour preferences/new-window defaults from each
window's layout. Each workbench host retains navigation, cursor, observation and
selected-diff state. A single profile preference owner publishes accepted
behaviour to its windows. Window layout persistence is bound to stable window
identity; writing one layout cannot replace a sibling or shared preferences.

Migrate the existing singleton workbench-settings record into profile preferences
and new-window defaults transactionally. Do not invent a legacy window identity.
Missing window rows use defaults without a write; explicit typed erase removes
only the requested window record. Bound records and preflight before mutations;
future/malformed/cancelled migrations leave existing data unchanged. Keep the
existing application facade composed through the new ownership seam.

Actual restoration decisions, close-versus-Quit calls to erase, shared Qt Settings
and multiple-window runtime construction are later work.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, bookkeeping and acceptance. Workers have
exclusive queue or workbench/storage paths and preserve parallel changes.

## Acceptance

Two window IDs persist different drawer/line-number/width state while sharing
accepted behaviour preferences. Legacy singleton migration preserves defaults and
behaviour, interrupted/repeated/future/malformed cases refuse or recover honestly,
and one-window erase preserves siblings. Host tests retain selected-view and
stale-result rules and prove preference fan-out without layout replacement.
Existing workbench/SQLite checks and one composed workbench gesture path pass.

## Evidence

The application now uses an explicit defaults store and passes the actual BPR
profile to the preference owner. This compatibility path does not invent a
persistent window identity. Schema5 separates preferences/defaults from at most
32 keyed window layouts; legacy migrations create zero window rows. Missing
window rows use defaults without writing, and typed erase preserves siblings.

Focused host tests pass (0.38s), shared consumers including reentrant fan-out
pass (0.29s), and storage migration/bounds/cancellation tests pass (0.47s).
All seven changed translation units and final cancellation changes pass tidy.
The rebuilt composed workbench gesture course passes (25.18s); the strict app
source/mutation-bypass inventory passes (0.727s) with updated source pins.

Independent medium review found one refused-subscription authority bug. The
correction fences preference effects and accepted-owner reads when attachment
fails; foreign/full/closed owner regressions pass with zero writes. Host and
consumer tests pass (0.43s/0.31s), and both changed TUs pass tidy. No additional
migration blocker was found. The documentation gate passes. Actual multiple-window
runtime/restore remains later.
