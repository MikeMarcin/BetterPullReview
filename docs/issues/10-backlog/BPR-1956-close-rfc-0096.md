---
schema: "implement-rfc.card.v1"
id: "BPR-1956"
title: "Verify and close RFC-BPR-0096"
status: "Backlog"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1951", "BPR-1952", "BPR-1953", "BPR-1954", "BPR-1955"]
closeout_for: ["RFC-BPR-0096"]
---

# BPR-1956: Verify and close RFC-BPR-0096

## Scope and acceptance

Judge every criterion AC01–AC12 of the
[RFC](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md) against
actual evidence from the preceding cards, including the defined public-path
and installed-agent checks, contract reconciliation and documentation gate.
Record failures and unreached requirements precisely. This is
`accepted-scope` in the [plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Leave live IDs, the implementation baseline, acceptance records and candidate
identities to authorized execution. This proposed closeout has no passing
implementation evidence and does not alter any live RFC status or historical
native, human, release or security obligation.

## Execution checkpoint

RFC_BASE_SHA: `bebea27edf44fd63a7308307227035bf33391c5c`. Mandate `native-agent-sessions-r1` implements the reviewed pack, superseding its authoring-only stop boundary. All twelve criteria remain pending. Initial milestone: native-agent-tab, BPR-1951; no passing product evidence yet. External continuation: `/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m1-handoff.md`. No task-owned background commands or attended candidate.

## Closeout checklist

RFC base: `bebea27edf44fd63a7308307227035bf33391c5c`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Both conversational surfaces demonstrate native multi-turn continuity with the same backend handle and workspace. A follow-up uses a prior tool observation absent from BPR's prose replay. Claude's input stays open after the first result; qualified Codex protocols follow the defined persistent-connection or native-resume behavior. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC02:** Switching agents, PRs, tour parts and stop order returns each scope's own session. Replacement/reset agent entries and replacement stops cannot acquire another identity's conversation; removed-stop history stays identifiable without becoming a new stop's history. Reopening a generation preserves its identity, and changed authority/configuration is rechecked. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC03:** Closing and reopening a chat, review and application restores history and permits resuming the exact native session and its preserved workspace, including retained allowed file changes. Startup recovery does not reap it. Viewing history performs no inference; failed saves, missing native state and missing required workspace/object state are reported without an invented successful resume. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC04:** New session starts a distinct generation only for the selected scope, preserves the previous session in history, and leaves accepted tour state, notes and drafts intact, without inference or a clear-confirmation dialog. Failed admission preserves the selected session. An earlier entry can be continued after binding checks; Reset tour/Undo reset and pending proposals retain the defined associations without reviving superseded work. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC05:** Stop, uncertain sends, process failure and restart produce visible outcomes with no automatic retransmission. A late completion/frame cannot enter another turn/generation or apply a stale proposal. Two BPR windows cannot concurrently write one native session; a completed answer does not tear down a live conversation or make its idle interval a turn timeout. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC06:** Unsupported backends, unavailable native sessions and legacy histories present the defined labelled replay and New session choices. Replay creates its own generation, preserving native provenance and exposing its prose/workspace limits and omissions. Ambiguous legacy agent identity is not silently reassigned, and replay cannot bypass read-only or authority restrictions. No failed turn is silently resent. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC07:** Head movement exercises both established stale continuations with the correct workspace and analyzed OIDs. Same-head edits, state changes without an accepted-tour revision change, proposal decisions and runtime compaction/reconnect receive current bootstrap facts on the next explicit question, with no observation-triggered inference or stop-number rebinding. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC08:** Captured tour requests distinguish all source/preparation states in the status table, including related lines, whole-tour blockers and unknown evidence. Confirmed lines with unfinished prose remain distinguishable from unconfirmed lines. Oversized composed prompts send no partial request. The guide and structured reasons let an installed agent explain a fixture's unconfirmed stops and actual corrective actions without BPR source access; an identical path/range with a different confirmation state is distinguishable. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC09:** A composed update course preserves confirmation for text-only edits and ordering while applying the existing preparation/Ready rules, and exposes the actual confirmation consequences of new, split and retargeted stops. UI status, the agent snapshot and core eligibility agree; confirmed author-selected replacements do not receive a warning solely because of their mapping kind. Agent output cannot confer human confirmation, Ready or publication authority. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC10:** Focused production-composition checks cover persistent streams, proposal acceptance and the New session/history controls with rendered offscreen UI evidence. Installed Claude Code and Codex courses identify their exact version, protocol and platform and exercise native resume under the requested permissions; fixture success is not claimed as installed-agent or all-platform qualification. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC11:** Equivalent Release native/replay courses report actual reuse and available usage/cost metrics, without a fixed savings claim. Long sessions exercise bounded active/idle output, catalog and workspace admission, safe reclamation and full-capacity refusal without losing protected state. Removed context selections, diagnostics and resumed permissions preserve the defined privacy, read-only and explicit-invocation boundaries. | Pending scoped functional/installed course | Pending | Not implemented |
| **AC12:** Operator and architecture documentation reconcile the proposed amendment, explain transcript/workspace ownership and retention, and distinguish native continuation, replay, New session, Reset tour and purge. Focused acceptance and the documentation gate pass for the eventual change. | Pending scoped functional/installed course | Pending | Not implemented |
