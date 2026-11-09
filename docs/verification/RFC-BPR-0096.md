# RFC-BPR-0096 verification

Mandate `native-agent-sessions-r1` implements the reviewed pack. Immutable
RFC_BASE_SHA: `bebea27edf44fd63a7308307227035bf33391c5c`. Import `819ff2708`;
BPR-1951 claim `f8eaca5be`. Product candidate `26fdce665d7ff09f9ecd52e0df6765ce569b21ff` (the tested source tree promoted unchanged). The [live plan](../rfcs/plans/native-agent-sessions-v1/milestone-plan.json)
retains all five milestones. This ledger preserves each milestone's observations
and the final criterion-by-criterion engineering audit.

Current frontier: all five milestones and AC01–AC12 are accepted for mandate
`native-agent-sessions-r1`, with the narrow installed-Claude owner waiver.
The required independent critical review is complete at reviewed revision
`ad81ec65c085cea6d81959cbf50fa6a65ef65702`, product `1df298efb9d5ac7fad8eb8886f1eaf7e3e6618c2`,
with no remaining blocking or nonblocking findings. The
[final acceptance](#final-accepted-scope-and-independent-review-reconciliation)
reconciles the actual repairs, evidence, dependencies and preserved limits.
Earlier milestone sections retain their historical frontiers, failures and
unreached observations; they do not replace this final disposition.

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


## Installed qualification and operator reconciliation (BPR-1955, amended acceptance)

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
| Claude Code 2.1.310 | Unverified; remaining installed verification is waived by the owner amendment below. Earlier attempts were signed out. A later normal and exact-materialized-environment CLI status reports loggedIn=true/authMethod=claude.ai, including USER. Actual read-only native, replay and interruption attempts fail before a usable answer. The fresh post-owner-handback native retry also returns authenticated HTTP429 and identifies an exhausted weekly allowance, with a reported reset November 10 at 00:00 America/Los_Angeles. This is an availability refusal; no native acceptance or usage credit is assigned. |

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
remain unchanged. At the original handoff BPR-1955 and qualified-continuation
stayed pending for Claude availability and native qualification, with BPR-1956
ineligible. The owner amendment below supersedes that acceptance wait; it does
not change the failed observations. The full RFC remains Proposed for the
separate accepted-scope audit.

Availability wizard session `e5378f2a-8c51-4e65-81e2-c08ac10cb8a9` consumed the owner's acknowledgement at revision2 and recorded the actual failed retry at revision3. The procedure is complete and its server stopped; product acceptance was still pending at that handoff. Its external state is `/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-availability-fepjjjg6/state`. Authenticated URLs are retained only in external runtime state.

BPR-1955 available-scope product candidate `f98e14cd604a251a32e8cfeb9c33a5b0614a489a`; the source tested above was promoted unchanged. At that handoff required Claude acceptance remained pending.

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

At the retry handoff, fresh weekly-availability wizard session
`94eafb5d-7d90-4146-9de5-f8802d8bade4` served external state
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-weekly-pnwqk34v/state`
at consumed revision1. Its next step was the owner's actual availability
acknowledgement, followed by agent-captured qualification. No automatic retry or
scheduled inference is armed. Acknowledgement alone does not accept behavior.
At that handoff this session's loopback server was the only handed-off command;
BPR-1955 was Parked and qualified-continuation remained unaccepted. The following
owner amendment supersedes that wait without completing the unanswered steps.


### Owner Claude verification waiver and M4 acceptance

The owner's actual instruction on 2026-11-08 is: “the claude is out of usage,
just keep going without verifying it works”. This instruction is recorded
separately from runtime observations as acceptance amendment
`native-agent-sessions-claude-waiver-r1`, under the unchanged implementation
mandate `native-agent-sessions-r1`. It waives the remaining installed Claude
native/replay, exact-ID resume, interruption/Stop, explanation, requested-
permission continuation and equivalent Release usage verification for this
pack's progression and acceptance. No Claude API, authentication or inference
request was made in this amendment round.

Claude remains unverified. The authenticated HTTP429 weekly-limit failure, zero
tool events, no usable answer and all unreached courses above retain their actual
meaning; they are not passes. Earlier signed-out and failed attempts remain in
the retained receipts. Functional fixture, installed Codex, resource, public-path
and documentation requirements remain unchanged. Product candidate
`f98e14cd604a251a32e8cfeb9c33a5b0614a489a` and Release binary SHA-256
`d09aba96eb271f63b7847aab8bbeae84e68ec38b4e5dc0ca36d3a07ff2f89bd0` are unchanged.

The live RFC states the waiver and the live plan amends only M4's outcome and
second exit. The immutable source pack, original `source.json` provenance,
historical Done-card verdicts and prior acceptance receipts are preserved.
`MILESTONES.md` is regenerated from the valid live plan; its inherited content
was an invalid-plan diagnostic rather than a usable generated view.

| Amended M4 obligation/exit | Judgment and evidence |
|---|---|
| BPR-1954 and exit1: explicit replay/New session, provenance and permission/submission fences | Accepted unchanged: product `2de6a3336a02b7aae5a62700d17b620eff98451d`, public6, core/host/SQLite and negative-control evidence above. |
| BPR-1955: installed Codex, composition, resource, usage and operator documentation | Accepted with the scoped Claude waiver: unchanged passing AppServer native/replay/exact-resume/Stop, JSONL, explanation, resource5/public6 and seven rasters; actual Release metrics above. Fresh amendment-round documentation gate passes (`m4-waiver-documentation.log`). |
| Amended exit2: actual available-scope evidence plus explicit Claude limitation | Accepted on those observations and the owner's acceptance amendment. The amendment is applied; installed Claude is not reported as passing. |

Wizard session `94eafb5d-7d90-4146-9de5-f8802d8bade4`, external state
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-weekly-pnwqk34v/state`,
is superseded. Status since cursor1 contained no new events. The host was stopped
with `wizard.py stop`; its acknowledgement and following qualification remain
unanswered, with state/history intact. The schema has no supersede operation, so
the lifecycle disposition is recorded separately in external
`m4-waiver-wizard-disposition.json`; no human answer was fabricated. There is no
remaining review server, scheduled retry or automatic inference.

M4 `qualified-continuation` is accepted under the amended requirements. BPR-1955
is Done at `5106dd472bd4312bd6c060d9bbb3559656ed0128`; BPR-1956 is Ready for a
fresh `accepted-scope` owner. The live amendment is committed at
`af1ab6d61c17bb1602e4d5bb1a004a9d9494f622`. This
does not close the RFC: the final AC01–AC12 audit remains with BPR-1956. Existing
Windows/Linux/system-appearance, forced-compaction, entry-count boundary, actual
five-minute expiry, original all-stops sequence and release/vault/crash/security
limitations remain explicit. Safe preserve/refuse remains the retained-workspace
policy; automatic reclamation is not claimed. Debug timing remains nongating.

Recovery for this amendment is external
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-waiver-handoff.md`,
with the renewed exact M4 checkpoint, owner-amendment record and acceptance
receipt beside it. No product or visual change was made in this round.


## M5 accepted-scope audit and critical review handoff

Mandate `native-agent-sessions-r1`; full RFC baseline
`bebea27edf44fd63a7308307227035bf33391c5c`. M5 claimed BPR-1956 at
`16432868082786bf31a77fa840a4e939cf7fc3ae` from clean
`1d3804a242eddc61221fa89d85b16b127ea9a96b`. Exact product candidate:
`bf09db0a3c73836587a4d6a0ea6e447b18b79829`. This is an engineering audit, with final independent
critical review required by the owner's later instruction. No critical reviewer
has supplied a result yet; no terminal RFC/card/pack acceptance is claimed.

### Causal AC07 repair and exact-candidate checks

The audit reproduced an in-scope gap through the actual tour composer after head
movement: the native catalog retained the old binding, but tour dispatch supplied
no explicit continuation and exposed no old-revision action. The negative course
fails `tour-old-revision-action` (`m5-tour-stale-negative.log`). The repair adds
Continue old revision and a saved/current-head disclosure through existing native
Qt controls. Core continuation decisions govern the offer; consent names exact
scope, selected generation, saved session and observed binding. Selecting another
chat/history or observing a changed binding invalidates it. Choosing sends no
inference. New session continues to retain history and requires its own admission.

A continued tour turn uses the existing explicit-session route with its own
complete tour context and read-only options. It bypasses the Agent tab's finding
realignment, whose ordinary launch tail does not carry tour options. The retained
workspace is inspected and reused, and the authoritative snapshot names pinned
and observed heads. Existing `finish_tour_ask`/`TourWorkspaceHead` stamping and
passing core `proposal_heads`/captured-target cases preserve analyzed OIDs and
stale proposal refusals. Stop remains enabled if the head moves during authorized
work; the focused Qt regression covers this state.

`m5-repair-check.log` retained the initial stale-control projection failure;
accepted revision observations now refresh it. `m5-repair-public2.log` then
passed old-revision native continuation, but New session correctly refused because
a synthetic reaccepted handoff had no fetchable new-head material. The final
course uses the real Refresh gesture and the existing two-head recording and waits
for the accepted comparison to move. No workspace refusal or source assertion was
relaxed.

| Fresh M5 evidence (external execution root) | Actual result |
|---|---|
| `m5-focused-integration.log` | 9/9 before repair: native/replay public6, core source/session, prompt, protocol, host, SQLite and safe workspace ownership. |
| `m5-final-integration.log` | 10/10 after repair: architecture/isolation, core tour, Qt tour, public6 and Agent-tab ordinary/stale/continued/anchored plus invocation composition contracts. |
| `m5-final-stop-check.log` | Final exact product 2/2: Qt tour includes Stop while revision choice is pending; public6 still passes. |
| `m5-repair-render3.log`, `offscreen-m5` | All six production courses pass. Actual old tour keeps original process and workspace HEAD; its wire carries read-only/network-off plus different pinned/observed bootstrap heads. A fresh generation runs at the fetched current HEAD in a distinct process; reopened old history requires choosing again. Both full-window stale-tour rasters inspected. |
| `m5-tidy.log`, `m5-tidy2.log`, `m5-final-tidy.log`, `m5-final-qt-tidy.log` | Scoped clang-tidy22 passes all five changed production units, with final tours and Qt rechecks after their last edits. |
| `m5-final-format6.log`, diff check | clang-format22 passes 10 changed C++ files; Python driver compiles; no whitespace errors. |
| `m5-scope-audit.json` | All15 immutable source-pack files and adopted `source.json` unchanged since import. The five original product commits cover96 scoped paths; M5's explicit-path product commit adds only its bounded repair/tests/operator prose. Interleaved Go-to-line/RFC0097 work is preserved and supplies no RFC0096 acceptance. |

Frontend-design was loaded from the configured installed skill before M5 Qt work.
The palette, fonts and existing tour column remain native; rendering is offscreen,
with no owner desktop or personal profile access. No Claude request, direct model
API, remote mutation, extra agent or push occurred. Prior M1–M4 installed Codex,
resource/storage/ownership, core-only, source/permission and67 documentation-script
passes remain relevant where their inputs are unchanged. Dedicated Debug timing
courses were not run and receive no timing credit.

### Full criterion audit

The judgments below apply to the exact candidate with unchanged earlier evidence
reused. They are implementation judgments awaiting the required independent
critical review, not a claim that Claude or every platform passed.

| Criterion | Engineering judgment and direct evidence |
|---|---|
| AC01 | Native Agent/tour visible composers keep backend memory, handle/process and workspace; installed Codex AppServer and JSONL exact resume. Claude functional fixtures pass; remaining installed Claude evidence is owner-waived. |
| AC02 | M1/M2 core/SQLite identity, incarnation, authority and history isolation; four/evolving six public scopes, Reset namespace/high-water, removed-stop inspection and captured stable stop IDs. |
| AC03 | Public application seed/reopen and exact selected history; retained real-Git object/edit recovery with lending clone removed; missing workspace/base, CAS, uncertain saves and visible legacy-save refusal. |
| AC04 | Both visible New/history controls, unused-empty reuse and full-capacity preservation; Reset/Undo payload v9 and accepted text/source/context/proposal-origin checks. |
| AC05 | Protocol/host/CAS/frame/uncertainty/cancellation tests; installed Codex Stop and exact explicit resume. New stale-tour choice starts no inference; Stop remains enabled during head movement. |
| AC06 | Six public native/replay/legacy seed/reopen courses, unsupported custom harness and missing-workspace negative control; distinct linked replay generation, current permissions, counted omissions and no resend. |
| AC07 | Public Refresh fetches changed head: tour Continue old revision keeps actual workspace HEAD/process and readonly/network-off, pinned/observed bootstrap; New session runs at current HEAD separately. Existing three Agent stale/continued/anchored courses, core old-head proposal/captured-ID tests and full-each-turn bootstrap pass. |
| AC08 | Core/prompt/Qt distinguishing source/preparation/related/blocker/unknown/cap tests; installed Codex explains identical ranges with different confirmation without BPR source. Installed Claude explanation is owner-waived. |
| AC09 | Visible composer update/author Accept preserves confirmed source; normal tour/core text/order/new/split/retargeted and stable-target refusal checks; shared Qt/core/context reasons and publication/Ready authority remain separate. |
| AC10 | Six production composer courses, inspected offscreen native/replay/source/control rasters and new stale-tour rasters; exact macOS arm64 Codex0.164.0 AppServer/JSONL/native/Stop qualification. Installed Claude remains unverified under the narrow owner waiver. |
| AC11 | Actual equivalent Release Codex metrics; active/idle64KiB/+1, cache4eviction/exactresume, shared500/protected-save, workspace ownership/recovery and16GiB+1 refusal. Safe exact-owned ordinary cleanup plus protected-state preserve/refuse; no automatic retained-tree reclamation or unmeasured boundary claim. |
| AC12 | Live operator/product/architecture and RFC0011/0012/0092 amendment reconciled; immutable15 source files/provenance and historical Done verdicts unchanged. Focused gates and documentation pass; final independent critical review is required before terminal acceptance. |

### Remaining gate and preserved limits

The owner additionally requested “do an an rfc-review-critical before we closeout
the whole pack when we're done with the impl.” The coordinator must supply that
independent critical review for the whole scoped implementation, then a subsequent
engineering round must adjudicate findings, repair any in-scope failure and finish
the acceptance/upkeep. The current card remains In Progress; no approved finish
receipt, Auto-Verified status or independent approval is issued by M5.

Owner amendment `native-agent-sessions-claude-waiver-r1` applies only to remaining
installed Claude verification. Claude2.1.310 remains unverified; the authenticated
HTTP429 weekly-limit failure, zero tool activity/no usable answer and unreached
native/replay/resume/Stop/explanation/permission/usage courses keep their original
meaning. The superseded weekly-availability wizard remains stopped at consumed
cursor1 with its human acknowledgement unanswered. No new availability session,
automatic retry or inferred pass exists.

The selected installed scope is macOS arm64/Codex0.164.0 with the recorded Qt and
Release identities. Broader Windows/Linux/system-appearance, forced runtime
compaction, the262144-entry boundary, actual five-minute wall-clock expiry and the
owner's original warning sequence were not independently measured. They are not
invented passes or new waivers. Full bootstrap on every explicit send is the
implemented fallback when runtime framing survival is unknown. The required
bounded-output/cache/admission/refusal paths have direct evidence; automatic
retained-tree reclamation is not implemented. The RFC allows safe preservation
and refusal when cleanup cannot be proved. Exact-owned ordinary/replay cleanup
and recovery are tested separately from retained native ownership. No source
promise of automatic retained cleanup or unlimited memory is made.

Existing release/vault/crash/vulnerability/native-trust/full-removal requirements
remain outside this scope. Profile purge excludes shared updater/registry state
and is not uninstall. Reported Release metrics retain unavailable USD/model and
single-course limits; no fixed saving is asserted.

External continuation and reviewer packet:
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m5-handoff.md` and
`m5-critical-review-handoff.md`, with exact candidate, scoped path inventory,
commands, criterion audit and checkpoint beside them. Final documentation/board
checks and command quiescence are recorded in that external handoff.


## M5 critical-review repairs — follow-up review pending

Mandate `native-agent-sessions-r1`, assignment `accepted-scope`/BPR-1956,
owner `/root/native_sessions_critical_repairs`, segment
`accepted-scope-critical-repairs-3`. Adopted clean HEAD
`7dbc2853acc86635cd5488f1709cdd0a08049dd2`; immutable baseline
`bebea27edf44fd63a7308307227035bf33391c5c`. Product repair candidate
`c66ed2166501da03026301f80f1285800ac56e47` changes fourteen explicit C++ paths. The original independent
review and original external probes remain unmodified. These repairs and
self-review do not constitute the required independent follow-up approval.

| Finding | Reproduction before repair | Final distinguishing result |
|---|---|---|
| P1 AC05: completed Claude identities were lost at each new turn | Production loop wrote two questions and returned two successful OLD ANSWER terminals when the prior same-session assistant/result arrived after the next send. | Same original probe writes each question once; first success, second protocol_failure with no assistant answer. Production-loop tests also cover normal second answers, idless exact repeats, tool-only identities, a repeated result UUID after a fresh assistant, result-only late output and 4,096 identities plus one refusal without eviction. |
| P1 AC03: Shallow skipped every missing-object marker | A real retained sparse workspace validated after deletion of its selected src/main.cpp Git blob. | Same original probe validates before deletion and refuses after it. Original unselected holes remain allowed; new missing selected blob/tree, old records without trusted evidence and full-workspace missing-base refuse. Allowed tracked/untracked edits survive inspection and refusal. Private SQLite evidence survives restart and shares the session CAS transaction. |
| P2 AC08/09: Ready projection treated an empty source-reason list as eligibility | A valid imported-style Remote/Destination/Mapped unchanged-material stop without Original/ReaffirmedHead projected Ready=true/zero blockers, while complete refused incomplete. | Same original probe reports Ready=false/one blocker and no Ready action, matching core refusal. Shared core source_readiness preserves ready() semantics. Imported/remapped tests keep confirmed lines separate from explanation reaffirmation; actual author reaffirmation enables Ready. A valid-source invariant matrix and serialized snapshot regression compare the command and projection. |

The stronger sparse test exposed an additional incidental cache write: Git
pack-objects used the source cache as its Git directory and changed its pack
directory timestamp through temporary files. Sparse packing now runs in the
owned workspace with the existing quoted source-alternate environment. Final
real-Git inventory checks prove both source cache and linked clone unchanged.
A separate sparse ownership registry keeps that course independent of the
existing complete-workspace startup-recovery count.

Final frozen-source evidence is external under
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/`:

- `m5-critical-repair-final-probe-{frames,sparse,status}.log`: all three original
  probes distinguish the repaired behavior; frames has success then protocol
  failure, sparse has before=1/after=0, status has projected_ready=0/blockers=1.
- `m5-critical-repair-final-focused-public.log`: five ctests pass, including
  core_review_tour_destination, application_agent_protocol_loop,
  application_tour_agent_conversation, sqlite_agent_setup_storage and
  application_native_agent_sessions (six production native/replay courses).
- `m5-critical-repair-final-retained.log`: GIT_NATIVE_RETAINED_WORKSPACE_OK,
  including missing selected blob/tree, allowed holes/edits, source/clone
  inventories and unchanged complete-workspace/required-base/alternate recovery.
- `m5-critical-repair-core-check.log`: four core-only checks pass: core_review_tour,
  core_review_tour_destination, architecture_boundaries and
  test_network_isolation_manifest. Both preset builds pass.
- `m5-critical-repair-final-tidy.log`: six changed production units pass tidy22.
  All fourteen changed C++ files pass clang-format22.1.8 and git diff --check.

Intermediate evidence remains: wrong initial build target, corrected compiler
signature/test namespace errors, incomplete-line idle fixture, a fixture helper
used for nonidentity output, and the source-cache timestamp failure. The final
idle fixture uses a complete ignored line at exactly64KiB; its extra byte still
refuses before selecting another request. Partial idle framing stays with the
completed decoder. An early tidy sampled a file during formatting and failed
with a truncated parse; only the settled six-unit rerun supplies static evidence.

Frontend-design was loaded before the status work; no Qt/layout/palette change
was needed. Existing offscreen appearance evidence is reused, and final public
composition is functional fixture evidence. No owner desktop or personal profile
was accessed. No installed Claude/API/auth/inference call, retry or wizard was
made. The narrow Claude waiver, actual HTTP429/unreached courses, other-platform,
forced-compaction, wall-clock expiry, broader resource and cost limitations above
are unchanged. Debug timing remains nongating; no timing course was run or given
functional credit. Required independent critical follow-up is the next action,
not terminal RFC or card closeout. The durable current handoff is
`m5-critical-repair-handoff.md` beside the external logs.


## M5 UUID-less terminal correlation — fresh review pending

Same assignment and owner, mandate `native-agent-sessions-r1`, segment
`accepted-scope-critical-repairs-4`. Adopted clean HEAD
`569bbc04a55a2774a8726b6d1f0b45f301142ec7`; product repair
`1df298efb9d5ac7fad8eb8886f1eaf7e3e6618c2` changes seven explicit paths. The follow-up critical
review confirmed the prior sparse and Ready fixes, but found residual P1 AC05:
a fresh second-turn assistant fragment followed by the first turn's UUID-less
result still completed that second turn. The unchanged reviewer probe
`critical-followup-ouc5r5h3/uuidless_result_probe.py` reproduced two writes/two
successful terminals, with NEW TURN STILL WORKING as the false second answer.
`m5-uuidless-red.log` preserves that observation.

A UUID-less result has no terminal identity or turn correlation. Comparing its
text/hash with an earlier result would reject legitimate identical answers and
still cannot establish a new terminal's origin. The repair therefore does not
claim to correlate that shape on a reused stream. It accepts the sole turn on a
fresh transport, retires the process before releasing the caller or asking the
connection for another question, and retains the exact native handle/workspace.
The next explicit send uses the existing exact-ID resume path on a fresh process.
Retirement performs no inference, automatic retry or transcript replay. A
UUID-less terminal on an already-connected turn refuses, and a reported malformed
UUID refuses even on the first turn. UUID-bearing streams keep their existing
persistent route and bounded prior-identity fences. No upstream field is invented.

The production connection's retiring state becomes nonmatching before terminal
publication. Thus an immediately following explicit send replaces/joins the old
connection safely; it cannot queue a decision onto the exiting worker. One-shot
UUID-less responses remain supported. This compatibility qualification supersedes
the earlier implication that all accepted older envelopes permit process reuse.
It is not a waiver of late-frame fencing or an installed-Claude observation.

Final causal verification, with frozen source promoted unchanged:

- `m5-uuidless-focused-public.log`: application_agent_protocol_loop,
  qt_agent_launch_canary and application_native_agent_sessions pass (three ctests;
  real-process course20.84s, public six-course composition24.73s). Production loop
  cases cover fresh assistant plus old or fresh-but-uncorrelated UUID-less result,
  malformed UUIDs, retirement before caller release and zero next/send calls.
- The real-process legacy fixture completes three immediate explicit sends with
  exactly identical UUID-less terminal text, the same native handle/workspace and
  distinct processes. It refuses a resume that omits the exact handle. Modern
  fixture terminals now carry UUIDs; the existing same-process/native-memory,
  exact-resume, Stop, idle bound and cache courses remain passing. No installed
  Claude executable is invoked.
- `m5-uuidless-green.log` and `m5-uuidless-original-frames.log`: unchanged residual
  and original frame probes now report writes=1/terminals=1/first success only.
  The legacy stream retires before any second request is selected; it cannot
  produce the false second completion. A later explicit real launcher call is
  covered separately by the exact-resume course.
- `m5-uuidless-build2.log`, `m5-uuidless-tidy.log` and
  `m5-uuidless-format-final.log`: focused/public build, three production-unit
  tidy22 (including the native connection header through its launch owner),
  seven-file format22 and diff-check pass. The initial wrong build target is
  preserved in build1.log and supplies no evidence.

Prior sparse/Ready/core-only/SQLite/Git/architecture evidence, all other original
probe observations and the prior documentation gates remain unchanged and retain
their recorded candidates. This round reruns only causal protocol/real-process/
public checks plus documentation upkeep. No new visual/Qt surface, dependency,
remote mutation, owner-desktop access or installed Claude/API/auth/inference call
exists. The narrow Claude waiver, actual HTTP429/unreached courses and all other
recorded native/platform/resource/cost limitations remain explicit. Required fresh
independent critical follow-up remains pending; RFC Proposed/BPR-1956 In Progress.
Current external checkpoint/handoff: `m5-uuidless-checkpoint.json`,
`m5-uuidless-observation.json` and `m5-uuidless-handoff.md` under the same execution
root. No terminal acceptance is inferred from these engineering results.


## Final accepted scope and independent review reconciliation

Mandate `native-agent-sessions-r1`; milestone `accepted-scope`/BPR-1956.
Final product `1df298efb9d5ac7fad8eb8886f1eaf7e3e6618c2`; reviewed whole-scope revision
`ad81ec65c085cea6d81959cbf50fa6a65ef65702`; immutable RFC_BASE_SHA
`bebea27edf44fd63a7308307227035bf33391c5c`. Source and test inputs are unchanged
in this bookkeeping-only closeout. All five authored groups and all six issue
slices are accepted; the live RFC and index close as Auto-Verified. The adopted
plan remains source-only, with no remaining work, dependency or exit omitted.
Original pack/provenance and historical Done-card verdicts are preserved.

### Independent review receipt and disposition

The owner's required whole-pack critical review was performed by the distinct
read-only agent `/root/native_sessions_critical_review`, role
`rfc-reviewer-critical`, model `gpt-6-astra`, effort `xhigh`. The final report was
delivered through agent message and authoritatively relayed by the coordinator;
there is no reviewer-written report artifact. This ledger is the durable receipt,
not a claim that the implementing owner independently reviewed itself.

Final reviewer report at `ad81ec65c085cea6d81959cbf50fa6a65ef65702`:
“The residual AC05 finding is addressed. No additional actionable finding emerged.”
Blocking findings: none. Nonblocking findings: none.

| Reviewed finding | Repair and evidence-backed final disposition |
|---|---|
| P1 AC05 prior Claude frames completed another turn | `c66ed2166501da03026301f80f1285800ac56e47` retains bounded completed assistant/result identities across turns, including idless/tool-only cases and4096+1 refusal. Follow-up found the UUID-less residual below; the original failure is preserved and is not accepted as passing. |
| P1 AC03 newly missing required sparse objects were ignored | The same repair records original allowed missing objects under session CAS, then rejects newly missing required blobs/trees, unverifiable old sparse records and required complete-workspace base loss. Independent follow-up verified the original deletion probe after=0, preserving original holes/allowed edits. Real-Git source/linked-clone inventories remain unchanged. |
| P2 AC08/09 Ready projection disagreed with actual command | Shared core source_readiness now preserves actual ready() eligibility and separately reports missing reaffirmation. Independent follow-up verified projected_ready=0/blockers=1 for the valid imported/remapped case. Author reaffirmation, valid-source invariant matrix and serialized snapshot checks pass. |
| Residual P1 AC05 UUID-less result could complete a later turn | `1df298efb9d5ac7fad8eb8886f1eaf7e3e6618c2` accepts that uncorrelated shape only on a transport's sole native turn and retires before releasing its terminal. The next explicit question exact-resumes the same native handle/workspace in a fresh process; UUID-bearing streams retain persistence. Both unchanged reviewer frame probes now report writes=1/terminals=1/first success only. Real-process tests separately verify three immediate explicit UUID-less turns, identical terminal text, exact handle/workspace and distinct processes. |

The final reviewer traced retirement before complete/caller release, Stopping and
nonmatching state, submit refusal, worker join/replacement, fresh exact-handle
resume, and missing/malformed connected UUID refusal. The reviewer independently
ran protocol-loop, real-process canary and public composition:3/3 passed in43.41s;
DOCUMENTATION_OK and git diff --check passed. Prior follow-up verified sparse and
Ready probes. This coverage reconciles the original whole-scope review and all
follow-ups; it is not an all-platform or installed-Claude qualification.
All reviewer commands ended and its worktree remained unchanged.

### Final criterion and milestone judgment

The BPR-1956 checklist and acceptance records bind each of AC01–AC12 to the final
product and relevant direct evidence. M1–M4 evidence and the M5 stale-tour repair
remain applicable where unchanged. The critical repairs supersede earlier frame,
sparse-object and Ready claims with their reproduced negative controls and final
positive/refusal observations. Both accepted-scope exits are fulfilled: the full
criterion/source-amendment/public-path/documentation audit is complete, and all
remaining limits are explicit. The added critical-review requirement is supplied
and reconciled; no required selected-scope acceptance remains pending.

Reused exact engineering evidence includes M5's integration10/10 and final Qt/
public2/2, two inspected stale-tour offscreen windows, critical-repair focused5,
retained real-Git course, core-only4/builds, six-unit tidy22/fourteen-file format22,
and final UUID-less protocol/canary/public3/3 plus three-unit tidy22/seven-file
format22. Earlier67 documentation-script tests and unchanged resource/privacy/
authority/installed-Codex courses retain their actual revisions and limits.
Only the changed closeout bookkeeping receives a fresh documentation/board gate;
no optional product rerun, new feature, reviewer or approval gate is added.

### Limits preserved at acceptance

The owner amendment `native-agent-sessions-claude-waiver-r1` waives only remaining
installed Claude verification. Claude2.1.310 remains unverified: authenticated
HTTP429 weekly-limit failure, zero usable tool activity/answer, and unreached
native/replay/resume/Stop/explanation/permission/usage remain recorded failures or
unreached courses. No Claude request/authentication/inference/retry was made in
this closeout. The stopped superseded availability wizard's acknowledgement
remains unanswered; it is not a pending pack gate under the owner's amendment.

Qualified UUID-bearing streams preserve persistent native input; UUID-less
compatibility keeps native handle/workspace continuity through fresh processes,
not process reuse. That safety qualification and the actual supported Codex
AppServer/JSONL scope are explicit. Installed Codex0.164.0/macOS arm64 evidence is
unchanged; no broader Windows/Linux/system appearance, forced compaction, actual
five-minute expiry,262144-entry exhaustion, original historic all-stops warning
sequence, universal resource/cost saving or native package/trust result is
invented. Full bootstrap on every explicit send remains the fallback when framing
survival is unknown. Automatic retained-tree reclamation is not implemented:
protected state is preserved and admission refuses when safe cleanup is not
established; exact-owned ordinary/replay cleanup and retained recovery have their
own direct evidence. Debug timing is permanently nongating.

Existing release/vault/crash/vulnerability/native-trust/full-removal obligations
remain separate and unchanged. Profile purge excludes shared updater/registry
state and is not uninstall. This is acceptance of the authorized RFC scope with
its recorded owner amendment, not a release approval.

Final exact commits, documentation/board results, completed checkpoint and command
quiescence are in external
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m5-final-handoff.md`.
No server, writer, review loop, automatic retry or follow-on milestone is handed
off; no push is authorized or performed.
