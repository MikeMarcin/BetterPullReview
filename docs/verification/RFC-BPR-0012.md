# RFC-BPR-0012 Verification Plan and Ledger

RFC base: `2966ff8103dc442775425386f6a31f8ac1a391c6`

Status: **partially implemented; claims are verified row by row.** BPR-0496
allocated the card set on 2026-09-01, when RFC-BPR-0012 was the first RFC in
the pack with no implementation at all. Since then BPR-0497 through BPR-0503
have landed the core finding, promotion, and chat-scope semantics, the host's
finding decode and delivery, the read-only MCP context helper, the Qt
side-chat panel, and the Qt finding queue and comment workshop, BPR-0504
has composed every one of them into the shipped workbench, BPR-0513 has placed
both panels in the conversations drawer, and BPR-0514 has composed the chat
*session* — one send on the shipped panel now reaches one agent turn on the
existing launch path and one delivered reply, which BPR-0517 has made a
question rather than a session start by carrying the reviewer's typed message
into the agent's own input frame exactly once. BPR-0505 then wrote the
documentation and BPR-0506 made the behaviour-preserving pass and filled
**Simplification** and **Closeout preparation** below.

No row reads `Planned`. Every one of the nine reads `Verified` and names the
card and the command that settles it. That is a statement about labels, not a
statement that the RFC is finished: **Closeout preparation** names what is not
delivered, and one row records a command that does not currently pass for a
reason belonging to another card.

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
| Verified | Every AI action originates from an explicit visible user command. | Core legality, the side-chat panel, and the composed workbench | `core_agent_profile`, `core_agent_chat_scope`, `qt_side_chat_panel`, `application_agent_finding_composition_contract`, `architecture_boundaries` | Each of the seven entry points is legal only with its own explicit intent, and a sweep of refresh, navigation, comment arrival, and revision update produces zero invocations and zero process launches | **Core half** (BPR-0499, 2026-09-01): `core_agent_chat_scope` proves the seven entry points legal only with their own explicit `agent_action_intent` (not default-constructible, `explicit`), an eighth failing the count at compile time, and every observation producing no intent; the panel half is now direct too (BPR-0502, 2026-09-02): `qt_side_chat_panel` drives every declared observation — refresh, navigation, comment arrival, and revision update — through the shipped panel and asserts zero intents, zero chips, and no movement in any of its ten presented fields, while each of the four gestures emits exactly one typed intent, the send carrying a `core::agent_action_intent` that names its entry point. **The composed half is verified** (BPR-0504, 2026-09-02): `application_agent_finding_composition_contract` drives the shipped window — connect, open, select a file, navigate to a change, mark it read, choose a profile, start — and exactly one agent process runs, the one the start gesture asked for. Nothing in the composed RFC-BPR-0012 surface starts anything by itself: the helper supervisor refuses every path but an explicit `start`, and each of the five construction-cut courses sees the same single gesture with its own effect at zero. **Visible is now literal** (BPR-0513, 2026-09-02): the side chat and the finding queue are laid into the conversations drawer beside the agent launcher, so the command a reviewer gives is one they can see the surface for. `qt_composed_github_scale_100`, `_125`, and `_200` find each panel by the accessible name it gives itself, ask the drawer's own column whether it manages the widget found, and save a crop of the drawer at each scale. Deleting either `conversationLayout->addWidget(` line fails the case by name — *the side chat is laid into the drawer's own column, not merely parented to it* and its finding-queue twin — both restored byte-exact by md5 **The turn itself is composed** (BPR-0514, 2026-09-02): `application_agent_chat_composition_contract` types one message into the shipped side-chat composer and triggers its Send action; exactly one agent process runs, exactly one validated `agent_finding_delivery` comes back, and `review_mutation_evidence` is 0/0/0 throughout. It is the launch path that already existed: the composition source carries exactly one `AgentHost->dispatch(` call site, reached from the launch panel's start gesture and from the chat host's turn dispatcher through one `dispatch_agent_invocation` declaration and two call sites, so a chat turn cannot have grown its own workspace or its own frozen snapshot. The reply arrives at the same seam the finding host reads, so nothing a chat receives bypasses BPR-0500's delivery bounds. `application_agent_chat_construction_cut_contract` deletes the chat host's construction: the same send gesture is still made and still counted, its turn count is zero, no process starts, the reviewer is told *side chat refused: no composed chat session host*, and the positive marker is false **The command carries what was typed** (BPR-0517, 2026-09-02): a send is now a question the agent can read rather than a session start with a scope. `core::agent_context_supplement` gained one bounded `Message`, admitted by `build_agent_supplement` only when the request's `core::agent_chat_message` names the session being built for and is within `maximum_chat_message_bytes`; an out-of-session or oversized message refuses the whole supplement, as an oversize note already did. `encode_agent_supplement` writes it at one documented position - the `chat_message` field, immediately after `pull_request_url` - which is the whole of the supplement's wire form, so every protocol's frame carries it exactly once: after the `BPR-CONTEXT/2` header for Codex JSONL and the external terminal, inside the escaped user text for Claude stream JSON, and on App Server's deferred context, whose first frame is a handshake. `frame_agent_input` appends nothing beside the payload and the decode side is unchanged. `application_agent_chat_composition_contract` proves the count rather than the presence: the fixture agent reads its own stdin, counts the typed message in the whole received frame, and echoes `BPR_CHAT_ECHO_0517 count=1 text=...` back through the finding it returns, which the course asserts on the queued claim. Deleting the placement - the one `append_field(result, "chat_message", ...)` line - fails that course with `count=0 text=` while every other conjunct stays green, restored byte-exact by md5 `1c672b7705bc94de798ece4f6add58e9`. `application_agent_finding_composition_contract` asserts the other half: a launch-panel start is not a chat turn and its frame carries `count=0`, so the chat host's send intent is the only thing that fills the field |
| Verified | Comment workshop preserves the original draft and shows a reviewable proposed revision. | Qt review-draft editor and finding queue | `ctest --preset dev -R '^(qt_finding_queue_view\|qt_review_draft_editor)$'` | Rejecting leaves the original draft byte-identical, and both texts are asserted on two separately presented values so a single overwritten buffer fails | **Verified** (BPR-0503, 2026-09-02): the workshop lives inside `review_draft_editor`, which holds a `comment_workshop_proposal` carrying the reviewer's `Original` and the agent's `Proposed` as two members and shows them on two `QPlainTextEdit` surfaces at once; the suite asserts each against its own source and asserts the two widgets are different objects. Accepting emits exactly one `review_draft_command` — an `edit` of the draft the proposal names, carrying the proposed text byte for byte — and writes nothing: the reviewer's body and the accepted draft projection are still byte-identical afterwards, and the receipt path is untouched. Rejecting emits nothing at all and leaves the body byte for byte what it was, tab and trailing space included. Six scripted negative controls each neutered one of these and failed by name: accepting twice fails `accepting a revision emits exactly one review draft command`, accepting zero times fails the same line, rejecting written back into the body fails `rejecting a revision asks for nothing at all`, pointing both surfaces at one buffer fails `the original, the proposal, and the editable body are three separately announced surfaces`, deleting the proposal's presentation fails `and the proposed revision is presented as a second, separate value at the same time`, and a `void*` store member added to the editor fails to compile at `decomposes into 20 elements, but only 19 names were provided`. Every restore was byte-exact by md5. **The workshop opens where the promotion lands** (BPR-0513, 2026-09-02): the finding queue's six actions are offered as buttons in the same drawer column as the draft editor whose workshop a promotion fills, and the composed-window case asserts both workshop surfaces are that drawer's descendants. Clicking the placed *Convert to local draft comment* button emits exactly one `finding_action_intent` and promotes nothing itself |
| Verified | Structured findings validate path/line/OID and cannot silently attach to a newer revision. | Core finding schema and the host decode path | `core_agent_finding`, `application_agent_finding_protocol`, `application_agent_invocation_host` | Valid, invalid, and stale anchors return three verdicts distinguishable by value, and a payload arriving after a head move is stale with its analyzed OIDs intact | **Verified** (BPR-0497 and BPR-0500): the core decides anchored, unanchored, and stale against the session's frozen binding; the host decodes a bounded payload once in the shared loop, validates every finding through that core function before re-entry (deleting the call fails by name), and delivers a payload arriving after an accepted head move as `head_moved` with the session's own binding and the analyzed OIDs intact. **The composed population of the frozen snapshot is delivered** (BPR-0504, 2026-09-02): `dispatch_agent_intent` reads the alignment the reviewer is looking at once, states it as `agent_invocation_request::FrozenFiles`, and writes the same values to the helper's snapshot root, so one revision has one answer rather than two that could disagree. The composed course's fixture agent anchors its finding at the exact `source_line_ref` the context disclosed to it, and the shipped queue shows `anchored (none)` |
| Verified | Converting a finding creates only a local draft until explicit publication. | Core promotion and the composed conversion course | `core_agent_finding_promotion`, `application_agent_finding_composition_contract` | One promotion yields exactly one command, no result type can carry a `review_publication_proposal`, and the conversion course records zero GitHub mutation calls | **Core half verified** (BPR-0498, 2026-09-01): `promote_agent_finding` returns exactly one `edit` draft command, one `reclassify` defect command, or one note payload, or a typed refusal; the result type fails to compile with a `review_publication_proposal` field; the suggested comment is carried byte for byte. **The composed course is verified** (BPR-0504, 2026-09-02): the `convert_to_draft_comment` gesture on the composed queue produces exactly one `review_draft_command` and changes no draft — the reviewer's body is still byte-identical afterwards, tab and trailing space included — because the conversion is presented as a before/after proposal on the shipped `review_draft_editor` first. Accepting it there is the one gesture that writes, and the local draft body then equals the agent's suggested comment byte for byte. `review_mutation_evidence` records zero submissions, zero target observations, and zero stored comments across the whole course |
| Verified | Full-agent GitHub changes round-trip through normal synchronization without a private backchannel. | Composed end-to-end over the RFC-BPR-0008 fixture path | `application_agent_finding_composition_contract`, `application_agent_finding_full_agent_contract`, `application_review_round_trip_composition_contract`, `composed_application_hosts` | The resulting GitHub state arrives only through RFC-BPR-0008 synchronization; a second edge from the agent host would appear as a mutation call outside it | **Verified** (BPR-0504, 2026-09-02): read two ways, because either alone would be weaker. A complete full-agent launch — real child process, its own workspace, a returned structured payload, a promotion, and an acceptance — records **zero** GitHub mutation calls of every kind, so nothing left the application while the agent was working or after it finished. And the composition source names `review_mutation_coordinator` zero times, carries exactly one `ReviewMutationHost->publish(` call site — the reviewer's own `begin_publish` branch — one `->submit(`, no `.submit(`, and no `observe_target` at all, so there is no second edge for a backchannel to travel along. The positive direction, that a publication does round-trip through that one path, is `application_review_round_trip_composition_contract`, which pins this same file by digest and was repinned and re-run green in this slice |
| Verified | The side chat shows stale state after head movement and offers new-session or explicit old-revision continuation. | Core session state and the Qt side-chat panel | `core_agent_session`, `core_agent_chat_scope`, `qt_side_chat_panel` | After `head_moved` the session offers exactly two continuations, retargets neither, and the panel starts neither by itself | **Core half** (BPR-0499, 2026-09-01): `offer_agent_continuations` carries `observe_agent_session`'s verdict, offers a two-element `std::array` of continuations, and names the session's own binding as the only continuation target. **The panel half is verified** (BPR-0502, 2026-09-02): `qt_side_chat_panel` presents a stale projection as stale with the core's own `head_moved` name, offers exactly the two-element `agent_continuation` array core defined, and emits nothing while doing it — the intent count after presenting a stale session is zero, and the send action is disabled until the reviewer answers. Each continuation emits exactly one `choose_chat_continuation_intent` when and only when it is chosen, carrying `ContinueAgainst` — the session's own binding, asserted unequal to the moved head — because the projection type has no field the observed revision could occupy. Making either continuation start itself on presentation fails `presenting a stale session starts neither continuation`, both restored byte-exact by md5. **Constructing the panel in the shipped application is verified** (BPR-0504, 2026-09-02): the panel is composed beside the agent host under the same fixture and production split, and `refresh_agent_authority` presents it with the session's own binding, so its head-OID field carries the accepted head rather than anything the panel computed. Cutting its construction leaves the start gesture intact and takes presentations and intents to zero, and `composed_application_hosts` fails by name when the construction is deleted. **The panel is on screen** (BPR-0513, 2026-09-02): placement moved into `workbench_window`, which composes both panels in its conversations drawer and hands them out; the application now adopts them and sets the dispatch rather than constructing a second pair nobody could see. The panel's member aggregate is untouched, `qt_side_chat_panel`'s guard still compiles unchanged, and collapsing the drawer hides both placed surfaces with it **The composed session is verified** (BPR-0514, 2026-09-02): `application::agent_chat_session_host` owns the chat's session in the shipped application and asks the core for every answer it gives — `offer_agent_continuations` first, then `decide_agent_action`, `next_agent_chat_turn`, and `admit_private_context`. `application_agent_chat_stale_contract` moves the head under a live session and the session is not retargeted: the projection reports `head_moved` with `Required` true, the panel disables Send and offers exactly the core's two continuations, and a send delivered on the panel's own composed dispatch edge is refused `session_stale (head_moved)` and named on the shipped agent panel. Choosing `start_new_session` then starts exactly one session — the continuation and started counts each rise by one, the adopted count does not, the new session is bound to the revision now under review while the old one keeps the revision it was asked about, and no process starts. `composed_application_hosts` fails by name — *required exact demangled anchor is absent: agent chat session application host constructor* — when the composed object is deleted, restored byte-exact by md5 **Both continuations are now driven** (BPR-0517, 2026-09-02): `application_agent_chat_continued_contract` takes the other one. From the same stale projection - the head moved by the shipped refresh gesture against the seeded recording, so the new revision is a real one - choosing `continue_against_old_revision` raises the continuation count by one and starts and adopts nothing; the chat keeps the session identity and the binding it was asked about, which is asserted unequal to the revision now under review; `Required` clears while the core's `head_moved` verdict stays; and no process starts. The reviewer then points at something again and asks a second question, and exactly one further turn is dispatched under the session they kept: the turn handed to the launch path carries that session's own id and old-revision binding and the second message's exact bytes, a second process runs, a second validated delivery comes back, and GitHub mutation stays 0/0/0 throughout **The context now follows the session** (BPR-0521, 2026-09-02): the same course asserts, immediately after the second send and before the run, that one revision is said three times — the binding the MCP helper is pinned to, the binding record the frozen snapshot carries on disk, and the kept session's own binding are byte-equal in `adapters::mcp::encode_revision_binding` spelling, and are not the head the reviewer moved to — and that the freeze count rose by one, so the record is that turn's rather than the first turn's. `dispatch_agent_invocation` composes the supplement, the pin, and the snapshot from the session's binding when the chat's session names a revision the epoch host has moved past; `agent_invocation_host` runs that turn under the continued session instead of admitting one bound to the current head, so the frozen snapshot's `Binding` is the session's `Binding` by construction and the chat is no longer silently retargeted after the turn — `SessionsAdopted` does not move and `Session->Binding` is still the kept one. A caller that disagrees with itself is refused by name: `Context.Binding` must equal `ContinuedSession->Binding` or the host answers `agent_continuation_binding_mismatch` and runs nothing. Deleting the one composition line — `turnBinding = aTurnSession->Binding;` at `src/app/src/github_application.cpp:5173` — fails the course as `agent-chat-composition-failure:continued-old-revision-composition`, with the pinned and recorded bindings printed as the moved head, restored byte-exact by md5 `f2df57bb9bc597101d5cb8d196bb7ad8` |
| Verified | The MCP helper exposes only documented read-only operations and cannot access credentials, network, writes, or unrelated filesystem content. | The helper's stdio canary | `application_mcp_context_helper` | The advertised tool list is compared exactly against the source enumeration, and a sweep over every refusal — connect, listener bind, read above the snapshot root, symlink escape, and each write, draft, publish, and note name — refuses each while the parent holds planted secrets | **Verified** (BPR-0501, 2026-09-02): the shipped `tool_better_pull_review_mcp_context_helper` speaks newline-delimited JSON-RPC 2.0 over stdio — `initialize`, `tools/list`, `tools/call` — and advertises four read-only tools over one frozen snapshot pinned to one `agent_revision_binding`. The advertised list is compared against three independent sources: the `enum class mcp_tool` text read out of the header, the compiled `mcp_tools` array, and a list written in the canary, so a tool added to the first two still fails. The refusal sweep drives twenty-one entries through the real executable — a URL argument, a `//host:port` authority, a UNC authority, `net/connect`, `net/listen`, `../../../etc/passwd`, `/etc/passwd`, `../binding`, a real directory symlink out of the snapshot on both path-taking tools, ten write/draft/publish/note/network tool names, and a helper started with `--listen 127.0.0.1:8080` — and each is refused by its own exact typed cause with no result beside it. Nineteen scripted negative controls each removed one check, rebuilt, and made the canary fail; every restore was byte-exact by md5 and the restored tree builds and passes. The child's complete environment reduces to exactly `BPR_MCP_SNAPSHOT_ROOT` and `BPR_MCP_PINNED_BINDING` while the parent holds planted GitHub, Anthropic, and AWS secrets, and no secret value reaches either child channel. Head movement with the pin unchanged makes every call `revision_binding_stale` and re-reads nothing. **No socket surface survives**: `nm -u` on the shipped binary shows no socket, connect, bind, listen, accept, or resolver import among its 84 undefined symbols, so `LOOPBACK_PORT_COUNT` stays 0 and `test_network_isolation_manifest` is not required. **Constructing and supervising the helper inside the shipped application is verified** (BPR-0504, 2026-09-02): `application::mcp_context_helper_supervisor` is composed beside the agent host with its own process launcher — a shared one would let one cancel stop whichever child the reviewer did not mean — and is pinned, on the reviewer's own launch gesture, to a snapshot root the application froze from the same values the finding validator uses. The composed course starts the shipped `tool_better_pull_review_mcp_context_helper` and reads back all four read-only tools. The launch is asserted whole rather than sampled: no argument vector at all, so `--listen` is not a flag this side could pass; a working directory that is the pinned snapshot root and names no URI scheme and no network or UNC authority; and an environment of exactly the two names `adapters::mcp::mcp_environment_allowlist` declares, compared element for element against the supervisor's own `environment_names()` so a rename fails rather than diverges. Cutting the construction leaves the gesture intact, writes no snapshot at all, and refuses the start as `no_helper` |
| Verified | Private notes and drafts are never included without explicit selection. | Core session opt-in, the context supplement, and the composed course | `core_agent_context_supplement`, `core_agent_chat_scope`, `application_agent_invocation_composition_contract` | A session without the per-session opt-in carries zero notes and zero drafts even when both exist for the review, and the composed course keeps its existing zero note and draft counts | **Core half verified** (BPR-0499, 2026-09-01): `agent_private_context_optin` is keyed by session id with notes and drafts separately default-off, and the two-argument `build_agent_supplement` carries zero of both without it; the host now builds through the opt-in overload keyed to the session (BPR-0500, with a source-order assertion that the ungated overload is unreachable from the host); **The composed course is verified** (BPR-0504, 2026-09-02; read again and settled by BPR-0506, 2026-09-02): `application_agent_invocation_composition_contract` proves this claim by *selection*, not by absence. The seeded review holds two private notes and one draft; the course selects note 246 and the current draft, then compares the child's whole decoded stdin field for field against an expected map in which `note.count` and `draft.count` are each exactly `1` and `note.0.text` and `draft.0.text` are the selected bytes. The unselected note — whose text carries `BPR_UNSELECTED_NOTE_0250`, `viewport`, `coordinate`, `qt_index`, and `BPR_CREDENTIAL_CANARY_0250` — is asserted absent from that stdin, and `contains_forbidden_context` sweeps every field key and every field value for those canaries plus `BPR_UNSELECTED_DRAFT_0247`. The **What will prove it** cell says the composed course keeps *zero* note and draft counts. It does not, and it should not: a course that carried zero of everything would pass a zero-count assertion without ever showing that explicit selection is the thing deciding. The plan's wording is left standing so it can still be compared against the outcome. **This row's command does not pass at HEAD** (BPR-0506, 2026-09-02) for a reason that is not this claim's: BPR-0517 added the `chat_message` field to `encode_agent_supplement` and did not add it to this course's `expected_context_fields` builder, so the exact field-map comparison sees 162 actual fields against 161 expected and the course exits non-zero with every note-and-draft assertion above it satisfied. That is BPR-0517's regression, carried here rather than repaired here, and it is named again under **Closeout preparation**. **The message is not a way around the opt-in** (BPR-0517, 2026-09-02): `core_agent_context_supplement` adds the case. A supplement built with a typed message and no opt-in carries the question and zero notes and zero drafts; a message keyed to another session refuses the whole build under both the plain and the gated overload, so an opt-in this session did make cannot admit a question it did not type; the type is exactly a session identity and the words, asserted by size, so there is no field on it a note, a draft, or a selection could occupy; a message at `maximum_chat_message_bytes` is admitted and the byte past it refuses the build whole rather than truncating it |
| Verified | Finding/chat/promotion decisions are core-tested without agent/Qt dependencies; host protocol tests and Qt workflow tests prove all results re-enter through typed revision-bound events. | All three regions | `ctest --preset core-only -R '^core_agent_'`, `application_agent_finding_protocol`, `qt_side_chat_panel`, `qt_finding_queue_view` | The core suites link `BetterPullReview::Core` only, the delivered event's binding is compared field for field against the session's, and deleting the validation call at the decode-to-core seam fails an invalid-anchor case | **The Qt half is verified** (BPR-0502 and BPR-0503, 2026-09-02): both Qt suites link `BetterPullReview::AdapterQt` only and run under `bpr_add_network_isolated_test`. `qt_finding_queue_view` presents an `application::agent_finding_delivery` — the typed, revision-bound value BPR-0500 delivers, verdict included — and offers exactly the six actions `docs/ux/ai-experience.md` names, each emitting one `finding_action_intent` carrying the session, the ordinal, the finding, and the verdict. A table sweeps all six against `finding_queue_action_count`, so a seventh action declared without a case fails at compile time on `expression evaluates to '6 == 7'`; dropping one action's intent fails `add_to_private_notes must emit exactly one intent, no more and no fewer`. Ten disclosed fields are swept and each must move alone, so freezing the presented confidence or anchor state fails `the confidence field did not move` and `the anchor state field did not move`. Neither widget holds a provider, a network address, a store, or a process: both member sets are one aggregate bound by name, and a `void*` store member added to either fails to compile. The core-only link line and the host decode evidence this row also names are BPR-0497's and BPR-0500's, already recorded above and not re-run here. **The composed workbench is verified** (BPR-0504, 2026-09-02): the host's typed `deliver_findings` is the one new edge out of `agent_invocation_host`, and it reaches `application::agent_finding_host`, which holds the delivery so a verdict is *read* rather than supplied — `agent_finding_action_request` has no verdict field a widget could forge one into. Every outcome is presented, refusal included: a refused conversion is named on the shipped agent panel rather than dropped, which the workshop construction cut exercises directly by making the same conversion produce a presented refusal and no draft **Settled** (BPR-0506, 2026-09-02): the label read `Qt half verified` while this cell already recorded the composed workbench as verified, so it was stale rather than weak. The row's own core command was re-run in this slice rather than referred to — `ctest --preset core-only -R '^core_agent_'`, six suites green in 1.00 s — so the core-only link line the claim names is evidence this slice produced |

