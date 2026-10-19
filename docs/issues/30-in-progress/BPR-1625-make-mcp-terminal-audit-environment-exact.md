---
schema: "implement-rfc.card.v1"
id: "BPR-1625"
title: "Make the MCP terminal audit environment exact on macOS"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"04eaf0e1271d4fccdb14a07ba20bef56788f6713","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1625: Make the MCP terminal audit environment exact on macOS

The frozen v0.3.0 macOS gate fails only complete_received_and_retained_counts in application_mcp_terminal_diagnostics: 257 supplied synthetic names become 258 after CoreFoundation startup, and all are removed. The coordinator explicitly added this bounded audit repair to the existing milestone. Product count, sanitization and diagnostic output requirements are preserved; no minimum-only assertion or skip is allowed.

## Acceptance

- AC1: Diagnose the frozen native helper count failure with controlled synthetic inputs and bind the runtime startup addition; preserve product count and sanitization behavior.
- AC2: Make every expected startup environment name explicit in the audit, assert exact supplied received counts and zero retained/unexpected counts, and pass the full existing terminal privacy/backpressure audit against the unchanged helper.
- AC3: Run focused negative count controls and documentation/diff checks; commit the bounded test repair and record exact helper and source identities for native reruns.

The [release engineering ledger](../../verification/release-v030-engineering.md) and ignored `.mask/sessions/release-v0.3.0/repair-handoff.json` preserve candidate evidence and coordinator-owned remaining work.
