# RFC-BPR-0012 Verification Plan and Ledger

RFC base: `2966ff8103dc442775425386f6a31f8ac1a391c6`

Status: **partially implemented; claims are verified row by row.** BPR-0496
allocated the card set on 2026-09-01, when RFC-BPR-0012 was the first RFC in
the pack with no implementation at all. Since then BPR-0497 through BPR-0502
have landed the core finding, promotion, and chat-scope semantics, the host's
finding decode and delivery, the read-only MCP context helper, and the Qt
side-chat panel; the rows those slices own read `Verified`, `Core and panel
verified`, or `Core half verified` below, each naming its evidence. The rows
still reading `Planned` name the card that will supply theirs: the remaining
Qt surface (BPR-0503) and the composition, hygiene, and closeout slices
(BPR-0504 through BPR-0507).

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
| Core and panel verified | Every AI action originates from an explicit visible user command. | Core legality, the side-chat panel, and the composed workbench | `core_agent_profile`, `core_agent_chat_scope`, `qt_side_chat_panel`, `application_agent_finding_composition_contract`, `architecture_boundaries` | Each of the seven entry points is legal only with its own explicit intent, and a sweep of refresh, navigation, comment arrival, and revision update produces zero invocations and zero process launches | **Core half** (BPR-0499, 2026-09-01): `core_agent_chat_scope` proves the seven entry points legal only with their own explicit `agent_action_intent` (not default-constructible, `explicit`), an eighth failing the count at compile time, and every observation producing no intent; the panel half is now direct too (BPR-0502, 2026-09-02): `qt_side_chat_panel` drives every declared observation — refresh, navigation, comment arrival, and revision update — through the shipped panel and asserts zero intents, zero chips, and no movement in any of its ten presented fields, while each of the four gestures emits exactly one typed intent, the send carrying a `core::agent_action_intent` that names its entry point. The composed half remains `Planned` — BPR-0504 |
| Planned | Comment workshop preserves the original draft and shows a reviewable proposed revision. | Qt review-draft editor and finding queue | `qt_review_draft_editor`, `qt_finding_queue_view` | Rejecting leaves the original draft byte-identical, and both texts are asserted on two separately presented values so a single overwritten buffer fails | `Planned` — BPR-0503 |
| Verified | Structured findings validate path/line/OID and cannot silently attach to a newer revision. | Core finding schema and the host decode path | `core_agent_finding`, `application_agent_finding_protocol`, `application_agent_invocation_host` | Valid, invalid, and stale anchors return three verdicts distinguishable by value, and a payload arriving after a head move is stale with its analyzed OIDs intact | **Verified** (BPR-0497 and BPR-0500): the core decides anchored, unanchored, and stale against the session's frozen binding; the host decodes a bounded payload once in the shared loop, validates every finding through that core function before re-entry (deleting the call fails by name), and delivers a payload arriving after an accepted head move as `head_moved` with the session's own binding and the analyzed OIDs intact. The composed population of the frozen snapshot is BPR-0504's |
| Core half verified | Converting a finding creates only a local draft until explicit publication. | Core promotion and the composed conversion course | `core_agent_finding_promotion`, `application_agent_finding_composition_contract` | One promotion yields exactly one command, no result type can carry a `review_publication_proposal`, and the conversion course records zero GitHub mutation calls | **Core half verified** (BPR-0498, 2026-09-01): `promote_agent_finding` returns exactly one `edit` draft command, one `reclassify` defect command, or one note payload, or a typed refusal; the result type fails to compile with a `review_publication_proposal` field; the suggested comment is carried byte for byte. The composed course remains `Planned` — BPR-0504 |
| Planned | Full-agent GitHub changes round-trip through normal synchronization without a private backchannel. | Composed end-to-end over the RFC-BPR-0008 fixture path | `application_agent_finding_composition_contract`, `application_review_round_trip_composition_contract`, `composed_application_hosts` | The resulting GitHub state arrives only through RFC-BPR-0008 synchronization; a second edge from the agent host would appear as a mutation call outside it | `Planned` — BPR-0504 |
| Core and panel verified | The side chat shows stale state after head movement and offers new-session or explicit old-revision continuation. | Core session state and the Qt side-chat panel | `core_agent_session`, `core_agent_chat_scope`, `qt_side_chat_panel` | After `head_moved` the session offers exactly two continuations, retargets neither, and the panel starts neither by itself | **Core half** (BPR-0499, 2026-09-01): `offer_agent_continuations` carries `observe_agent_session`'s verdict, offers a two-element `std::array` of continuations, and names the session's own binding as the only continuation target. **The panel half is verified** (BPR-0502, 2026-09-02): `qt_side_chat_panel` presents a stale projection as stale with the core's own `head_moved` name, offers exactly the two-element `agent_continuation` array core defined, and emits nothing while doing it — the intent count after presenting a stale session is zero, and the send action is disabled until the reviewer answers. Each continuation emits exactly one `choose_chat_continuation_intent` when and only when it is chosen, carrying `ContinueAgainst` — the session's own binding, asserted unequal to the moved head — because the projection type has no field the observed revision could occupy. Making either continuation start itself on presentation fails `presenting a stale session starts neither continuation`, both restored byte-exact by md5. Constructing the panel in the shipped window remains `Planned` — BPR-0504 |
| Verified | The MCP helper exposes only documented read-only operations and cannot access credentials, network, writes, or unrelated filesystem content. | The helper's stdio canary | `application_mcp_context_helper` | The advertised tool list is compared exactly against the source enumeration, and a sweep over every refusal — connect, listener bind, read above the snapshot root, symlink escape, and each write, draft, publish, and note name — refuses each while the parent holds planted secrets | **Verified** (BPR-0501, 2026-09-02): the shipped `tool_better_pull_review_mcp_context_helper` speaks newline-delimited JSON-RPC 2.0 over stdio — `initialize`, `tools/list`, `tools/call` — and advertises four read-only tools over one frozen snapshot pinned to one `agent_revision_binding`. The advertised list is compared against three independent sources: the `enum class mcp_tool` text read out of the header, the compiled `mcp_tools` array, and a list written in the canary, so a tool added to the first two still fails. The refusal sweep drives twenty-one entries through the real executable — a URL argument, a `//host:port` authority, a UNC authority, `net/connect`, `net/listen`, `../../../etc/passwd`, `/etc/passwd`, `../binding`, a real directory symlink out of the snapshot on both path-taking tools, ten write/draft/publish/note/network tool names, and a helper started with `--listen 127.0.0.1:8080` — and each is refused by its own exact typed cause with no result beside it. Nineteen scripted negative controls each removed one check, rebuilt, and made the canary fail; every restore was byte-exact by md5 and the restored tree builds and passes. The child's complete environment reduces to exactly `BPR_MCP_SNAPSHOT_ROOT` and `BPR_MCP_PINNED_BINDING` while the parent holds planted GitHub, Anthropic, and AWS secrets, and no secret value reaches either child channel. Head movement with the pin unchanged makes every call `revision_binding_stale` and re-reads nothing. **No socket surface survives**: `nm -u` on the shipped binary shows no socket, connect, bind, listen, accept, or resolver import among its 84 undefined symbols, so `LOOPBACK_PORT_COUNT` stays 0 and `test_network_isolation_manifest` is not required. Constructing and supervising the helper inside the shipped application remains BPR-0504's |
| Core half verified | Private notes and drafts are never included without explicit selection. | Core session opt-in, the context supplement, and the composed course | `core_agent_context_supplement`, `core_agent_chat_scope`, `application_agent_invocation_composition_contract` | A session without the per-session opt-in carries zero notes and zero drafts even when both exist for the review, and the composed course keeps its existing zero note and draft counts | **Core half verified** (BPR-0499, 2026-09-01): `agent_private_context_optin` is keyed by session id with notes and drafts separately default-off, and the two-argument `build_agent_supplement` carries zero of both without it; the host now builds through the opt-in overload keyed to the session (BPR-0500, with a source-order assertion that the ungated overload is unreachable from the host); the composed course remains `Planned` — BPR-0504 |
| Planned | Finding/chat/promotion decisions are core-tested without agent/Qt dependencies; host protocol tests and Qt workflow tests prove all results re-enter through typed revision-bound events. | All three regions | `ctest --preset core-only -R '^core_agent_'`, `application_agent_finding_protocol`, `qt_side_chat_panel`, `qt_finding_queue_view` | The core suites link `BetterPullReview::Core` only, the delivered event's binding is compared field for field against the session's, and deleting the validation call at the decode-to-core seam fails an invalid-anchor case | `Planned` — BPR-0503. The side-chat half of the Qt evidence landed with BPR-0502: `qt_side_chat_panel` links `BetterPullReview::AdapterQt` only, runs under `bpr_add_network_isolated_test`, and proves the panel moves only when `present` is called with an accepted projection. The finding queue and workshop are BPR-0503's |

