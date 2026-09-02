# RFC-BPR-0012 Verification Plan and Ledger

RFC base: `2966ff8103dc442775425386f6a31f8ac1a391c6`

Status: **planning. No claim in this ledger is verified.** BPR-0496 allocated
the card set; no source has moved. RFC-BPR-0012 is the first RFC in the pack
with no implementation at all — before BPR-0496 no card linked it — so every
row of the claim matrix below reads `Planned` and names the card that will
supply its evidence.

RFC-BPR-0012 stays `Proposed`. No card here makes a closure claim, and closure
remains ordered behind RFC-BPR-0008 and RFC-BPR-0011.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0008 and RFC-BPR-0011. Both are substantially
implemented on available targets: RFC-BPR-0008's review-mutation host,
coordinator, and round-trip composition contract are composed and exercised,
and RFC-BPR-0011's profile policy, revision binding, context supplement,
portable protocol loop, workspace, launch panel, and composed invocation path
are composed and exercised, with the Windows lifecycle green target-native on
2026-08-26/27.

The RFC index records the product owner's exception: implementation, tests,
documentation, security review, and simplification for RFC-BPR-0005 through
RFC-BPR-0014 may proceed while earlier RFCs remain `Proposed`, and each such
RFC ends its available-target work at a closeout card in `10-backlog/`. This
RFC's closeout card is BPR-0507.

## What this RFC is, in one sentence

Give the reviewer explicit AI operations — findings, a comment workshop, a
revision-aware side chat, a full agent session, and a read-only MCP context
helper — without turning the product into an autonomous reviewer.

## The rule everything else serves

**AI output is a proposal until a person promotes it, and nothing it says can
publish, mutate, or move the UI on its own.**

The convenience that would erode this is obvious and would look helpful: the
agent returned a good comment, so post it. The RFC says the opposite three
times over in its non-goals — no automatic invocation, no automatic
publication, no treating confidence as proof — and the product contract
forbids automatically publishing AI output.

Planned structural expression: a validated finding converts into exactly one
`review_draft_command`, one `defect_command`, or one private-note payload, and
the promotion result type has no field, overload, or branch that could produce
a `review_publication_proposal`. The host has no edge from agent output to the
RFC-BPR-0008 mutation coordinator.

## The second rule

**An anchor is a semantic identity or it is nothing.**

A finding's path, line range, and OIDs validate against RFC-BPR-0004's
`source_line_ref`, `source_boundary_ref`, `line_span`, `alignment_segment`,
`change_group`, and `move_relation` identities and against the session's frozen
`agent_revision_binding`. An anchor that does not validate is shown as
unanchored; one whose revision moved is shown as stale.

Planned structural expression: no coordinate, y-position, viewport offset, or
ribbon path is representable in the finding or anchor type, and the guard that
says so reads the type rather than a construction site.

## Architecture allocation

Restating the RFC's own allocation section, with the evidence each region will
carry:

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Finding schema validation outcomes, path/OID/revision anchoring policy, chat scope, stale-session state, explicit conversion of a finding into a local draft, and the prohibition on automatic publication | Qt-free and provider-free suites under `core-only`, linking `BetterPullReview::Core` only, including a type-reading guard that no finding or anchor field can hold a coordinate |
| Application host | Codex and Claude protocol sessions, MCP stdio helper lifecycle, context retrieval, structured-output parsing limits, cancellation, and typed completion events; GitHub publication delegated to the RFC-BPR-0008 mutation path | Fixture-process tests on the shared portable loop, a helper canary that enumerates tools and attempts every forbidden access, and field-for-field comparison of the delivered event's revision binding |
| Qt adapter | Side chat, context chips, finding queue, before/after comment workshop, approvals, and explicit promote, dismiss, and publish gestures | Adapter suites asserting every action is an intent, that no widget holds a process, launcher, credential, endpoint, or store, and that navigation adds no context |

Neither agent output nor MCP input mutates core or Qt state directly. All
results re-enter through typed events carrying the session's exact binding.

