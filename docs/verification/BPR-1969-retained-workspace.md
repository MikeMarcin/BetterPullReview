# BPR-1969 retained-workspace continuation

Mandate `retained-workspace-continuation-r1`, implementer
`/root/retained_workspace_fix`, branch `main`, immutable RFC_BASE_SHA
`7e74220335d90d7520a566df529742f676a3691f`. All seven supplied ACs are selected
in the adopted `bpr-1969-retained-workspace` plan, milestone
`bounded-retained-continuation`. Import `02a3c659d`, claim `63dd864ef`, product
`ea290c6393fc16cccafc0526f117db6bd3f1f44b`. The original supplied card is retained byte-for-byte under
`docs/rfcs/assets/bpr-1969-retained-workspace/`. No new RFC, requirement waiver,
remote mutation or live owner-desktop access was introduced.

## Result and criterion evidence

Complete retained inspection checks the head commit/root tree and, when recorded,
the required base commit/root tree. One `cat-file --batch-check=%(objecttype)`
request names at most four objects, admits 1 KiB stdout and has a 30-second process
deadline. It does not enumerate history. It verifies these pinned objects, not all
historical trees/blobs or every checkout blob. Existing ownership, journal phase,
HEAD, no-alternates and sparse missing-object allowlist checks remain enforced.
The files and transcript survive refusal; no replacement workspace is created.

| Criterion | Direct observation and judgment |
|---|---|
| AC1 | The six-course production native-session wrapper passes for Claude stream-json and Codex App Server stubs. In Tour/Problem/Solution/stop scopes, the course verifies a newly created non-shallow workspace, sends a second turn, and reads the tool-observation reply in that same side chat with process continuity. The real Git contract separately pins the complete inspector's fixed command against a runner whose old `rev-list` branch would emit 65 MiB. Together these distinguish the reported history-output overflow without manufacturing millions of objects. Pass by composition plus command invariance; the owner's private repository was not accessed or rerun. |
| AC2 | The production runner's captured argv is `cat-file --batch-check=%(objecttype)`, with exactly the named head/base commit/tree input. No `rev-list` is invoked for the non-shallow check. Output is four type lines on success with a 1 KiB cap and fixed deadline. The unchanged sparse course keeps its 64 MiB walk/allowlist contract. Pass; no universal latency measurement is claimed. |
| AC3 | Real Git retained-workspace tests reject an absent required base. A head-only pack retains the head commit while removing its root tree, with no required base in that control: inspection returns `object_unavailable`. Host injected missing-object failures and the composed ownership-marker control both refuse with zero new process/launcher invocations, no rematerialization or cleanup. Pass. |
| AC4 | Host tests preserve the safe workspace reason and assert exactly one sanitized `agent.workspace` terminal with `action=native_continuation`. Missing objects, output limits, time limits and unverifiable ownership have distinct typed reasons/stable failure names. Cache-actor tests carry those reasons and its own wait deadline through to the invocation host. The composed tour refusal visibly names the retained workspace, with no private-history sentence and zero launcher calls. Anonymized and identifying diagnostic-export tests retain the known summary while omitting a private-handle control. Pass. |
| AC5 | Host fault rows separately fail native `select`, pre-send `store`, `append`, and post-send `store`; each remains `private_storage_failure`, with the expected submitted/not-submitted boundary. Pass. |
| AC6 | `ai-agent-boundary.md` documents the bounded pinned-object guarantee and truthful refusal/diagnostic contract. BPR-1951's historical acceptance is preserved with a linked follow-up description. Documentation gate and 67 documentation-script tests pass. Pass. |
| AC7 | The required Git, invocation, native-session, Qt side-chat and architecture courses pass, alongside focused epoch-reason, diagnostic-export, protocol and core-only evidence below. Pass on macOS arm64; no new Windows/Linux native qualification is asserted. |

## Verification

The tested relevant source contents are the product revision above. Tests built
before that commit used those same contents; subsequent changes are bookkeeping.
The final architecture passes read that committed source. Build products, synthetic
fixture output and renders remain external, never tracked.

