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

RFC_BASE_SHA: `bebea27edf44fd63a7308307227035bf33391c5c`.
Mandate `native-agent-sessions-r1` implements the reviewed pack, superseding its
authoring-only stop boundary. First milestone `native-agent-tab`, BPR-1951,
completed scoped functional engineering at product candidate
`26fdce665d7ff09f9ecd52e0df6765ce569b21ff`. The [ledger](../../verification/RFC-BPR-0096.md)
records passing visible Agent-tab/native continuation and restart, actual installed
Codex resume, retained Git object/edit recovery, storage/CAS/refusal and focused
architecture/format/tidy/documentation evidence, with prior failures preserved.
All twelve full-RFC criteria remain pending their complete cross-surface scope.

Second milestone `tour-sessions`, BPR-1952, completed functional engineering
at product candidate `5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e`. Native conversations
are scoped by agent, tour draft/reset generation, whole/Problem/Solution/stable
stop; Draft Tour shares whole-tour native ownership. Nondestructive New session,
exact generation history, read-only detached inspection and Reset/Undo preserve
accepted state and proposal origin. Four production seed/reopen courses and
inspected offscreen windows, 12 functional/integration checks, core-only checks,
changed workbench, format/tidy and documentation gates pass. See the ledger for
actual intermediate failures and scoped limits. All twelve full-RFC criteria
remain pending their complete cross-surface/installed/qualification scope.

At the M2 checkpoint the next eligible milestone was `tour-context`, BPR-1953. BPR-1954/1955 retained legacy
migration, both-direction shared quota/projection failure handling, labelled
replay, installed Claude (currently signed out), installed permissions, resource,
resume-only and Release usage qualification. Native Windows/Linux acceptance is
not inferred. The adopted five groups and immutable original pack are unchanged.

External continuation:
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m2-handoff.md` and bound
`m2-checkpoint.json`. M1 product `26fdce665`, finish `3a4c055f0` and Ready
`9fe9be9f1` remain prior evidence. Unrelated ID-reservation ancestor `2e178b953`
was preserved; it changed no product inputs. BPR-1952 finish commit `1fae95d03e2336cd3a61e5856370071cb1b9ebdf`;
BPR-1953 is Ready. M2 build/test/render/static-analysis commands have completed; no attended candidate, review server, task worktree, remote
mutation or push exists.

Current frontier after the owner acceptance amendment: BPR-1953 and BPR-1954
are Done at products `a89b0fa88f5a8e4c0eae8495ab8a44da8f7377e8` and
`2de6a3336a02b7aae5a62700d17b620eff98451d`. BPR-1955's unchanged product
`f98e14cd604a251a32e8cfeb9c33a5b0614a489a` has passing installed Codex,
public-path, resource, usage and operator-documentation evidence.

The owner's actual instruction is: “the claude is out of usage, just keep going
without verifying it works”. Acceptance amendment
`native-agent-sessions-claude-waiver-r1` under mandate
`native-agent-sessions-r1` waives only the remaining installed Claude verification
for pack progression and acceptance. The
[live RFC](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md#owner-acceptance-amendment-2026-11-08)
and amended M4 plan exit bind the changed requirement. Installed Claude remains
unverified: the fresh authenticated HTTP429 weekly-limit failure, no usable
answer/tool activity, and unreached native/replay/resume/Stop/explanation/usage
courses remain recorded in the
[ledger](../../verification/RFC-BPR-0096.md#owner-claude-verification-waiver-and-m4-acceptance).
No fixture result or waiver is represented as a passing installed Claude course.
All unrelated requirements and prior evidence retain their meaning.

The weekly-availability wizard `94eafb5d-7d90-4146-9de5-f8802d8bade4` is
superseded by that amendment. Its host was stopped through the helper at consumed
revision1; the human acknowledgement and following observation remain unanswered.
History is retained in external state
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-weekly-pnwqk34v/state`.
No review server, retry or inference remains active.

M4 is accepted under the amended requirements. After BPR-1955 finishes this card
is dependency-eligible for Ready and belongs to the fresh `accepted-scope` owner.
Recovery is
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-waiver-handoff.md`.
The full criterion-by-criterion closeout below remains that owner's work; the RFC
stays Proposed and historical native, release and security limits are preserved.

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
