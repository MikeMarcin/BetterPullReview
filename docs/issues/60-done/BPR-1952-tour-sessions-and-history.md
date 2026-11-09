---
schema: "implement-rfc.card.v1"
id: "BPR-1952"
title: "Add per-agent tour sessions and New session history"
status: "Done"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1951"]
acceptance: [{"criterion":"AC01-AC02-tour-native-scopes","outcome":"pass","revision":"5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e","evidence":{"kind":"observation","detail":"Four actual-composer public9 seed/reopen courses pass both native fixture protocols across whole/Problem/Solution/stable-stop and agents. Distinct scoped processes and retained followups; Draft Tour adds to whole-tour native generation. Stable stop identities retain history across order changes; removed/replaced/reset scopes stay separate under core ID/highwater and catalog checks.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution"}},{"criterion":"AC03-AC04-durable-New-session-history","outcome":"pass","revision":"5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e","evidence":{"kind":"observation","detail":"Native SQLite restart, exact-ID selection, late-origin append, same-head new generation, unused-empty reuse, detached read-only inspection and capacity-refusal preservation pass. Visible Agent/tour New/history and full application reopen pass without inference; four rasters inspected.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution"}},{"criterion":"AC04-AC05-state-origin-reset","outcome":"pass","revision":"5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e","evidence":{"kind":"observation","detail":"Public New preserves accepted Problem/Solution/stops and pending draft parts; Reset/Undo restores associations. Core reset/highwater, codec v9/backward reads, chat current-context retention, existing cancellation/stale/uncertain-send/CAS fences and origin-only transcript writes pass focused checks.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution"}},{"criterion":"tour-native-permission-configuration","outcome":"pass","revision":"5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e","evidence":{"kind":"observation","detail":"Launch functional course asserts Claude persistence change preserves read-only restrictions; App Server create/exact resume/each turn reapply read-only/network-off/never approval with MCP/web disabled. Tool-free hooks remain separate one-shot. Installed enforcement explicitly remains BPR1955.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution"}},{"criterion":"focused-integration-rendering-upkeep","outcome":"pass","revision":"5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e","evidence":{"kind":"observation","detail":"Final12 functional/integration, core-only3, changed workbench scale100, public4 courses, inspected offscreen rasters, format22 on34 files, scoped tidy22 on13 units, documentation gate and67 script tests pass. Frontend-design loaded; no desktop/live remote/model API. Full RFC and legacy/installed/resource/Release obligations retained.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution"}}]
---

# Add per-agent tour sessions and New session history

## Scope and acceptance

Extend the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
native host to the whole-tour, Problem, Solution and stable-stop threads,
separately per agent. Share the whole-tour native conversation with initial
tour drafting. Add scoped durable history and New session behavior in both
surfaces without Clear's destructive confirmation, with accepted work and
pending proposal identity preserved. Cover failed retention, history selection,
removed stops and Reset tour/Undo reset associations. Qualify the tour's
native permission configuration; keep tool-free hooks separate. This is the `tour-sessions` milestone in the
[plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover the tour portions of AC01–AC03, New session/history in AC04, and the
tour/new-generation portions of AC05. Exercise stop removal/replacement,
reordering and agent switching without inheriting unrelated conversations.
Rendered control checks contribute to AC10; installed qualification remains
on its own proposed card.

## Verification and execution record

Mandate `native-agent-sessions-r1`; implementer `/root/native_sessions_m2`.
Claim `fba210806f7277039d0214d132211ff022de1902`; product candidate
`5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e`. The [ledger](../../verification/RFC-BPR-0096.md)
records native tour/Agent-tab seed and application reopen through actual composers,
scoped history/New session, Draft Tour's shared whole-tour generation and
Reset/Undo associations, with inspected offscreen windows. Functional/integration
checks passed 12/12; core-only checks 3/3; the changed workbench scale100 course,
clang-format 22, scoped clang-tidy 22, documentation gate and 67 documentation
script tests passed. Failed intermediate attempts and their causal corrections
remain in external logs; no structured-output validator was relaxed.

New session/history changes no accepted tour/source/focus, note or draft state.
The native catalog preserves old generations and original terminal provenance;
full capacity refuses without changing selection. Version-nine tour payloads
retain reset namespace high water through Undo; older versions default to zero.
Read-only detached inspection cannot activate a removed stop or reset draft.
Frontend-design was loaded before Qt edits; all rendering was offscreen.

This accepts this milestone's functional scope only. BPR-1953 retains the tour
guide/status/source-warning work; BPR-1954/1955 retain legacy/replay migration,
bidirectional shared quota/projection failures, installed Claude (signed out),
installed tour permissions, full resource and Release cost qualification. No full
RFC or Windows/Linux acceptance is inferred. No attended candidate exists.
External handoff: `/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m2-handoff.md`.
