---
schema: "implement-rfc.card.v1"
id: "BPR-0700"
title: "Separate profile behaviour preferences and window layout persistence"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696","BPR-0698"]
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

Read-only ownership inventory established this bounded split. No implementation
or new acceptance check has run for this card. BPR-0697/0698 extraction logs and
finish decisions were consumed and removed; shared build outputs remain.
