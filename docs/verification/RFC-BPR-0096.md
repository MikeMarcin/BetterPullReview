# RFC-BPR-0096 verification

Mandate `native-agent-sessions-r1` implements the reviewed pack. Immutable
RFC_BASE_SHA: `bebea27edf44fd63a7308307227035bf33391c5c`. Import `819ff2708`;
BPR-1951 claim `f8eaca5be`. Product candidate `26fdce665d7ff09f9ecd52e0df6765ce569b21ff` (the tested source tree promoted unchanged). The [live plan](../rfcs/plans/native-agent-sessions-v1/milestone-plan.json)
retains all five milestones. This ledger records completed first-milestone functional
engineering; it does not accept the whole RFC or later tour/history/replay work.

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
was performed. Remaining tour integration, New session/history controls, replay
migration, tour state context, Release usage comparison and full RFC acceptance
belong to the dependent milestones. Native Windows/Linux acceptance is not implied.
