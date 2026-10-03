---
id: "RFC-BPR-0012"
title: "AI Review Findings, Comment Workshop, Side Chat, and MCP Context"
status: "Auto-Verified"
area: "BPR"
milestone: "M5 Explicit AI Assistance"
depends_on: ["RFC-BPR-0008","RFC-BPR-0011"]
pack_version: 2
metadata: {"milestone_plan":{"id":"rfc-bpr-0012-owner-closeout","path":"docs/rfcs/plans/rfc-bpr-0012-owner-closeout/milestone-plan.json"}}
---

# RFC-BPR-0012: AI Review Findings, Comment Workshop, Side Chat, and MCP Context

## Summary

Add explicit AI review operations, structured findings, comment workshop, revision-aware side chat, full-agent launch, and a read-only local MCP context helper.


## Motivation

Once agent launch and context boundaries are sound, BetterPullReview can add high-value explicit assistance for analysis and comment quality without turning the product into an autonomous reviewer.

## Goals

- Add Ask About Selection, Analyze File, Analyze Latest Upload, Evaluate Fix, Workshop Comment, AI Review, and Full Agent Session actions.
- Add a revision-aware side chat drawer/tab.
- Validate structured findings and convert them to private drafts.
- Add a read-only BetterPullReview MCP context helper for approved external agents.
- Preserve normal GitHub publication and agent approval behavior.

## Non-goals

- Automatic AI invocation, re-run, publication, resolution, verification, approval, or requested-changes decisions.
- Granting the MCP helper GitHub credentials or mutation tools.
- Treating AI confidence as proof.

## Architectural allocation

- **Core:** owns finding-schema validation outcomes, path/OID/revision anchoring policy, chat scope, stale-session state, explicit conversion of a finding into a local draft, and prohibition of automatic publication.
- **Host:** owns Codex/Claude protocol sessions, MCP stdio helper lifecycle, context retrieval, structured-output parsing limits, cancellation, and typed completion events. GitHub publication is delegated to the RFC-0008 host mutation path.
- **Qt adapter:** owns the side-chat, context chips, finding queue, before/after comment workshop, approvals, and explicit promote/dismiss/publish gestures.
- Neither agent output nor MCP input can mutate core/Qt state directly; all results re-enter through typed events and revision/generation checks.

## Structured findings

Findings contain severity, confidence, category, path, line range, claim, evidence, consequence, suggested comment, required context, provider, and analyzed OIDs. BetterPullReview validates anchors against the frozen snapshot and marks invalid or stale results explicitly.

## Side chat

The chat shows provider, host, workspace, revision, context chips, and current/stale state. Navigation does not silently add context. Expanding scope or adding private notes/drafts requires explicit action.

## Publication

AI text remains a local proposal until the user converts it to a draft and publishes through ordinary review controls. Full-agent mode may publish through the agent's own GitHub tooling subject to its native approvals; resulting state returns through RFC-0008 synchronization.

## Comment workshop under RFC-BPR-0023

Owner decision `BPR-1245-workshop-v001` (accepted 2026-10-03, built on
[[BPR-1245]]): RFC-BPR-0023 does not replace this RFC's workshop, but its
private-run rule applies to it. No run carries a mutable draft or note. A
workshop press therefore first saves the draft's current text in Private
history as an immutable reviewer revision, then asks the agent with that
revision as the only selected source. The six operations, one turn per press,
and the proposal the reviewer accepts or rejects are unchanged. Each press adds
one reviewer revision to Private history, and History shows it like any other.

## MCP helper

The helper uses stdio, exposes only read-only frozen review tools, has no network listener, no credential, no arbitrary filesystem root, and no mutation operation.

Finding and chat anchors consume RFC-BPR-0004 semantic source-line, boundary,
segment, group, and move-relation identities. They never infer an exact line
from measured y-coordinates or a ribbon path, and uncertain/opposing-span
mapping remains visible.

## Acceptance criteria

- Every AI action originates from an explicit visible user command.
- Comment workshop preserves the original draft and shows a reviewable proposed revision.
- Structured findings validate path/line/OID and cannot silently attach to a newer revision.
- Converting a finding creates only a local draft until explicit publication.
- Full-agent GitHub changes round-trip through normal synchronization without a private backchannel.
- The side chat shows stale state after head movement and offers new-session or explicit old-revision continuation.
- The MCP helper exposes only documented read-only operations and cannot access credentials, network, writes, or unrelated filesystem content.
- Private notes and drafts are never included without explicit selection.
- Finding/chat/promotion decisions are core-tested without agent/Qt dependencies; host protocol tests and Qt workflow tests prove all results re-enter through typed revision-bound events.

## Verification matrix

Owner closeout amendment r4 (2026-09-21), applied by explicit coordinator
assignment: retain the nine criteria above and the original BPR-0496 fixture
acceptance scope. BPR-0507 reconciles stale closeout text and current evidence
after RFC-BPR-0008 through RFC-BPR-0011 became Auto-Verified. The full-agent
criterion is accepted through the local full-agent no-backchannel course and
ordinary RFC0008 fixture synchronization, as authored; it does not claim a
live external-agent GitHub write, installed-agent compatibility, independent
authentication, native approvals or package qualification. Those remain
unobserved follow-ups in the [verification ledger](../verification/RFC-BPR-0012.md#owner-amended-closeout-r4--2026-09-21).
The later Dif workshop-cue failure is preserved separately as BPR-0966; its
test is not described as passing and no RFC0012 criterion is removed.

| Claim | Direct surface | Proof |
|---|---|---|
| No automatic invocation | UI/event integration | Exercise refresh/navigation/comment arrival and inspect process launches |
| Finding anchors are validated | Structured result path | Return valid, invalid, and stale anchors |
| Publication remains explicit | Draft workflow | Convert finding and prove no GitHub mutation until publish |
| MCP is read-only | Protocol conformance/security test | Enumerate tools and attempt forbidden network/filesystem/write access |
| AI results re-enter as typed events | Protocol + transition + Qt tests | Direct process output cannot publish or mutate UI/core outside the event path |

## Documentation impact

AI operations, context controls, finding schema, staleness, publication boundary, full-agent behavior, MCP tools, privacy, and limitations are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