Four `State` cells were relabelled by BPR-0506 on 2026-09-02. None of them is a
new claim: in every case the cell's own `Result` already recorded the evidence
that the label had stopped describing. *Every AI action originates from an
explicit visible user command* and *The side chat shows stale state after head
movement* both read `Core and panel verified` while their results recorded the
composed half as verified by BPR-0504, BPR-0513, BPR-0514, and BPR-0517.
*Finding/chat/promotion decisions* read `Qt half verified` while its result
recorded the composed workbench. *Private notes and drafts* read
`Core half verified` and still carried a leftover `Planned` phrase for a course
that had landed. Relabelling was the whole of the change; no result text was
weakened, and the one row whose command does not currently pass says so in its
own cell.

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

## Surface decision (BPR-0503)

BPR-0503 was asked to record the comment workshop's before/after surface rather
than assume it, and to justify a renderer in this ledger if it chose one.
**Plain text, in the editor's existing presentation.**

The premise the card offered survives contact with the code. There is no
Markdown or rich-text rendering surface anywhere in the tree:
`grep -rn 'setMarkdown\|setHtml\|Qt::RichText\|toHtml\|cmark\|md4c' src/`
returns nothing, and the single `QTextDocument` include in
`src/adapters/qt/src/measured_alignment_axis.cpp` measures plain-text block
geometry rather than rendering markup. Every label this adapter builds calls
`setTextFormat(Qt::PlainText)` on purpose. The workshop's two surfaces are
`QPlainTextEdit`, which is the widget the reviewer's own draft body already is.

