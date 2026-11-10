# BPR-2037: Native retention in the Codex finding course

Mandate `release53-cleanup-accounting-r1` owns one test correction following
frozen53 `ac49b0b2f7192f3ea114b898bcd81e60467cf595`. Source
`f70ba9db355a1a6ad65320a54361672846c7b86b` landed main as
`042a5458780e841141637f810e889e6c86340a98`. Only
`tests/end_to_end/agent_finding_composition_public_path.cpp` changes code.
The final TU SHA256 is
`4ef67808e6ba27f08bd4e74e03e82c84240307d20a1fee969a4cfaf544fb0ba6`.

## Cause and preserved failure

The original full Mac Release course761 fails46.61s at
`WorkspaceCleanups == 1`, with one process, `Finished: success` and a quiescent
host. Its numeric cleanup count was not reported. The raw53 failure remains
failed at `candidate53/macos-headless-qualification-r1/release.log` in the
external release run; it is not rewritten or passed by this correction.

Unlike the other finding variants' built-in fixture profiles, codex-found adds
its Codex entry through Settings. The existing launch composition gives an added
entry its native identity and scope. The host stores the native session and
sets `RetainWorkspace` before process execution. Successful retained cleanup
retires the invocation without deleting the workspace or incrementing its
cleanup counter. RFC-BPR-0096 and BPR-1974 require this shared tree to survive
turn/session/window closure; the existing native host test explicitly expects
zero cleanups with quiescence. The obsolete expectation is in the test.

The corrected course requires one process and workspace, zero cleanups and
cleanup failures, the concrete retained directory and regular ownership marker,
and exactly one selected native history row with one turn. It refreshes History
only after the original behavior assertions have judged their evidence; that
read submits no question. Built-in variants retain exactly one cleanup. All121
original named `contract.require` call sites and their order remain unchanged,
as do finding/draft/MCP/zero-publication assertions,45s inner budgets and the
120s CTest watchdog.

## Focused native proof

A private sole-TU compile and link use the actual normal53 Mac arm64 Release
flags and immutable dependency objects. Compilation changes only source and
object output paths; linking changes only that object and executable output.
The two original registered rows are projected with only the executable
substituted. Programmatic comparison of the configured graph confirms every
other command argument and every property, including working directory,
network-denial wrapper, environment and modifications, match exactly. The
original offscreen/foreground-disabled/trace outer environment is preserved.
This is focused engineering proof, not release qualification or an original53
PASS.

| Check | Actual result |
|---|---|
| Sole TU / private link | 0 /6.651s;0 /0.537s |
| Scoped LLVM22 tidy, warnings as errors | 0 /18.875s |
| Corrected codex-found, once | CTest0 /2.79335s; native child0 |
| Ordinary positive, once | CTest0 /1.95826s; native child0; unchanged cleanup-one assertion |
| Protected inputs | All129 exact before/after, including all111 prior protected53 inputs |
| Producer | HEADac49 unchanged; tracked clean; no writes |
| Commands | Terminal; observed owned process identities have no surviving matches |

The corrected native course reports actual counts:
`process=1 workspaces=1 cleanups=0 cleanup-failures=0
owned-retained-workspace=1 selected-native-history-one-turn=1
old-cleanups-is-one=0`, followed by its original PASS marker and child exit0.
Together with the retained original failure, this distinguishes the old oracle
without rerunning an unchanged failing course.

Raw manifests, compiler/link argv, configured row comparisons, stdout/stderr,
JUnit, input/image hashes and terminal receipt are retained under
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer53-agent-finding-cleanup-r1/native-proof-r1`.
Receipt SHA256:
`d34b429037514ac261ebd6ae1658b7be220044365f58215430001506bca5c0a3`.
Private image SHA256:
`ba18bea41b5b6dbf6d90575e210c567b4b468557145173855b926b2f36dfa83a`.
Format22, diff checks and documentation pass. Mac native lease was returned
immediately after proof. No PC work, desktop access or product change occurred.

## Bounded sibling audit and limits

The requested read-only literal `WorkspaceCleanups` audit covers four frozen53
TUs. The finding course's multi-turn helper is used only by built-in workshop
and guidance variants. In agent-chat, the shared positive scalar contains a
cleanup-one term, but the added custom-agent branch uses its own independent
predicate and returns without consuming that scalar. In agent-invocation,
native/replay/tour variants return before the later built-in external-terminal
cleanup checks. The host unit check is a non-native external-terminal retry.
No additional reachable stale native cleanup gate was found in this bounded
search; no new source edits, probes or issue were inferred from it.

This does not establish every alternative lifecycle predicate or any full
successor release result. Windows/Linux focused evidence for this exact change
is not claimed. Fresh common-source native qualification remains required;
the separate53 SQLite timeout and every other retained release failure keep
their original status.
