# Design record

## Scope and provenance

Working draft v1, authored 2026-11-08 for both the Agent tab and Review Tour
side chats. The owner requested the author-rfc skill and explicitly said
“Don't implement yet.” Later answers below refine the same requested scope.
The owner additionally requested an Astra subagent at max reasoning to review
and update this pack after the initial draft. That review revised this same
unadopted v1 pack. The pre-review snapshot is preserved outside the editable
pack at
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-native-sessions-authoring-2azgdq_f/draft-v1`;
it was not changed by the review. The
[proposed amendment](proposed-session-amendment.md) binds inspected live
sources by content hash; none is altered by authoring.

## Decisions

| Decision | Authoritative behavior | Status and actual answer | Consequences |
|---|---|---|---|
| Actual agent-session continuity | RFC, Native sessions and persistent JSON connections | Settled intent: “persistent session per side-chat per-agent”; later “whatever the vscode claude code plugin does if that's possible”, extended explicitly to both surfaces in the RFC request | Native backend conversation and tool history are the intended continuity; saved prose alone is insufficient. |
| Start fresh while retaining history | RFC, New session and returning to history | Settled: owner selected “Start fresh; keep the old session in history” | Prior conversations remain available; accepted BPR work stays outside conversational reset. |
| Unsupported-native fallback | RFC, Native capability, fallback and existing histories | Settled: owner selected “Offer explicitly labelled transcript replay” | Replay is offered openly, with its lost-tool-context limitation; there is no silent downgrade. |
| Control name | RFC, New session and returning to history | Settled: “'New session' like vscode is probably better than clear” | The proposed user-facing action is New session, not deletion. |
| Persistent JSON stream | RFC, Native sessions and persistent JSON connections | Proposed explicitly in response to “You said vscode claude uses a persistent json stream? Are we going to do that?”; the response commits the draft to a persistent Claude stream-json connection and equivalent Codex JSON-RPC connection. The owner has not separately confirmed the finished draft. | Distinguish turn completion from process completion, retain exact native IDs for reconnect; do not attribute unverified extension internals to upstream. |
| Current tour facts and product knowledge | RFC, The tour agent's guide and structured status | Requested: give the tour agent the status/context it needs after it could not explain the reported source warnings | Include structured reasons, eligibility and actions plus a versioned BPR tour guide; retain explicit unknown evidence. |
| Staleness, source confirmation and publication | RFC, Updated BPR state and revision changes; Ownership, compatibility and qualification | Preserved established requirements; new shared reason projection and bounded warning correction are proposed | Persistent memory grants no automatic retargeting, human confirmation, Ready or publication authority. |
| Reviewed lifecycle and fallback details | RFC, Native sessions; New session and returning to history; Native capability | Proposed refinements from the requested Astra max review, not additional owner answers | Separate durable identity from window activation, retain original native provenance when replaying, remove Clear's destructive confirmation, and preserve Reset tour/Undo reset associations. |
| Reviewed storage and context boundaries | RFC, Session scope, workspace and history; Updated BPR state; Structured status | Proposed refinements from the requested review within the settled outcomes | Safe storage refusal protects modified work; all relevant facts drive snapshot freshness; source confirmation, Ready and publication remain distinct. |
| Backend/state/storage mechanics | RFC, Session scope, workspace and history | Deferred to implementation within defined outcomes | Durable layout, bounded storage thresholds, protocol-version selection and shared projection types need no separate owner choice unless a discovered constraint would change the stated behavior. |

All requirements and acceptance live in the
[RFC](RFC-DRAFT-0030-native-agent-sessions-and-tour-context.md); cards and
milestones derive from it. A proposed detail is not recorded as an owner
answer.

## Open frontier and explicit deferrals

There is no unanswered owner choice in the requested session identity,
New session/history semantics or replay-fallback policy after the review.
Exact UI placement, wire field names, backend-version selection, bounded
storage thresholds and durable storage layout remain
engineering decisions within the stated contract. UI implementation must
load frontend-design first. This pack includes no mockup, Qt edit or visual
redesign.

The exact reported transition to every stop warning has not been reproduced.
The RFC preserves that uncertainty and specifies distinguishing update and
status observations. Authoring does not assert that all confirmed stops were
invalidated, that native resume works in an installed wrapper, or that it
achieves a particular cost saving.

Backend/tool authorization, GHES host restrictions, source identity, explicit
publication and existing native/human obligations remain in force. Adoption
must reconcile currently active writers and candidate protection; nothing in
this proposal reopens Done cards or accepts earlier unobserved criteria.

## Requested review

Astra max reviewed the entire draft pack against the cited host/protocol,
conversation-store, tour-prompt, core source/preparation and Qt warning paths,
the live session/workspace/tour contracts, and the linked official Claude and
Codex documentation. It resolved these concrete ambiguities:

- Durable conversation identity now differs from window activation and turn
  identity; one BPR writer, uncertain sends, late frames, idle deadlines and
  runtime auto-continuation have explicit boundaries.
- Retained workspaces survive normal close/restart and startup recovery,
  account for borrowed objects and allowed changes, and cannot escape limits
  through protected or orphan records. Unsafe reclamation refuses admission.
  The 128-entry invocation catalog and 500-record conversation history are
  distinct.
- New session retains the old generation without a destructive confirmation;
  failed retention preserves selection. Replay branches with its own identity,
  preserves native provenance and cannot restore missing tool/filesystem
  state or bypass tour permissions. Legacy agent-slot ambiguity is explicit.
- Snapshot freshness covers observations, proposal outcomes and selections,
  with a bootstrap fallback after compaction/reconnect. Updates ride explicit
  sends; old context cannot quietly become a narrower privacy scope.
- Source confirmation, preparation/Ready, explanation reaffirmation and
  publication blockers are separate facts, including whole-tour restrictions.
  Text edits may preserve lines while requiring renewed Ready. Returned stop
  numbers bind to the submitted snapshot, and the whole prompt retains its
  bound.

The review retained persistent Claude stream-json, Codex App Server JSON-RPC
and the disclosed Codex JSONL native-resume exception. It made no assertion
about undocumented VS Code internals and did not run installed-agent inference,
reproduce the owner's exact warning sequence or grant implementation
acceptance. Cards, amendment, glossary and canonical plan were reconciled;
milestone grouping and all twelve uniquely defined criteria were preserved.

## Checks and confirmation

- **Structural validity:** the initial draft passed the canonical offline
  validator and its generated milestone view matched a fresh render. A
  pack-local check resolved every local Markdown link and proposed card/plan
  reference, found seven unique document IDs, twelve unique RFC criterion
  definitions and exactly one closeout, and verified the schema's bytes
  against the canonical source. `python3 scripts/verify_documentation.py
  --root .` returned `DOCUMENTATION_OK`. The optional `jsonschema` Python
  library was unavailable, so no separate JSON-Schema-engine result is
  claimed. Validator provenance is the discovered
  `/Users/jaeger/.agents/skills/implement-rfc/examples/milestone-plans/validate.py`
  with its existing `scripts/milestone_plan_contract.py` module. The portable
  schema comes from that same skill's `schemas/milestone-plan.schema.json`.
  Validation is draft validation, not implementation acceptance; offline
  structure checking does not resolve live selectors or certify semantic
  coverage. Final post-review results are recorded below.
- **Design readiness:** the requested material choices above are answered;
  native runtime capability and public-path qualification remain work in the
  proposed plan. The requested Astra max review and document revisions are
  complete; its refinements remain proposals, not newly inferred owner answers.
- **Confirmation:** the owner's quoted statements confirm only their named
  choices. No blanket confirmation of working draft v1 has been requested or
  inferred, and none is a new implementation gate.
- **Execution eligibility:** not evaluated by authoring. Live IDs, mandates,
  mappings, prerequisites and writer reconciliation belong to a later
  authorized implement-rfc session. The present stop boundary remains no
  implementation.

Post-review draft checks, 2026-11-08:

- `python3 /Users/jaeger/.agents/skills/implement-rfc/examples/milestone-plans/validate.py /Users/jaeger/Documents/GitHub/BetterPullReview/docs/rfcs/assets/native-agent-sessions-v1/milestone-plan.json`
  exited 0: `Structurally valid; references and semantic coverage unchecked.`
  Its `--render` stdout replaced `MILESTONES.md`; a fresh render matched exactly.
- The pack-local check passed: 63 local Markdown links resolve, seven unique
  document IDs match the plan scope, all card references resolve, twelve
  criterion definitions are unique, and exactly one closeout exists. All
  nine amendment source hashes match; the portable schema matches its
  canonical source byte for byte. The optional `jsonschema` engine remains
  unavailable.
- `python3 scripts/verify_documentation.py --root /Users/jaeger/Documents/GitHub/BetterPullReview`
  exited 0 with `DOCUMENTATION_OK`. These are authoring checks only; no
  product/runtime tests or installed-agent inference were performed.
- The parent independently repeated the final canonical validation with a
  pack-local reference catalog, local-link/ID/criterion/closeout checks,
  generated-view and schema comparisons, all nine source hashes and new-file
  whitespace checks. All passed; a separate documentation-gate run returned
  `DOCUMENTATION_OK`.

## Continuation

Authoring, the requested Astra max review and the final artifact checks are
complete. The next execution entrypoint, only when implementation is
authorized, is [IMPLEMENTATION_PROMPT.md](IMPLEMENTATION_PROMPT.md).