Three consequences make that the right answer rather than the cheap one. The
comparison is between the reviewer's bytes and the agent's bytes, and a
renderer would show neither: a proposal differing from the original only by a
backtick or a leading `#` would render identically to it, which is the case the
reviewer most needs to see. The original must survive rejection **byte for
byte**, and a rendered surface has no bytes to compare - the suite's fixture
carries a tab and a trailing space for exactly that reason, and both survive a
`QPlainTextEdit` round trip. And GitHub, not BetterPullReview, decides how this
text finally renders; a local renderer would be a second opinion about that,
and the reviewer would be agreeing to what BetterPullReview drew rather than to
what they are about to publish.

What a renderer would have cost, stated so a later slice can weigh it rather
than rediscover it: a rich-text pipeline in the Qt adapter for one view
(`QTextDocument::setMarkdown` is Qt's own, but it is a document that normalizes
what it parses rather than a buffer that keeps it); a second text path in a
widget whose existing one is proven byte-exact; and a surface on which "the
original is unchanged" could only be asserted about a rendering rather than
about the text. None of that buys the reviewer a comparison they cannot already
make.

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
| BPR-0513 | Qt: place the side-chat panel and finding queue in the workbench drawer |
| BPR-0514 | Host: compose a side-chat session host so a send reaches the agent |
| BPR-0517 | Host: carry the side-chat message in the agent's input frame |

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

