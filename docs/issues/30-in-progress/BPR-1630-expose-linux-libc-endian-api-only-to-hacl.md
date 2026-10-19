---
schema: "implement-rfc.card.v1"
id: "BPR-1630"
title: "Expose Linux libc endian API only to HACL"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"18dff0d12623520da114392f92d18c4c21536c67","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1630: Expose Linux libc endian API only to HACL

The coordinator explicitly added this bounded repair to the same pending release
repair milestone. The shared native slot permits focused Ubuntu then Windows
work; full normal suites, packaging, signing and publication remain coordinator
work. The frozen Mac baseline stays untouched. No credential changes or signing
are in scope.

## Acceptance

- AC1: Bind the pinned HACL generated C provider to the Linux libc endian API with a private target definition, preserving strict C11, archive verification, hardening and all consumer flags.
- AC2: Reproduce retained native failure, build the exact repaired provider and affected SHA-256 contracts on Ubuntu26.04 without a global flag, verify private scope from generated commands, and retain focused native evidence.
- AC3: Self-review, pass documentation/diff checks, commit explicit paths and return source/command ownership to the coordinator.

Evidence and remaining qualification are bound by the ignored
`.mask/sessions/release-v0.3.0/repair-handoff.json`.
