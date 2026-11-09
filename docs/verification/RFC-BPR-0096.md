# RFC-BPR-0096 verification

Mandate `native-agent-sessions-r1` implements the reviewed pack. Immutable
RFC_BASE_SHA: `bebea27edf44fd63a7308307227035bf33391c5c`. Import `819ff2708`;
BPR-1951 claim `f8eaca5be`. Product candidate `26fdce665d7ff09f9ecd52e0df6765ce569b21ff` (the tested source tree promoted unchanged). The [live plan](../rfcs/plans/native-agent-sessions-v1/milestone-plan.json)
retains all five milestones. This ledger records scoped functional engineering
for the native Agent-tab, tour/history and tour-context milestones; it does not accept the whole RFC or later tour/history/replay work.

## Native Agent-tab frontier

The host keeps native ownership separately from the 128-entry invocation catalog.
Its private SQLite records bind a conversation ID, entry incarnation, account/PR
scope, pinned revision, backend handle, exact workspace/guard, turn counter,
submission state and compare-before-write revision. Observation restores only a
safe core binding. An explicit send checks current host policy and configuration,
validates the retained workspace and saves submission state before input. An
uncertain previous send is not resent. A storage refusal is visible.

The existing supervised launch path owns a C++ thread per cached native connection.
Claude stream-json and Codex App Server keep their input open after a successful
result; each later turn has its own decoder/output budget and deadline. Codex RPC
turn replies are correlated separately from connection initialization. Reconnect
uses the recorded handle. Codex JSONL uses exact-ID native resume with a process
per turn; it does not claim process reuse. One-shot tool-free requests remain
separate. No direct model API, API-key requirement or JavaScript/Python product
runtime is introduced.

Retention is an explicit journal state (phase 7), backward compatible with the
existing journal fields. Startup counts retained trees and never reclaims them.
Borrowed objects needed by a retained workspace are packed into that workspace
before removing its alternates file. Resume checks the exact tree, marker, journal,
pinned HEAD and required available object closure without cleaning allowed edits.
Missing or unverifiable state refuses resumption and preserves the tree. Complete retained workspaces check both head and required base closure. A focused real-Git course passed with the linked clone moved away, a fresh adapter and startup recovery, preserved tracked/untracked edits, and missing-base refusal. The restarted adapter also refuses an old in-memory cleanup capability; no generic retained-history reclamation is claimed.

Proposed implementation thresholds within the adopted RFC, with scoped functional evidence:

| Resource | Bound and refusal policy |
|---|---|
| Native admission | New native records check the combined native plus legacy count against 500, conservatively counting transcript projections separately. Native ownership is never silently pruned. The independent legacy writer still has its older 500-projection policy; BPR-1954 must unify bidirectional admission/migration before full AC11 acceptance. |
| Workspace journals | Existing 64-record bound includes retained and crash-recovery state. Protected/unverifiable work is preserved on refusal. |
| Retained workspace admission | 16 GiB and 262,144 directory entries across the workspace neighbourhood; no links are followed. A failed inspection refuses new input. These are admission bounds, not a promise that an agent cannot create more files during its authorized turn. |
| Cached native processes | Four per launcher, with at most five minutes idle. Releasing a process preserves durable ownership. |
| Idle output | 64 KiB per idle interval; late turn frames are decoded under the completed turn and cannot become the next answer. |
| Active input/output | Existing per-turn protocol/frame, stdin, stderr and retained-output limits. A live idle interval is not an active-turn timeout. |

## Evidence and current limitations

The initial ctest attempt could not launch the stale/missing protocol test binary
(exit 71); it supplied no functional baseline. After regeneration, existing launch,
protocol-loop, invocation-host and setup-host courses passed. New native protocol
assertions passed. New SQLite restart/CAS/isolation assertions passed after a real
admission defect was corrected: the private helper now uses `bind_text`, avoiding
ADL resolution to `std::bind` for mutable strings. Native real-process fixture
checks passed for Claude and Codex: two turns retained one supervised process and
handle, a later reply used a prior tool observation absent from the first assistant
answer, and another launcher reconnected by exact ID. Initial malformed synthetic
frames were corrected to include the required assistant role and turn items array;
protocol checks were retained.