One decision was left explicit rather than pre-empted: the comment workshop's
before/after surface, since no Markdown renderer exists in the tree. BPR-0503
answered it in **Surface decision (BPR-0503)** above - plain text, with the
cost of the alternative named.

## Documentation ledger

Filled by BPR-0505 on 2026-09-02. Documentation only: no source, test, or
build file was touched, so nothing here changes what the tree does.

`docs/architecture/ai-agent-boundary.md` was **extended in place**, as
BPR-0178 required and as this card's non-goals restate: a second AI document
beside it would leave a reader working out which was current. Its new
top-level section, **Explicit AI review: findings, chat, workshop, and the MCP
helper**, carries nine subsections — the finding schema, the anchor policy and
its three states, chat scope, the two stale continuations, the promotion
boundary, full-agent behaviour, the MCP tool list, privacy, and a
claim-to-test table — followed by **What RFC-BPR-0012 does not claim**, which
states the limitations and names everything not delivered.

`docs/ux/ai-experience.md` gained **Implementation truth**: what shipped, the
three anchor-state names as the queue prints them, the two continuations by
name, the workshop's plain-text surface, and what the document specifies that
the tree does not yet have. `docs/verification/fixture-catalog.md` gained
**The finding-payload canary** and **The MCP helper canary** in its `## AI`
section. `docs/verification/master-verification-matrix.md` rows AI-3 and AI-4
now name the delivered surfaces and their selectors, and AI-5 (MCP read-only
enforcement) and AI-6 (side-chat staleness) are new.