## Placement decision (BPR-0501)

BPR-0501 asked the implementing slice to confirm or overturn `src/adapters/mcp/`
as the helper's home, with the reason either way. **Confirmed.**

The reason the card offered survives contact with the code, and the build file
makes it checkable rather than asserted: `src/adapters/agent` is a narrow launch
port for a program BetterPullReview does not implement and only starts, and it
links `BetterPullReview::Application` and `Qt6::Core` to do it. This helper is
the opposite direction of the same boundary — a program BetterPullReview
implements and ships, serving its own frozen projection outward to somebody
else's agent. Housing a server inside a launch port would make one directory
answer for both directions, and the target that starts an agent would link the
target that answers one.

Two consequences are load-bearing rather than tidy. `better_pull_review_adapter_mcp`
links `BetterPullReview::Core` and nothing else — not the application host, not
Qt, not the GitHub, Git, or SQLite adapters — so the claim that the helper reaches
no credential and no network is a link line a reviewer reads in one line. And
`src/adapters/agent/CMakeLists.txt` returns early without Qt, so a helper placed
there would not exist in the `core-only` configuration at all; here it builds and
is proven in both.

The `tool_` prefix is deliberate: it puts the helper in the manifest
`live_mutation_isolation` walks, so every build also proves the shipped helper
links no live GitHub mutation. `app_` would have exempted it.

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