## Claim matrix

One row per acceptance criterion, quoted from the RFC. `State` is the row's
lifecycle; `Result` records the evidence and the cards that supply it. A row
moves off `Planned` only when its card lands with direct evidence.

| State | Claim | Direct surface | Command | What will prove it | Result |
|---|---|---|---|---|---|
| Planned | Every AI action originates from an explicit visible user command. | Core legality, the side-chat panel, and the composed workbench | `core_agent_profile`, `core_agent_chat_scope`, `qt_side_chat_panel`, `application_agent_finding_composition_contract`, `architecture_boundaries` | Each of the seven entry points is legal only with its own explicit intent, and a sweep of refresh, navigation, comment arrival, and revision update produces zero invocations and zero process launches | `Planned` — BPR-0499, BPR-0502, BPR-0504 |
| Planned | Comment workshop preserves the original draft and shows a reviewable proposed revision. | Qt review-draft editor and finding queue | `qt_review_draft_editor`, `qt_finding_queue_view` | Rejecting leaves the original draft byte-identical, and both texts are asserted on two separately presented values so a single overwritten buffer fails | `Planned` — BPR-0503 |
| Core half verified | Structured findings validate path/line/OID and cannot silently attach to a newer revision. | Core finding schema and the host decode path | `core_agent_finding`, `application_agent_finding_protocol` | Valid, invalid, and stale anchors return three verdicts distinguishable by value, and a payload arriving after a head move is stale with its analyzed OIDs intact | **Core half verified** (BPR-0497, 2026-09-01): `core_agent_finding` proves anchored, unanchored (`unknown_path`, `span_past_end`, `malformed`), and stale by value, with staleness decided against the session's frozen binding before any line count is read and every verdict carrying the analyzed OIDs; the host decode half remains `Planned` — BPR-0500 |
| Planned | Converting a finding creates only a local draft until explicit publication. | Core promotion and the composed conversion course | `core_agent_finding` (promotion cases), `application_agent_finding_composition_contract` | One promotion yields exactly one command, no result type can carry a `review_publication_proposal`, and the conversion course records zero GitHub mutation calls | `Planned` — BPR-0498, BPR-0504 |
| Planned | Full-agent GitHub changes round-trip through normal synchronization without a private backchannel. | Composed end-to-end over the RFC-BPR-0008 fixture path | `application_agent_finding_composition_contract`, `application_review_round_trip_composition_contract`, `composed_application_hosts` | The resulting GitHub state arrives only through RFC-BPR-0008 synchronization; a second edge from the agent host would appear as a mutation call outside it | `Planned` — BPR-0504 |
| Planned | The side chat shows stale state after head movement and offers new-session or explicit old-revision continuation. | Core session state and the Qt side-chat panel | `core_agent_session`, `core_agent_chat_scope`, `qt_side_chat_panel` | After `head_moved` the session offers exactly two continuations, retargets neither, and the panel starts neither by itself | `Planned` — BPR-0499, BPR-0502 |
| Planned | The MCP helper exposes only documented read-only operations and cannot access credentials, network, writes, or unrelated filesystem content. | The helper's stdio canary | `application_mcp_context_helper`, and `test_network_isolation_manifest` if any socket surface exists | The advertised tool list is compared exactly against the source enumeration, and a sweep over every refusal — connect, listener bind, read above the snapshot root, symlink escape, and each write, draft, publish, and note name — refuses each while the parent holds planted secrets | `Planned` — BPR-0501 |
| Planned | Private notes and drafts are never included without explicit selection. | Core session opt-in, the context supplement, and the composed course | `core_agent_context_supplement`, `core_agent_chat_scope`, `application_agent_invocation_composition_contract` | A session without the per-session opt-in carries zero notes and zero drafts even when both exist for the review, and the composed course keeps its existing zero note and draft counts | `Planned` — BPR-0499, BPR-0504 |
| Planned | Finding/chat/promotion decisions are core-tested without agent/Qt dependencies; host protocol tests and Qt workflow tests prove all results re-enter through typed revision-bound events. | All three regions | `ctest --preset core-only -R '^core_agent_'`, `application_agent_finding_protocol`, `qt_side_chat_panel`, `qt_finding_queue_view` | The core suites link `BetterPullReview::Core` only, the delivered event's binding is compared field for field against the session's, and deleting the validation call at the decode-to-core seam fails an invalid-anchor case | `Planned` — BPR-0497 through BPR-0503 |