No sentence in any of them asserts a behaviour the tree does not have. Each is
traceable to a selector recorded in this ledger, and every selector named in a
new or edited matrix row was confirmed present in `ctest --preset dev -N`, with
the core rows also confirmed in `ctest --preset core-only -N`.

### Region ownership

| Region | What RFC-BPR-0012 put there | Evidence |
|---|---|---|
| Functional core | `agent_finding.hpp` (the finding, its anchor, the frozen snapshot, and `validate_agent_finding`), `agent_finding_promotion.hpp` (`promote_agent_finding` and its eleven refusals), `agent_chat_scope.hpp` (seven entry points, four observations, the ordered scope ladder, `decide_agent_action`), and additive extensions to `agent_session.hpp` (`agent_continuation`, `offer_agent_continuations`) and `agent_context_supplement.hpp` (`agent_private_context_optin`) | `core_agent_finding`, `core_agent_finding_promotion`, `core_agent_chat_scope`, `core_agent_session`, `core_agent_context_supplement`, all Qt-free and provider-free under `core-only` |
| Application host | Payload decode in the one portable protocol loop, the validation seam and typed re-entry in `agent_finding_delivery.hpp`, `agent_finding_host.hpp` (holds one delivery, answers one action, reaches one durable authority), `agent_comment_workshop_host.hpp` (one proposal, five refusals, applies nothing), and `mcp_context_helper_supervisor.hpp` (pin, describe, start, stop) | `application_agent_finding_protocol`, `application_agent_invocation_host`, `application_agent_launch`, `application_agent_protocol_loop`, `application_mcp_context_helper` |
| Qt presentation adapter | `side_chat_panel.hpp` (ten disclosed fields, four intents, an `observe` that starts nothing) and `finding_queue_view.hpp` (seven disclosed fields, six actions), plus the comment workshop inside the existing `review_draft_editor` | `qt_side_chat_panel`, `qt_finding_queue_view`, `qt_review_draft_editor`, each linking `BetterPullReview::AdapterQt` only and registered through `bpr_add_network_isolated_test` |
| Shipped MCP adapter | `src/adapters/mcp/`, whose library links `BetterPullReview::Core` and nothing else, and the `tool_`-prefixed executable the live-mutation manifest walks | `application_mcp_context_helper`, `live_mutation_isolation`, and the link line itself |
| Composition | Five constructions in `src/app/src/github_application.cpp` under the same fixture and production split as `AgentHost`, all five named in `SHARED_REQUIRED` | `application_agent_finding_composition_contract`, `composed_application_hosts`, and one construction cut per host |

