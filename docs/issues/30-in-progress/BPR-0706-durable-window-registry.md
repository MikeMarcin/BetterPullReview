---
schema: "implement-rfc.card.v1"
id: "BPR-0706"
title: "Persist bounded window identities in the profile registry"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0705"]
---

# Persist bounded window identities in the profile registry

## Scope

Extend the existing profile registry DTO and SQLite schema to version 2 with a
global LastWindowId high-water mark and each profile's ordered retained window
records (stable ID and latest generation). Reuse core bpr_window_record and core
catalog validation. The order is least to most recently focused, matching core
policy. Keep one global CAS transaction for profile identities, root authority,
window allocation metadata and retained rows. Do not create a second database or
persist Live runtime state, private review content, credentials, request IDs or
provider snapshots. The private per-profile restoration payload is later work.

Schema 1 migration preserves every existing profile/root/revision exactly and
adds LastWindowId=0 with zero retained rows. Never invent a Default window or
Resume target. Bounds are the existing 64 profiles and 32 windows per profile,
with global uniqueness and canonical identity encoding. Updates cannot regress
allocation or a surviving window generation, transfer a stable window to another
profile, or reintroduce a previously removed ID at/below the accepted high-water.
Removal and reordering are permitted for later close/focus policy.

This is a storage foundation: it does not yet restore windows or claim runtime
Close/Quit persistence. Existing create/rename must preserve accepted window
metadata. The following host integration must commit lifecycle metadata before
emitting start/close/quit effects. Corrupt or future allocator metadata refuses;
optional private payload failure must later be distinguished from allocator
failure, so corrupt high-water never silently resets and reuses identity.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, acceptance, commits and board. Worker owns
profile_registry.hpp/.cpp, profile_registry_store.cpp and their direct tests.

## Acceptance

Direct application/SQLite contracts prove exact schema1 migration with no invented
windows, schema2 reopen and MRU preservation across multiple profiles, atomic CAS,
monotonic allocation/generation and unchanged profile authority. Malformed IDs,
zero generations, duplicate or foreign window ownership, excessive records,
noncanonical/missing ordinals, counter regressions and future schema refuse
without mutation. Injected interruption rolls back migration and row replacement
including schema stamp, revision and high-water. Existing profile creation and
rename preserve retained metadata; cancellation does not publish uncommitted
acceptance. Focused current management/storage checks and documentation pass.

## Evidence

Read-only scout traced the existing global registry CAS and core retained-window
policy. No schema2 implementation or acceptance has run for this card.