- `git_process_contract`: pass, 25.66 s; includes the real retained/sparse checks
  and history-size command adversary. Direct `--native-retained-workspace`: pass.
- `application_agent_invocation_host`, `application_agent_protocol_loop`,
  `sqlite_agent_setup_storage`, `qt_agent_launch_canary`,
  `application_release_maintenance`: 5/5 pass, 27.91 s total. This includes native
  continuity/storage and both diagnostic export policies.
- Final `application_epoch_host`, `qt_tour_side_chat`,
  `application_native_agent_sessions`: 3/3 pass, 32.99 s total (1.93/4.93/26.10 s).
  The last course runs native first/reopen, tour first/reopen and transcript replay
  first/reopen through the real application composition and isolated stub agents.
- `core_operation_diagnosis` and `application_operation_diagnostics`: pass.
  Dev and core-only `architecture_boundaries`, plus core-only diagnostics: pass.
- Clang-format 22.1.8 checks all 22 touched C++ files. Scoped clang-tidy 22 passes
  all 11 changed production translation units and a final recheck of the three
  refined units; only dependency/non-user diagnostics are suppressed.
- `python3 scripts/verify_documentation.py --root .`: DOCUMENTATION_OK.
  `python3 tests/scripts/verify_documentation_tests.py`: 67/67 pass.
  `git diff --check`: pass.

Frontend-design and its desktop reference were loaded for the existing failure
surface. The isolated six-course command was also run with an external evidence
directory. `renders/native-tour-refusal.png` was inspected: the earlier follow-up
reply and the safe workspace refusal appear in the actual tour chat. This is an
offscreen functional/render check, not target-native system-appearance acceptance.
No layout, icons or controls were added, and no human judgment criterion is open.

## Failures retained and scope limits

The first integration attempt passed five courses but failed two new fixture
checks: the fake Git fault was enabled during bootstrap, and the tour course
read the Agent-tab-only workspace evidence path. A subsequent tour assertion used
the data root instead of its owned sibling workspace neighbourhood. These fixture
mistakes were corrected; the final Git and composed courses pass. Original failing
logs (`integration.log`, `native-correction.log`) are preserved. Several build
commands named nonexistent targets and were corrected; no failed command is
reported as a pass. Debug timing was not an acceptance gate; no dedicated timing
course or optimized Release/resource qualification was claimed.

The actual 4.4-million-object private repository was not read or benchmarked. The
large-history claim is the fixed command/input/output bound combined with the
composed second reply, as the supplied test instructions permit. Materialization
cost, 16 GiB/262144-entry retention admission, default shallow policy, installed
Claude/Codex behavior, Windows/Linux native behavior and release/trust qualification
are unchanged and outside this defect. Full historical object closure is deliberately
not a continuation precondition for complete workspaces.

## Durable checkpoint and ownership

External durable state: `/Users/jaeger/.codex/tmp/bpr-1969-retained-workspace/`.
It contains resolved `execution-selection.json`, `plan-binding.json`,
`checkpoint.json`, acceptance/observation records, source/path identities, command
logs and `handoff.md`. The stock global checkpoint bind/discovery refuses the
unrelated `release-v041` plan's existing `amendments` field. Explicit-plan binding
succeeds with all seven obligations and three exits. The checkpoint uses that
binding plus the official repository-state helper; this documented fallback does
not modify historical plans or claim successful global discovery. The stock
checkpoint check evaluates the final observation directly against the adopted plan.

The complete selected milestone is accepted, with no required human/native or
independent decision pending. Self-review covered the touched control flow,
reason propagation, safe text/log boundaries, cancellation, sparse preservation
and refusal-before-dispatch. This is not a claim of independent review. No review
server or protected attended candidate exists. All task-owned build/test/tidy
commands finish before handoff; temporary test roots are removed by their owning
courses. No push or remote mutation was performed. Final bookkeeping revision,
checkpoint check and command-quiescence receipt are in the external handoff.