Neither agent output nor MCP input mutates core or Qt state directly. Every
result re-enters through a typed value carrying the session's exact binding.

### Typed intents, events, observations, requests, and deltas

| Kind | Values | Where the shape is held |
|---|---|---|
| Intents | `core::agent_action_intent` (deleted default constructor, `explicit`), `core::agent_scope_widening_intent` (the same), and the Qt intents `send_chat_message_intent`, `widen_chat_scope_intent`, `chat_private_context_intent`, `choose_chat_continuation_intent`, and `finding_action_intent` | `core_agent_chat_scope`, `qt_side_chat_panel`, `qt_finding_queue_view` |
| Events | `application::agent_finding_delivery` — session, the session's own binding, staleness, and one `agent_delivered_finding` per finding, verdict included. `agent_protocol_event_kind::finding_payload` carries the exact undecoded bytes out of a result frame, so the shared loop rather than a frame reader is where a payload becomes findings | `application_agent_finding_protocol`, `application_agent_invocation_host` |
| Observations | `core::agent_observation` — `refresh`, `navigation`, `comment_arrival`, `revision_update` — whose only product is a one-field `agent_observation_record`. `intent_for` returns nothing for every one of them, unconditionally | `core_agent_chat_scope`, `qt_side_chat_panel` |
| Requests | `core::agent_finding_promotion_request` (finding, verdict, choice, standing, and the target being promoted onto), `application::agent_finding_action_request` (session, ordinal, action, standing, target, note identity — **and no verdict field**), and `application::mcp_helper_launch` (executable, working directory, environment; **no argument vector at all**) | `core_agent_finding_promotion`, `application_agent_finding_composition_contract`, `application_mcp_context_helper` |
| Deltas | Both new widgets present whole accepted projections rather than deltas, and neither computes one. The single increment either retains is the finding queue's selected ordinal, which survives a re-presentation that still has a row at it, so an accepted refresh does not move the reviewer somewhere they did not ask to be | `qt_finding_queue_view` |

### Authority

One rule, held in three places so no single edit can lift it.

`core::agent_finding_promotion` has no field a `review_publication_proposal`
could occupy, and the draft command it emits is always an `edit`.
`application::agent_finding_outcome` has no field for one either, nor for a
`review_mutation_target`, and the finding host holds exactly two ports — a
supplier of the durable ledger store open for the review right now, and a
projection. The one durable write agent output can reach is
`review_ledger_store_port::store_note`. The composition source names
`review_mutation_coordinator` zero times and carries exactly one
`ReviewMutationHost->publish(` call site, which is the reviewer's own
`begin_publish` branch.

Authority over an anchor is single too: `core::validate_agent_finding` is the
only place the question is decided, the verdict is supplied to promotion rather
than recomputed, and the Qt request has no verdict field a widget could forge
one into. The MCP helper's authority is a pin: the host that froze the
projection supplies the binding, and every `tools/call` compares the snapshot's
own record against it before reading anything.

### Concurrency and generation

RFC-BPR-0012 introduces **no new generation counter**, and that is a decision
rather than an omission. Identity here is the `core::agent_session_id` plus the
frozen `core::agent_revision_binding` the session was started with, and
`core::agent_session_staleness` — `current`, `head_moved`, `epoch_pair_changed`,
`different_review` — is the whole vocabulary for how the world has moved since.
A second counter would be a second answer to the same question.

Everything that could race is answered by identity rather than by ordering.
Findings re-enter on the existing serialized completion lane and are projected
immediately before `finish`, so the terminal carries no undecoded payload and a
verdict is the only route to a presentation. `agent_finding_delivery` carries
the session's own binding and has no field the head current at delivery could
occupy, so a payload that arrives after an accepted head move is `head_moved`
rather than retargeted. `agent_finding_host` holds one delivery: an action
naming another session is refused `other_session`, and one naming an ordinal
that is not there is `unknown_finding`, rather than either being answered with
somebody else's finding. `mcp_context_helper_supervisor` refuses a second child
as `already_running`, because two helpers answering from one snapshot would have
two lifetimes and the reviewer could stop neither by name; it holds its own
process launcher for the same reason, since a shared one would let one cancel
stop whichever child the reviewer did not mean. The helper's own staleness is a
refusal, not a re-read: a moved head makes every call `revision_binding_stale`
and reads no projection at all.