The application and composed test targets built together. The production Agent-tab
course selects a real diff/change, sends a first question, then sends a follow-up
recovering the fixture's prior tool observation absent from the first prose answer.
Both Claude and Codex retain the same supervised process. A second application
process opens the same profile, restores metadata without inference, resumes and
sends again using the retained workspace with zero rematerialization. Initial offscreen
rasters exposed an incomplete setup course. The corrected course completes setup,
selects the agent, types in the actual composer and presses its visible Ask. Both
seed/reopen courses passed and their rendered conversation/composer states were
inspected externally. This is fixture-native evidence,
not installed Claude or all-platform acceptance.

| Focused evidence | Actual result |
|---|---|
| `application_agent_protocol_loop`, `application_agent_invocation_host`, `application_agent_setup_host`, `sqlite_agent_setup_storage`, `qt_agent_launch_canary`, `application_native_agent_sessions` | Six functional courses passed together after native protocol integration. |
| `architecture_boundaries`, `application_agent_invocation_composition_contract` | Passed, including existing launcher/host/gesture construction cuts. |
| `test_network_isolation_manifest`, `application_agent_workspace`, `agent_workspace_ownership` | Three passed after their missing executables were built. Initial missing-binary failures supplied no functional evidence. |
| Scoped clang-tidy 22, seven changed implementation units | Passed. Final Git/decoder/composition recheck passed with their unchanged production compile entries selected from the generated database; an `emplace_back` diagnostic was corrected. |
| Documentation structure and documentation script tests | `DOCUMENTATION_OK`; 67 script tests passed. Invalid-fixture error messages are expected script-test output. |

An initial composed follow-up correctly refused `selection_required`; the course
was corrected to select an actual change. A stronger terminal visible-answer check
then exposed a real picker/history race: refreshing the offered profiles emitted
an intermediate no-agent selection, which replaced a live thread with agent-zero
history. The picker now blocks intermediate signals and publishes only a settled
logical change; history loading also fences new questions. The final corrected public course passed both native families through setup,
typed input and visible Ask, current rendered first/follow-up answers, full app
reopen and retained workspace reuse. The direct picker regression passed, and
changed Qt/composition units passed scoped clang-tidy 22. Installed Codex 0.164.0 on macOS arm64
then exposed previously unsupported non-answer notifications. The decoder admits
an explicit schema-grounded list, including handshake-adjacent connection status
and resume-time goal state; notifications cannot complete a turn or satisfy
a correlated reply, and early RPC reply refusal remains tested. Two installed
turns succeeded with one process and memory of a random tool-read value never
included in BPR's assistant replay. Exact-ID reconnect then passed with a different supervised process,
the same recorded handle and the original observation. The final guarded course
reported success for all three turns and unchanged selected live files. An initial memory assertion incorrectly searched individual stream
chunks; the corrected assertion searches the assembled answer.

Installed Claude Code 2.1.310 on macOS arm64 reports `not_signed_in`.
`claude auth status --json` independently reported `loggedIn=false`,
`authMethod=none`. No credential stores were read and no API-key fallback was
used. Its installed inference/resume acceptance remains on BPR-1955, alongside
resume-only/no-inference, composed permission, Release usage and broader runtime
qualification. The installed course uses the production agent adapter and a
disposable local fixture with the existing sign-in, under a cooperative
qualification guard. Each run compared selected live project/config files;
the guard is not an OS sandbox.

