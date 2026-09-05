---
id: BPR-0507
title: Verify and close RFC-BPR-0012
status: Backlog
rfc: RFC-BPR-0012
depends_on: [BPR-0242, BPR-0245, BPR-0496, BPR-0497, BPR-0498, BPR-0499, BPR-0500, BPR-0501, BPR-0502, BPR-0503, BPR-0504, BPR-0505, BPR-0506, BPR-0513, BPR-0514, BPR-0517, BPR-0521]
---

# Verify and close RFC-BPR-0012

RFC base: `2966ff8103dc442775425386f6a31f8ac1a391c6`.

Requested RFC set for the run that opened this card: the remaining pack in
canonical order — RFC-BPR-0006 through RFC-BPR-0014 — with RFC-BPR-0012 the
first of them to start from nothing. Closure stays ordered: this card cannot
mark the RFC `Auto-Verified` before RFC-BPR-0008 ([[BPR-0242]]) and
RFC-BPR-0011 ([[BPR-0245]]) are, which is why both closeout cards are
dependencies here.

Planned cards: BPR-0496 (this plan) through BPR-0506; see
[BPR-0496](../60-done/BPR-0496-plan-rfc-0012.md) and the
[RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md).

When this card is prepared, replay against the composed application and ask
the linker: `scripts/verify_composed_hosts.py` must see the finding, chat,
workshop, and MCP constructors in the shipped binary, and the composition
contract must reach them from a real gesture. Green component suites against
an uncomposed feature are exactly the gap BPR-0180 and BPR-0188 found.

## Closeout checklist

One row per canonical acceptance criterion: the check that would fail if the
criterion were false, its result, and the commit it was produced at. Selector
names are the ones the planned cards register; a card that registers a
different name updates this table and the ledger atomically.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [ ] | Every AI action originates from an explicit visible user command. | Core explicit-action legality plus Qt entry points and the composed refresh/navigation/comment-arrival course | `ctest --preset dev -R '^(core_agent_chat_scope\|qt_side_chat_panel\|application_agent_finding_composition_contract)$' --output-on-failure` | Refresh, navigation, comment arrival, and revision update launch nothing; each of the seven entry points launches only from its visible command | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0499, BPR-0502, BPR-0504 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | Comment workshop preserves the original draft and shows a reviewable proposed revision. | Workshop surface inside the review draft editor's semantic-command rule | `ctest --preset dev -R '^(qt_finding_queue_view\|qt_review_draft_editor)$' --output-on-failure` | The original draft is retained unchanged beside the proposed revision; only an explicit accept replaces it, through a semantic command | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0503 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | Structured findings validate path/line/OID and cannot silently attach to a newer revision. | Core finding validation against RFC-0004 identities and the frozen revision binding, plus host re-entry | `ctest --preset core-only -R '^core_agent_finding$' --output-on-failure` and `ctest --preset dev -R '^application_agent_finding_protocol$' --output-on-failure` | Valid anchors resolve; invalid ones are unanchored; a head moved after analysis makes the finding stale rather than re-attaching it | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0497, BPR-0500 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | Converting a finding creates only a local draft until explicit publication. | Core promotion function and the composed finding-to-draft course with the mutation host observed | `ctest --preset core-only -R '^core_agent_finding$' --output-on-failure` and `ctest --preset dev -R '^application_agent_finding_composition_contract$' --output-on-failure` | Converting yields exactly one local draft or defect command and zero mutation-host requests until the ordinary Publish action | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0498, BPR-0504 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | Full-agent GitHub changes round-trip through normal synchronization without a private backchannel. | Full-agent launch through the RFC-0011 host with the RFC-0008 fixture synchronization path | `ctest --preset dev -R '^application_agent_finding_composition_contract$' --output-on-failure` | A full-agent session receives no BetterPullReview credential or backchannel; shared changes appear only after ordinary refresh | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0504 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | The side chat shows stale state after head movement and offers new-session or explicit old-revision continuation. | Core staleness reuse and the Qt side-chat panel | `ctest --preset dev -R '^(core_agent_session\|core_agent_chat_scope\|qt_side_chat_panel)$' --output-on-failure` | After head movement the chat shows stale and offers exactly new-session or explicit old-revision continuation; nothing retargets silently | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0499, BPR-0502 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | The MCP helper exposes only documented read-only operations and cannot access credentials, network, writes, or unrelated filesystem content. | MCP helper canary enumerating tools and attempting forbidden access | `ctest --preset dev -R '^application_mcp_context_helper$' --output-on-failure` | Only the documented read-only tools are listed; credential, network, write, and out-of-snapshot filesystem attempts are refused and observable | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0501 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | Private notes and drafts are never included without explicit selection. | Core context-inclusion policy and the two-process private-context canary | `ctest --preset dev -R '^(core_agent_context_supplement\|core_agent_chat_scope\|application_agent_invocation_composition_contract)$' --output-on-failure` | Unselected notes and drafts are absent from the child's stdin and from every finding/chat payload; selected ones appear exactly | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0499, BPR-0504 | automated | No live blocker; the owning cards are not yet implemented. |
| [ ] | Finding/chat/promotion decisions are core-tested without agent/Qt dependencies; host protocol tests and Qt workflow tests prove all results re-enter through typed revision-bound events. | The whole core, host, and Qt selector set | `ctest --preset core-only -R '^(core_agent_finding\|core_agent_chat_scope\|core_agent_session)$' --output-on-failure` and `ctest --preset dev -R '^(application_agent_finding_protocol\|application_mcp_context_helper\|qt_side_chat_panel\|qt_finding_queue_view\|application_agent_finding_composition_contract)$' --output-on-failure` | Core suites link no agent process or Qt; host and Qt suites observe every result arriving as a typed event carrying the exact revision binding | Claim matrix row in [RFC-BPR-0012 verification ledger](../../verification/RFC-BPR-0012.md#claim-matrix); owning card(s) BPR-0497 through BPR-0504 | automated | No live blocker; the owning cards are not yet implemented. |