### Evidence-backed no-impact regions

| Region | Claim | Evidence |
|---|---|---|
| The RFC-BPR-0008 mutation path | No new edge reaches it, and publication still round-trips through the one it already had | `application_agent_finding_full_agent_contract` (zero GitHub mutation calls of every kind; `review_mutation_coordinator` named zero times in the composition source), `application_review_round_trip_composition_contract` (green on the repinned digests) |
| The GitHub, Git, and SQLite adapters | The shipped MCP helper cannot reach any of them | `better_pull_review_adapter_mcp` links `BetterPullReview::Core` and nothing else — a one-line link check — and `live_mutation_isolation` walks the `tool_`-prefixed binary |
| Effect-freedom of `src/adapters/qt` | The two new widgets start no process and hold no launcher, credential, endpoint, or store | `architecture_boundaries` (no `QProcess` in the Qt adapter), plus the aggregate guards in `qt_side_chat_panel` and `qt_finding_queue_view`, each pinned by `sizeof` so a member added outside the aggregate fails too |
| The RFC-BPR-0004 alignment model | Findings consume its identities and add none; no coordinate is representable in a finding, an anchor, or a chat scope | `core_agent_finding`, `core_agent_chat_scope` (structured-binding guards that fail to compile on an added field) |
| The composed workbench's existing rendering | New drawer content stays downstream of the composed window | `qt_composed_github_scale_100` |
| Every other core suite | The additive core extensions disturbed nothing | `ctest --preset core-only -R '^core_'`, recorded green by BPR-0497 and BPR-0499 |

## Simplification

The behaviour-preserving pass over BPR-0497 through BPR-0505, plus BPR-0513,
BPR-0514, and BPR-0517. Two removals, and the reasoning for everything that was
looked at and left alone.

**Removed: the MCP helper's fourth request method.** `mcp_context_helper.hpp`
described `mcp_refusal::unknown_method` as "the method is not one of the four
this helper implements". It implements three — `initialize`, `tools/list`, and
`tools/call` — and never implemented a fourth; the comment was counting the
four *tools* the helper advertises, which is a different closed set two
declarations further up. The count is replaced by the three names, so the
comment and the dispatch can be compared by reading them rather than by
trusting a number. `application_mcp_context_helper` proves the behaviour either
way: it compares the advertised tool list against three independent sources and
drives twenty-one refusals through the real executable.

**Removed: the finding queue's action row.** The six actions sat in one
`QHBoxLayout` in `workbench_window`'s conversations drawer. BPR-0513's own
capture at 100 % showed the result: *Convert to local draft comment* was cut
off mid-word and *Add to private notes* and *Dismiss* were not on screen at
all. The drawer deliberately does not scroll sideways — a column that scrolled
horizontally would hide a control behind a gesture nobody makes — so a row
wider than the drawer does not become reachable, it becomes absent. The row is
now a `QVBoxLayout`. A column cannot clip: each button is as wide as its own
label, and the drawer already scrolls down, which is the mechanism BPR-0513
added for exactly this reason. Presentation only: the six actions, their order,
their labels, their object names, and the single `finding_action_intent` each
emits are untouched, and `qt_finding_queue_view` and the composed-window case
that clicks the placed *Convert to local draft comment* button both stay green.
The recaptured 100 % drawer crop shows all six labels whole.

**Not removed: `LastChatTurn`.** BPR-0517's test-only field on the composition
stays. The chat host's counters prove that a turn happened, but not what it
carried or which binding it kept, and they cannot be made to: the launch path
admits a session of its own, so by the time a turn has run the chat host is
holding a different one and the dispatched request is no longer readable off
it. `agent_chat_composition_public_path.cpp` asserts through this field that
the turn carried the second message's exact bytes and the *kept* old-revision
session identity and binding, asserted unequal to the revision now under
review. That is not a counter's job, and a counter cannot be given it.

**Not removed: the eight `Disable…Composition` construction-cut flags.** The
eight flags in `github_application_fixture_access.hpp` share one
declare-copy-guard shape at eight sites, and folding them into one keyed
mechanism was considered and refused. It would be new structure rather than a
deletion, which this card's non-goals forbid; the eight cuts are proven by
name, and a `Disable…Composition` name read at its own `if` is what makes a
cut legible where it acts; a ninth flag of the identical shape
(`SuppressDeviceBrowser`, RFC-BPR-0016) sits beside them and would either stay
outside the new mechanism or drag an unrelated RFC into this one. Eight
repetitions of three lines is a cheaper thing to read than an indirection that
answers for eight different constructions. Recorded so the next person choosing
between the two forms does not have to rediscover the argument.

**Not removed, and not this RFC's: `review_draft_editor::mutation_status_label()`.**
A mechanical sweep over every declared name in the RFC-BPR-0012 surface found
exactly one function with no call site anywhere in `src/` or `tests/`. It is
not RFC-BPR-0012's: it arrived with BPR-0229 under RFC-BPR-0008 and was
superseded by `mutation_status_text()`, which is what all six call sites and
both test suites use. Deleting it here would file an RFC-BPR-0008 removal in an
RFC-BPR-0012 ledger, so it is named here and left for a card of its own.

**Nothing else was found to remove**, and that is a result rather than an
omission. The five translation units the BPR-0506 scout had not read were read
in this pass — `agent_chat_scope.cpp`, `agent_finding_promotion.cpp`,
`side_chat_panel.cpp`, the comment-workshop half of `review_draft_editor.cpp`,
and `agent_protocol_loop.cpp` — looking for duplicated policy, dead members or
overloads, and parallel helpers. What is there is already deduplicated at the
seams where drift would matter: `refusal_for_line_target` is shared by the
draft and the defect promotions precisely so a defect cannot accept an anchor a
draft refused, and `agent_protocol_loop` is the one place the four protocols'
lifecycle rules live. Three shapes that *look* like candidates were left:
the two `make_field` helpers in `side_chat_panel.cpp` and
`finding_queue_view.cpp` are file-local by design and hoisting them would add a
shared Qt header for four lines; the two `present_side_chat` overloads are a
composed path and a construction-cut path, and the second is what keeps the
panel truthful when its host is cut; and the `declared` overloads in
`agent_chat_scope.cpp` are per-type by intent, since a template would remove
the switch a fifth enumerator has to be argued about at.