## Cards

| Card | Slice |
|---|---|
| BPR-0496 | Plan RFC-BPR-0012 verification and allocate its cards |
| BPR-0497 | Core: structured finding schema and anchor validation |
| BPR-0498 | Core: finding promotion into a local draft or defect, never a publication |
| BPR-0499 | Core: explicit AI actions, side-chat scope, and stale continuation |
| BPR-0500 | Host: structured-finding decode, limits, and typed revision-bound re-entry |
| BPR-0501 | Host: read-only MCP context helper over stdio |
| BPR-0502 | Qt: side-chat drawer with context chips and stale state |
| BPR-0503 | Qt: finding queue and before/after comment workshop |
| BPR-0504 | Composition: wire findings, chat, workshop, and MCP into the shipped workbench |
| BPR-0505 | Documentation: extend the AI boundary, UX, fixture catalog, and matrix |
| BPR-0506 | Simplification and closeout preparation |
| BPR-0507 | Verify and close RFC-BPR-0012 |

## Deferred gates

None. That was checked against the RFC text rather than assumed.

No acceptance criterion needs target-native Windows execution: the criteria
name no platform, and the decode path this RFC extends is the one portable
loop BPR-0334 put behind both process transports. No criterion needs a
credential — the RFC's non-goals forbid granting the MCP helper GitHub
credentials, and the helper's own criterion is that it cannot reach one. No
criterion needs a real agent installation: findings, chat, and the helper are
proven with fixture processes in the pattern
`tests/fixtures/agent/agent_canary_main.cpp` established. No criterion needs a
network at runtime; every suite registers through
`bpr_add_network_isolated_test`.

No criterion needs a live GitHub mutation. The full-agent round trip is proven
with the RFC-BPR-0008 fixture synchronization path already exercised by
`application_review_round_trip_composition_contract`, not against a real pull
request. A backchannel is detectable there — it would show as a mutation call
outside the synchronization path — which is what the criterion asks for.

A smoke run against an installed Codex or Claude harness stays optional extra
validation, exactly as it is for RFC-BPR-0011. Carding it would put a
permanent open item on the board.

One decision is left explicit rather than pre-empted: the comment workshop's
before/after surface, since no Markdown renderer exists in the tree. BPR-0503
records plain text as the chosen surface and names what a renderer would cost.

## Documentation ledger

BPR-0505 fills this section. Nothing is recorded yet. It must identify
functional-core, application-host, and Qt-presentation-adapter ownership; the
typed intents, events, observations, requests, and deltas; authority;
concurrency and generation; and any evidence-backed no-impact region — and it
extends `docs/architecture/ai-agent-boundary.md` rather than adding a second
document beside it.

## Simplification

BPR-0506 fills this section. Nothing is recorded yet.

## Closeout preparation

RFC-BPR-0012 stays **`Proposed`**. No closure claim is made anywhere in this
ledger, and closure order is unchanged: it is ordered behind RFC-BPR-0008 and
RFC-BPR-0011. RFC-BPR-0008's closeout (BPR-0242) waits on its attended
live-GitHub cards, and RFC-BPR-0011's (BPR-0245) waits on RFC-BPR-0008
through RFC-BPR-0010 closing in order; RFC-BPR-0002 through RFC-BPR-0005 are
already `Auto-Verified`. The available-target work ends at BPR-0507 in
`10-backlog/`, as the RFC index's product-owner exception requires.

### Not delivered

Nothing is delivered yet. BPR-0506 replaces this section with the named list a
reader would otherwise have to infer from silence.
