---
schema: "implement-rfc.card.v1"
id: "BPR-1625"
title: "Make the MCP terminal audit environment exact on macOS"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"04eaf0e1271d4fccdb14a07ba20bef56788f6713","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ee7534e2f2ba1c4680d92130a8aa7f4b3a4dd86b","evidence":{"kind":"observation","detail":"Frozen helper f063cfca5159527a9fb1c8e781f5bde0cb0f537d8267b07f6ce9d53288c45205 reports258 received/0 retained/0 unexpected for257 supplied names. Disposable CoreFoundation probe proves startup insertion of __CF_USER_TEXT_ENCODING and exact equality once explicitly supplied."}},{"criterion":"AC2","outcome":"pass","revision":"ee7534e2f2ba1c4680d92130a8aa7f4b3a4dd86b","evidence":{"kind":"command","detail":"Full audit_terminal_diagnostics.py against the unchanged frozen helper passes every count/content/bounded/blocked-and-closed-pipe/protocol check. Synthetic startup field is supplied explicitly; exact count equals len(constructed environment), retained and unexpected remain exactly zero."}},{"criterion":"AC3","outcome":"pass","revision":"ee7534e2f2ba1c4680d92130a8aa7f4b3a4dd86b","evidence":{"kind":"artifact","detail":"Three in-memory count negative controls each fail only complete_received_and_retained_counts for independent received/retained/unexpected mismatches; no product/file mutations. DOCUMENTATION_OK and git diff --check pass. Bounded diff self-reviewed; helper/product code unchanged."}}]
---

# BPR-1625: Make the MCP terminal audit environment exact on macOS

The frozen v0.3.0 macOS gate fails only complete_received_and_retained_counts in application_mcp_terminal_diagnostics: 257 supplied synthetic names become 258 after CoreFoundation startup, and all are removed. The coordinator explicitly added this bounded audit repair to the existing milestone. Product count, sanitization and diagnostic output requirements are preserved; no minimum-only assertion or skip is allowed.

## Acceptance

- AC1: Diagnose the frozen native helper count failure with controlled synthetic inputs and bind the runtime startup addition; preserve product count and sanitization behavior.
- AC2: Make every expected startup environment name explicit in the audit, assert exact supplied received counts and zero retained/unexpected counts, and pass the full existing terminal privacy/backpressure audit against the unchanged helper.
- AC3: Run focused negative count controls and documentation/diff checks; commit the bounded test repair and record exact helper and source identities for native reruns.

The [release engineering ledger](../../verification/release-v030-engineering.md) and ignored `.mask/sessions/release-v0.3.0/repair-handoff.json` preserve candidate evidence and coordinator-owned remaining work.