## Closeout preparation

RFC-BPR-0012 stays **`Proposed`**. No closure claim is made anywhere in this
ledger, and closure order is unchanged: it is ordered behind RFC-BPR-0008 and
RFC-BPR-0011. RFC-BPR-0008's closeout (BPR-0242) waits on its attended
live-GitHub cards, and RFC-BPR-0011's (BPR-0245) waits on RFC-BPR-0008
through RFC-BPR-0010 closing in order; RFC-BPR-0002 through RFC-BPR-0005 are
already `Auto-Verified`. The available-target work ends at BPR-0507 in
`10-backlog/`, as the RFC index's product-owner exception requires.

The reruns after the simplification, recorded with their results and this
commit:

- `ctest --preset core-only -R '^core_agent_' --output-on-failure` — **6/6
  passed**, 1.00 s, after `cmake --build --preset core-only` exited 0.
- `ctest --preset dev -R '^(application_agent_|application_mcp_|qt_side_chat_panel|qt_finding_queue_view|qt_review_draft_editor|composed_application_hosts)' --output-on-failure`
  — **28 of 29 passed**, 291.31 s, after `cmake --build --preset dev` exited 0.
  The one failure is `application_agent_invocation_composition_contract`,
  described under **Not delivered** below; it is not this slice's and does not
  touch this slice's changes.
- `qt_composed_github_scale_100`, `_125`, and `_200` and
  `qt_finding_queue_view` were re-run for the action-row change and are green,
  and `application_review_round_trip_composition_contract` is green on its
  existing digests — `github_application.cpp` was not touched, so neither pin
  moved.

### Not delivered

Named rather than left for a reader to infer from silence:

- **The six comment-workshop operations are specification only.** Clarify,
  shorten, make actionable, phrase as a question, make more direct, and check
  the premise appear nowhere in the tree. What exists is one shipped path from a
  proposed draft edit to a before/after proposal, with `workshop_comment` as a
  single `core::agent_entry_point`. BPR-0506 added the same caveat to
  `docs/ux/ai-experience.md`, which had carried the promise without it while
  `docs/architecture/ai-agent-boundary.md` already carried the qualification.
  Whether a card owns the six or the promise is withdrawn is the owner's
  decision, not this slice's.
- **`continue_against_old_revision` composes the old revision, except for its
  alignment.** BPR-0521 closed the half BPR-0514 and BPR-0517 named: the turn's
  supplement, the helper's pin, and the frozen snapshot's binding record are
  built from the kept session's own binding, the turn runs under that session
  rather than under one admitted against the moved head, and the disposable
  workspace is therefore materialized at the old session's `HeadOid` out of the
  Git cache — which is where the old revision's file bytes come from. What no
  part of the design can supply is the old *alignment*. `core::epoch_state`
  holds exactly one `AcceptedRevision` and one `AcceptedComparison`, and the
  workbench holds one alignment, so the comparison digest, line counts,
  segments, groups, and moves of the revision being continued against are
  retained nowhere once the head moves. Rather than send the current
  comparison's identities beneath an older binding — the silent re-anchoring
  this continuation exists to prevent — that turn freezes no per-path
  identities and carries no file receipt and no semantic selection. That is the
  documented *empty is a complete answer* case: every finding the turn returns
  arrives unanchored, which is what a turn that froze nothing has evidence for.
  Re-opening the old comparison would need retained epoch history or a
  re-alignment request against the binding's epoch pair, and is an owner
  decision rather than storage a slice should invent.
- **`application_agent_invocation_composition_contract` does not pass at
  HEAD.** BPR-0517 added the `chat_message` field to `encode_agent_supplement`
  and did not add it to this course's `expected_context_fields` builder, so the
  exact whole-map comparison sees 162 actual fields against 161 expected and
  the course exits non-zero. Every note-and-draft and canary-absence assertion
  in the course is satisfied; only the field-count equality is not. It is
  BPR-0517's regression, reproduced and diagnosed by BPR-0506 and deliberately
  not repaired inside a simplification slice, because a sibling card's
  committed defect repaired silently here would leave the board saying that
  card landed clean.
- **Two other action rows in the same drawer still clip at 100 %.** The
  side chat's four-button row loses *Continue against the old revision*, and
  the defect rows lose their last button each. Only the finding queue's row was
  this card's, and the same one-word fix applies to both; they are named here
  rather than changed, because a presentation edit to RFC-BPR-0009's and
  RFC-BPR-0010's surfaces does not belong in an RFC-BPR-0012 slice.
- **The MCP helper's frozen snapshot carries alignment facts, not file bytes.**
  `binding` and `alignment` are written from the same values the finding
  validator uses and `files/` is created empty, so `list_snapshot_files`
  answers with an empty list rather than a wrong one.
- **No deferred gate is hiding here.** **Deferred gates** above records `None`
  against the RFC text rather than by assumption: no criterion needs
  target-native Windows execution, a credential, an installed agent, a network,
  or a live GitHub mutation. A smoke run against an installed Codex or Claude
  harness stays optional extra validation, as it is for RFC-BPR-0011, and is
  deliberately not carded.
- **The Windows backend is not re-proven here.** RFC-BPR-0012 adds no platform
  code; the Windows process lifecycle remains claimed on BPR-0240's
  2026-08-26/27 target-native runs, exactly as RFC-BPR-0011's ledger says.