Protocol capability sources were opened during engineering:
[Claude CLI reference](https://code.claude.com/docs/en/cli-reference),
[Claude streaming input](https://code.claude.com/docs/en/agent-sdk/streaming-vs-single-mode)
and [Codex App Server](https://learn.chatgpt.com/docs/app-server).
The installed Codex schema was generated without inference. Its thread/resume
parameters contain no `autoContinue` field; no invented field is sent. These
sources are capability evidence, not installed-course acceptance.

Frontend-design was loaded from the installed Claude plugin before presentation
changes. The milestone preserves native palette, fonts and the existing drawer
layout. All implementation rendering is offscreen; the owner's desktop is unused.
Debug timing is nongating. No live GitHub mutation or additional independent review
was performed. At that first-milestone handoff, tour integration and New session/history
controls remained dependent work; the next section records their scoped result.
Replay migration, tour state context, Release usage comparison and full RFC
acceptance remain with their dependent milestones. Native Windows/Linux acceptance is not implied.

## Tour sessions and nondestructive history

BPR-1952, milestone `tour-sessions`, product candidate `5ecd7583dd45e7a3cbf0780a3416fe89ae7cd09e`
(the tested source promoted unchanged), adds native conversations for the whole tour,
Problem, Solution and stable stop IDs, separately per entry incarnation and
accepted authority. Draft Tour shares the whole-tour generation. The existing
read-only tools remain separate from small one-shot writing hooks. Native tour
turns use current BPR context without replaying another agent's or thread's prose.

Both surfaces expose New session and scoped History. Admission retains the old
generation before selection changes, submits no inference and allocates no
workspace. Repeated New session on an unused generation reuses it. A failed or
full admission leaves the previous selection and transcript intact. History
selection restores the exact generation; native binding/workspace/configuration
checks still govern sending. Saved questions/replies remain bounded at 50 turns
and 64 KiB per text field, separately from the native backend's own memory.

The native catalog appends terminal text to the originating generation rather
than whichever history is selected. Core run identities retain proposal origin;
selection neither accepts pending work nor reconstructs proposals from saved
answers. Current context choices survive conversational reset. Accepted tour
text, sources, focus, notes and drafts live outside this operation. Removed-stop
chats remain identifiable and cannot send while absent. Other tour chats and
reset-detached histories have a read-only viewer that changes no active
association and sends no question.

Tour payload version 9 stores active and high-water chat generations. Versions
1–8 read these as zero. Reset saves a new namespace with its draft; Undo restores
the old active association while preserving the high water, so repeating Reset
after Undo cannot inherit an abandoned reset draft's sessions. Existing Stop,
uncertain-send, stale-result and native process/frame fences remain in force.
Deferred history work captures application/authority/binding and keeps an explicit
choice separate from later observational selection, without redirecting it.

App Server tour creation and exact-ID resume set read-only sandbox and never
approval, with MCP and web search disabled; every continuing turn reapplies
network-off read-only policy. The serialized fields follow the installed
0.164.0 schema and pass focused functional checks. Persistent Claude removes only
the conversational no-session-persistence flag and keeps its existing read-only
allowlist, empty MCP configuration and disclosed enforcement limits. These are
functional/configuration results; installed tour permission qualification remains
BPR-1955.

Evidence lives externally at
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/`:

| Check | Scoped result |
|---|---|
| `application_native_agent_sessions`, public9 and render-course9 | Four production-composition seed/reopen courses passed through visible Agent/tour composers for Claude and App Server. Whole/Problem/Solution/stable-stop scopes have distinct processes and same-process follow-ups, Draft Tour contributes to the whole-tour native generation, and New/empty reuse/exact-history selection preserve state without process/workspace starts. Reset/Undo restores associations. |
| Rendered windows | All four `offscreen-m2` rasters inspected: actual Agent and tour controls, retained replies after restart, accepted prose and pending draft parts remain visible. Offscreen layout evidence is not native system-appearance qualification. |
| Final functional/integration9 | 12/12 passed: core tour session, launch, invocation host, chat replies, SQLite native catalog and tour payload, both tour Qt views, architecture boundaries and their contract, isolation manifest and production invocation composition contract. |
| Changed workbench9 | `qt_workbench_window_scale_100` passed, including nondestructive New session translation and retained-admission checks. |
| Core-only6 | Focused core tour session, core dependency boundary and architecture boundaries passed (3/3). |
| Formatting and static analysis | Changed-file clang-format 22 passed. Scoped clang-tidy 22 passed all 13 changed implementation units; two tour units were rechecked after narrow diagnostic fixes. |
| Documentation | Structure gate and 67 documentation script tests passed; final tracker upkeep receives its own structure check. |

Actual failures were retained and corrected: initial host integration spelling and
capture errors, misplaced Qt fields/missing include, one incorrect build target,
a provisional chat binding lost during history loading (`no_session`), a deferred
cleanup probe that changed quiescence, a history test that selected a correctly
retained empty generation instead of its exact previous ID, and a synthetic
whole-tour fixture that recognized only the part-answer schema. The production
structured-answer validator was preserved. Tidy diagnosed two existing local
spellings, an automatic move and an optional-copy expression in touched tour
units; these were corrected. No failure was accepted as a pass.

Frontend-design was loaded before Qt work. Palette, fonts and existing drawer
layout remain native; no owner desktop, personal profile, live GitHub mutation,
extra reviewer or model API was used. No dedicated Debug timing/performance course
was run. Native Windows/Linux, installed Claude sign-in, full installed permission
and resource/Release-cost qualification remain later work. BPR-1954 still owns
legacy replay/migration and bidirectional quota/projection-save reconciliation;
full AC03/AC11 acceptance is not claimed. BPR-1953 owns the versioned tour guide,
coherent source/status snapshots and warning correction. The RFC stays Proposed.


## Authoritative tour context and source reasons

BPR-1953, milestone `tour-context`, product candidate `a89b0fa88f5a8e4c0eae8495ab8a44da8f7377e8`, implements
`bpr.tour-guide.v1` and `bpr.tour-context.v1`. The guide and complete snapshot
accompany every explicit ask or Draft Tour request because native compaction and
reconnect cannot establish that prior framing survives. Ordinary observations
submit no inference. Native turns retain backend memory; replay callers still
supply only their bounded same-thread prose. The 160 KiB whole tour carrier and
16 KiB typed-question cap refuse an oversized required snapshot before dispatch;
stops and blocker facts are not dropped to fit.

The content digest covers source/head observations, stable stop IDs and current
numbers, preparation, selected passage/source, focus, proposal decisions and
publication/save observations, rather than accepted-tour revision alone. The
snapshot carries original/mapped/suggested/related ranges, one-based lines and
commit/side identity. Missing evidence remains explicit: confirmation head was
not historically recorded separately from a range's commit; related mapping
observations are unknown. A bounded 128-record decision journal retains origin,
pending/accepted/discarded/superseded/refused outcomes and omission counts. Its
window scope and earlier unknown decisions are disclosed; it does not reconstruct
proposals from native history or import other chats' transcripts.

Core source/preparation/publication reasons drive the prompt and Qt's source
chip and route marker. A confirmed author-selected replacement alone has no
source warning. Missing prose and generated-stop visitation/preparation remain
separate from confirmation. Ambiguous/missing/unavailable mappings retain their
actual attention reasons. Available actions use the existing Show in code,
Edit stop, Use selected lines, Ready, next and Head changed/Confirm flow. The
publication snapshot includes the exact host presentation and qualification
facts; read-only, ownership, conflict, source and operation blockers confer no
publication authority. New publication also refuses unresolved tour conflicts.

Results resolve display numbers against the submitted draft's stable IDs and
refuse targets changed during the turn. Author acceptance remains explicit and
undoable. Text-only edits and ordering preserve confirmed bindings; new and split
stops retain their actual unconfirmed evidence. A repeated identical text update
can be accepted without inventing a draft revision, and its acceptance is recorded.
No output flag grants confirmation, Ready or publication.

Evidence at `/Users/jaeger/.codex/tmp/native-agent-sessions-execution/`:

| Check | Actual scoped result |
|---|---|
| `m3-public7.log` | Network-isolated production driver passes all four Agent/tour seed/reopen courses. Both fixture protocols see the full bootstrap, a real composer explanation update, author Accept, unchanged confirmation and a follow-up containing current prose and accepted origin. |
| `offscreen-m3`, `m3-render8.log` | Both full-window tour/reopen rasters and confirmed-replacement/ambiguous-mapping light rasters inspected. Qt source status course passes. This is offscreen presentation evidence, not installed-model explanation or native appearance acceptance. |
| `m3-integration7.log`, `m3-tour-integration8.log` | Nine integration checks passed, then the ordinary tour course initially stopped before startup because its scoped writing helper was missing. After building that helper, that course passes (including existing source/related/order/publication-preview flow). No product assertion was relaxed. |
| `m3-focused6.log`, `m3-core-check7.log` | Four focused core/prompt/Qt checks and four core-only/dependency/architecture checks pass. Cases distinguish identical proposed/confirmed source evidence, missing prose, ambiguity, unchanged-revision freshness, caps and captured-ID stale results. |
| Formatting/static analysis | clang-format 22 passes 22 changed C++ files; diff check passes. Nine implementation units pass scoped tidy: five final/corrected units in `m3-tidy7.log`, four unchanged units in `m3-tidy6.log`. |
| Documentation | Product structure gate and 67 script tests pass; final tracker upkeep receives a fresh structure check. |

Actual intermediate failures are preserved: declaration ordering/missing include,
two old prompt assertions that incorrectly excluded discarded/empty-focus facts
from the entire new snapshot (their pending-list/schema coverage was retained),
the no-op acceptance guard exposed by public reopen, two editor tidy diagnostics,
and the missing integration helper. The fixes reran their affected checks.

Frontend-design was loaded before Qt changes. No owner desktop, personal profile,
remote mutation, extra agent/reviewer, credential workaround or direct model API
was used. Dedicated Debug timing/performance courses were omitted under the
permanent owner policy, with no performance credit.

This accepts M3's functional engineering boundary and contributes to AC07–AC10;
it does not accept their full installed/native scope. The owner's exact original
all-stops warning sequence remains unverified. In particular AC08's installed-agent
explanation course remains on BPR-1955; fixture carriage is not installed-model
understanding. BPR-1954 retains explicit replay/legacy migration plus bidirectional
shared quota, protected history and save-failure reconciliation. BPR-1955 retains
installed Claude sign-in, installed read-only/resume/compaction/platform/resource
and Release reuse/cost qualification. The RFC remains Proposed.


## Explicit replay and legacy history migration

BPR-1954 adds a separately identified **Transcript replay** generation with a
source-history link. The original native handle, transcript and owned workspace
remain intact. Native failure sends no replay automatically; choosing replay
itself submits no inference. Replay uses only selected-generation answered prose,
new private workspaces and the existing provider/authority/read-only checks.
Legacy slots have unknown entry identity and require an explicit destination;
they remain readable without selection or inference. New session is the empty
alternative, labelled replay for unsupported backends, with unused-empty reuse.

Core replay bounds retain Agent newest50/8KiB-fields/48KiB or tour newest12/48KiB.
Actual retained-turn omissions and cut bytes enter the prompt and presentation;
unknown earlier storage omissions remain labelled unknown. Unanswered/interrupted
turns are excluded. The whole tour snapshot plus replay is refused before launch
when its carrier exceeds160KiB. Native/legacy500 admission is now bidirectional
and transactional: no protected record is pruned; failed admission keeps the
prior selection, and existing records remain savable at capacity. Legacy save
failure is projected visibly rather than claiming durable retention. The128-entry
invocation catalog remains independent. Automatic retained-tree reclamation is
not implemented: exact unverifiable/modified ownership is preserved and capacity
refuses new admission.

Evidence is external under `/Users/jaeger/.codex/tmp/native-agent-sessions-execution/`.
`m4-functional3.log` passes 5 focused/public checks, including all 6 seed/reopen
courses with actual composers. Missing native workspace refuses before a process
or retry; explicit replay gets a fresh workspace, preserves the source tree and
omits the unsent failed question. Unknown legacy slot selection and replay survive
restart. Both native fixture protocols exercise read-only tour replay. A custom
plain harness cannot silently acquire native continuation and works after explicit
labelled replay. `m4-core-check.log` passes core replay, dependency and architecture
boundaries. Final candidate checks/static/gates and revisions are recorded below
when complete. Prior scoped M1–M3 installed/platform limitations remain open on
BPR-1955. No full AC10/AC11 or RFC acceptance is inferred from this migration.

Final migration evidence: `m4-integration-final.log` passes 10/11 checks, including
architecture contract, launch/loop, Qt, storage and composition contracts. The
public driver's new unsupported-New action initially clicked while its asynchronous
history load disabled the button; the test now waits for the public enabled state.
`m4-public-raster.log` then passes all 6 production seed/reopen courses under network
denial and saves the offscreen-m4 rasters. Agent replay/reopen and native tour
rasters were inspected. `m4-uncertain-check.log` passes a final replay restart
regression: uncertainty is visible even without native handle/workspace, with zero
process/workspace starts. Native-only early return previously hid that notice and
was corrected. No automatic resend was added.

Clang-format22 passes 23 changed C++ files. Scoped tidy passes all 9 implementation
units (final affected6 in `m4-tidy2.log`, final uncertainty host in
`m4-tidy3.log`, unchanged3 in `m4-tidy.log`); a SQLite cast-placement diagnostic was
corrected. Documentation structure passes and 67 documentation script tests pass.
Earlier build failures (target spellings, statement expected/optional, tooltip
insertion, and structural test field count) remain in the external logs; none was
accepted as functional evidence. Core-only replay/dependency/architecture3/3 pass.
Debug timing/performance courses were not selected and receive no timing credit.
Frontend-design was loaded before Qt changes. This is scoped BPR-1954 acceptance;
BPR-1955 and qualified-continuation remain active for installed/Release qualification.

BPR-1954 product candidate: `2de6a3336a02b7aae5a62700d17b620eff98451d` (tested source promoted unchanged).


## Installed qualification and operator reconciliation (BPR-1955, partial)

The exact installed scope is macOS 26.6.2 / Darwin arm64, Qt 6.12.0, tests-enabled
optimized Release, Codex CLI 0.164.0 and Claude Code 2.1.310. No Windows/Linux native
or system-appearance acceptance is inferred. Opt-in installed courses use the
production protocol/launch builders, actual BPR guide/snapshots and a disposable
repository containing `src/item.cpp`; the agent receives no BPR source access.
A test-only transparent stdio wrapper counts reported numeric usage and fixture
read commands. It is not a product Python dependency or an OS sandbox.

| Actual installed course | Result |
|---|---|
| Codex App Server native, three questions | Pass: first two turns keep one runtime process, third resumes the exact handle in a new process; one preserved fixture workspace. Follow-up recalls a random tool-read value absent from the first prose answer, with no second read. |
| Codex App Server explicit replay, same three logical questions | Pass after the EOF correction below; three new fixture workspaces/processes and three fixture read commands. Every turn carries full current BPR context and bounded saved prose only. |
| Codex JSONL native, three questions | Pass by exact handle with one workspace and a process per turn. No persistent-process claim. |
| Codex App Server explicit interruption | Pass: READY, an authorized long turn cancelled on its first streamed output, then exact-ID reconnect answers only the explicit AFTER_INTERRUPTION question. The stopped question is not automatically continued or resent. This later course reports model `gpt-6.1-sol`. |
| Claude Code 2.1.310 | Pending. Earlier attempts were signed out. A later normal and exact-materialized-environment CLI status reports loggedIn=true/authMethod=claude.ai, including USER. Actual read-only native, replay and interruption attempts fail before a usable answer. The fresh post-owner-handback native retry also returns authenticated HTTP429 and identifies an exhausted weekly allowance, with a reported reset November 10 at 00:00 America/Los_Angeles. This is an availability refusal; no native acceptance or usage credit is assigned. |

The installed Codex answers were inspected. They distinguish unconfirmed Stop 1
from Stop 2 with the identical path/range confirmed but unfinished prose, name
Show in code, Edit stop, Use selected lines and Ready, next, and keep publication
separate. The updated same-revision/reconnect snapshot is authoritative. Its
confirmed author-selected Stop 1 still has missing mapped evidence, so the model
correctly explains the independent source_unavailable reason; this does not claim
to reproduce the owner's original all-stops sequence or invent absent evidence.
M3's distinguishing source/core/UI cases remain unchanged.

Equivalent App Server Release observations, one three-question sample per mode:

| Observed metric | Native | Transcript replay |
|---|---:|---:|
| Elapsed total | 56.984s | 78.772s |
| Fixture workspace starts | 1 | 3 |
| Runtime process starts | 2 (includes exact reconnect) | 3 |
| Fixture read commands | 1 | 3 |
| Reported input tokens | 101047 | 177476 |
| Reported cached input tokens | 51456 | 124416 |
| Reported output tokens | 2133 | 2448 |
| Reported USD cost | unavailable | unavailable |

The usage totals retain App Server's final cumulative thread counters; replay
has three observed resets and three serial processes. Cache counts are reported,
not converted into a price or a fixed savings percentage. Actual model IDs were
not captured in this first comparison; identical configured built-in read-only
profiles were used. The separately observed interruption model is not imputed to
those earlier turns. Workspace-start counts here describe the installed harness;
production-host reuse/admission is independently exercised by public courses.
JSONL reports input/cached/output counters of 43696/28800/747,
71266/51200/990 and 104954/78848/1779 for its three invocations; these are retained
as reported, with no sum across potentially cumulative resume counters. Claude
usage and its equivalent comparison remain unqualified. Single-course elapsed
values are not universal performance promises. Dedicated Debug timing/performance
courses were not selected and receive no timing credit.

Installed qualification exposed two causal issues. One-shot App Server previously
closed stdin immediately after turn/start; installed 0.164.0 treated EOF as
cancellation and replay returned incomplete_frame. It now closes only after the
decoded terminal, preserving native persistent streams. Catalog close bookkeeping
still closes once. Tour persistence also now saves the terminal assistant item
consumed by the public tour, excluding preliminary narration that could corrupt
restored structured answers. The initial qualifier itself concatenated commentary
and terminal JSON; it was aligned with the existing public consumer without
relaxing the answer validator.

Final functional evidence is external in
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/`:

- `m4-final-resource-check.log`:5/5 pass, covering architecture, isolation manifest,
  protocol loop, invocation host and real process canary. Exact64KiB idle output
  fits; the next byte closes before another turn is selected. Four idle process
  slots evict at the fifth session without losing exact native resume/workspace.
  Active output/frame/input and cancellation cases remain passing. Five-minute
  idle expiry is a configured ceiling; no wall-clock expiry claim is added.
- `m4-qualification-public-raster.log`:all 6 production Agent/tour native/replay
  seed/reopen courses pass under network denial. A sparse logical 16 GiB+1 file in
  the owned fixture refuses admission before process start and preserves the
  original history/workspace; only that test-owned file is removed. Shared 500
  catalog/protected-save and failure courses remain passing. The 262144-entry
  ceiling is documented rather than claimed as separately boundary-exhausted.
- Seven offscreen rasters in `offscreen-m4-qualification`, including active Agent
  and tour Transcript replay, were retained; both replay rasters were inspected.
  Controls and provenance/omission disclosures are visible. This is offscreen
  layout evidence, not target-native system appearance acceptance.
- `m4-qualification-tidy.log` passes both changed production implementation units;
  changed-file clang-format22 and documentation gates pass. Earlier67 passing
  documentation script tests remain applicable and unchanged.
- Installed guard receipts for Codex native/replay/JSONL/interruption pass and
  selected live declaration bytes remain unchanged. Claude failure receipts also
  preserve those bytes. These are cooperative comparisons of selected files,
  not complete filesystem containment. No credential, personal browser/profile
  data, API-key workaround, live GitHub mutation or extra reviewer was used.

Actual failed attempts remain explicit: initial EOF replay cancellation, helper
commentary concatenation and const-array/cancel-method fixture build diagnostics,
synthetic idle framing, catalog double-close, and one stale-helper launch stopped
by its exact task-owned wrapper PID. Corrected narrow checks pass. Native engine
transcripts remain engine-owned; BPR metadata, New session, Reset tour and purge
are distinct. Automatic retained-workspace reclamation is not implemented:
modified/unverifiable ownership is kept and admission refuses safely. This work
does not close M8/M9 vault, crash, vulnerability, package trust or full-removal gaps;
profile purge excludes shared updater/registry state and is not uninstall.

Live operator/product/architecture contracts and RFC0011/0012/0092 carry the scoped
RFC0096 amendment while historical Done-card verdicts and immutable source packs
remain unchanged. BPR-1955 and qualified-continuation stay pending for actual
Claude request availability and subsequent native qualification. The full RFC
remains Proposed; BPR-1956 is not dependency-eligible.

Availability wizard session `e5378f2a-8c51-4e65-81e2-c08ac10cb8a9` consumed the owner's acknowledgement at revision2 and recorded the actual failed retry at revision3. The procedure is complete and its server stopped; product acceptance remains pending. Its external state is `/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-availability-fepjjjg6/state`. Authenticated URLs are retained only in external runtime state.

BPR-1955 available-scope product candidate `f98e14cd604a251a32e8cfeb9c33a5b0614a489a`; the source tested above was promoted unchanged. Required Claude acceptance remains pending.

### Post-handback Claude retry

A fresh implementer resumed mandate `native-agent-sessions-r1` after consuming
the actual human availability acknowledgement. Ready/claim bookkeeping commits
`59baae4c3` and `4e8c05191317585d230d39602c3cc2978df6836b` change no product inputs.
The exact Release qualification binary remains SHA-256
`d09aba96eb271f63b7847aab8bbeae84e68ec38b4e5dc0ca36d3a07ff2f89bd0`.
The new owner-private fixture root is
`/Users/jaeger/.codex/tmp/bpr-m4-claude-resume-8hq1wfac`.

The native first turn returned `protocol_failure` / `remote_failed` in 3.032s,
with zero tool events and no usable answer. Claude's own result reports
`is_error=true`, `api_error_status=429`, and an exhausted weekly allowance
resetting November 10 at 00:00 America/Los_Angeles. The reported runtime model
is `claude-opus-5-5`; the error also reports a synthetic response model, which
is not inference evidence. The driver stopped at that first failure. Follow-up,
exact-ID resume, replay, interruption and Claude usage comparison were not
rerun and remain unqualified. No account, model, credential or API-key
workaround was attempted.

The external `m4-resume-installed-identity.json`,
`m4-resume-installed-results.json`, `m4-resume-claude-native.log`,
`m4-resume-claude-native-guard.json` and the fixture's
`native/qualification-results.json` retain exact identity and failure evidence.
The completed guard compares the same three selected live declaration files
unchanged; it remains a cooperative comparison, not an OS sandbox. The failed
driver and its native child have ended. Unchanged Codex, public-path,
resource, format/tidy and documentation-script evidence above is reused.

Fresh weekly-availability wizard session
`94eafb5d-7d90-4146-9de5-f8802d8bade4` serves external state
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-weekly-pnwqk34v/state`
at consumed revision1. Its next step is the owner's actual availability
acknowledgement, followed by agent-captured qualification. No automatic retry or
scheduled inference is armed. Acknowledgement alone does not accept behavior.
This session's loopback server is the only handed-off command. BPR-1955 remains
Parked for human-review; qualified-continuation is not accepted and BPR-1956
remains dependency-ineligible. The RFC stays Proposed.
