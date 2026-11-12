# BPR-2134: settle the publication read before current-head inline setup

## Observed failure and causal boundary

Frozen Build60 Linux Release `application_review_submission_composition_public_path`
failed after the last RequestChanges acknowledgement: the original 20-second
line-selection predicate ended with row0 selected but no active tab, comparison,
result or pending work. Its original full output remains unchanged in candidate60.
This observation alone does not establish an application queue defect.

The fixture's `submitted()` helper observes publication acknowledgement and a new
submission draft. Publication also calls `reread_after_publication()`, which owes
and queues a window read. `window_read_progress().Reading` explicitly includes
that owed/queued read and a pending pull-request read. An acknowledgement is not
a settled post-publication snapshot. Accepting the new epoch can retire a line
selected against the earlier comparison; Comment correctly requires current-head
material and a live selection.

The bounded R4 control held the existing per-window read terminal around the last
RequestChanges and released it immediately after the original file gesture.
Before that gesture, files were enabled, Navigation was ready, an accepted
comparison existed, and the read was pending (seven held terminal entries).
The gesture opened an active tab immediately and the original line predicate
succeeded. After release, the original `Comment is offered on the current-head
line` assertion failed: the complete course exited unsuccessfully after24.69s.
That is a concrete counterexample to using acknowledgement alone for this setup.
It refutes a disabled-view diagnosis in that controlled state. It is not claimed
as a reproduction of the original exact empty-tab terminal state.

Earlier single controlled cases are retained separately. R2 showed a pending
comparison with admitted Navigation can converge. R3 cleared only the disposable
cache before starting and delayed fixture Git reads: Navigation was ready with
no accepted comparison before the gesture; the normal application subsequently
opened the exact line. Neither supports changing product queuing.

## Correction and regression

Only `tests/end_to_end/review_submission_composition_public_path.cpp` changes.
Before choosing a file, the fixture now waits for a snapshot different from the
one before publication, no owed/queued/pending read, and admitted Navigation.
It does **not** require a comparison before the first file gesture.

Inside the existing single 20-second line predicate, it additionally requires a
ready epoch, an available nonpending accepted comparison ending at the accepted
head, and the same comparison/file actually displayed by the public surface.
It then calls the original right-side line29 selection. The existing Comment
availability, inline draft, stale-selection/summary refusal, publication and all
remaining assertions follow unchanged.

The final committed course deterministically holds the real publication read,
requires `Reading == true` and rejects setup readiness while held, releases it,
and traverses the complete original tail. The hold has no elapsed-time success
oracle. No product code, dispatch queue, release configuration, compiled normal
library, source-owner pin, budget or assertion is relaxed.

## Exact source and native proof

- Frozen source: `6efba00e1afbf874c88c7e5de2623a33528ff36f`.
- Private working candidate: `ea69e2d756b9e70b97bad1ebfcfe0d4dfa6a1c2c`.
- Frozen-compatible **test-only** commit: `408bbe40366116b10afd092a6bdc59aaf6fbd303`.
- Canonical Main equivalent: `e4e7655fe17a77595986f6d00d92179be1e051f9`.
- Stable patch ID on both: `e118714089eef8eadefe7513c0df2d5ecd358ee2`.
- Final candidate TU SHA-256: `53f66e7f4cc64238b9f30ab2c704a9f62ba2d11c63e64d5f5fd5342b8414a1af`.

One complete final course per configuration ran serially on the native Ubuntu26.04
WSL2 UID1000 producer. Each compiled only that test TU with the actual stock
GCC15.2 compile argv, then replaced its one object in the original link order.
All other libraries, app/helper images, fixtures and compiler/configuration inputs
remained the compiled60 inputs. Stock CTest sandbox, environment modifications,
working directory and assertions were preserved. Only the private executable and
output PNG tokens changed. Actual configured watchdogs were Release90s and
Debug600s; both kept the original functional20s predicate.

| Configuration | Complete course | Native supervised elapsed | Protected inputs | Remaining owned forest |
| --- | --- | --- | --- | --- |
| Release | PASS, exit0 | 14.512s | 1816 exact | empty |
| Debug | PASS, exit0 | 17.756s | 1803 exact | empty |

Both reached `review-submission-composition:pass` and `TEST_RUNNER_CHILD_EXIT:0`.
The real held-read negative and every later original assertion therefore ran.
All normal source/build/library/app/helper/input seals and final candidate bytes
matched; native source remained clean6ef. The intentional sole new compile
input is the existing frozen `src/core/include/bpr/core/hex.hpp`, used to compare
the accepted right commit with the accepted head; there are no removed inputs.
Its bytes were sealed, not replaced. No build/output went into a producer tree.

External evidence root:
`/Users/jaeger/.codex/engineering-runs/bpr60-workspace-fixture-r1/linux-blockers-r1`.

- `submission-final-release-r2/raw/receipt.json` SHA-256: `e4c1cd6f842d593d78de1008101f0563a5cf4ba291905c3ceb47bb22ff9aa946`.
- `submission-final-debug-r2/raw/receipt.json` SHA-256: `4caa434cdc31c32865c8005d4d73703ce803aa2e6fae060b88dd4de284050fb2`.
- `submission-native-r4/raw/receipt.json` SHA-256: `6a9d630b142627adc2180529d6267d24b949fba8f01b37a8a18326cc44ec8d63`.

Raw sibling logs, JUnit, argv, dependency sets, process identities and seals remain.
The earlier warm pre-gesture-comparison candidate's Release pass is historical
only. Its unused Release course was resumed after recording an intentional header
dependency; no course was retried. Its Debug preflight refused before compilation
because the planned row had90s while actual Debug has600s. Both orchestration
refusals (attempts0) remain intact. Final source changed to remove that unnecessary
pre-gesture comparison dependency, and ran once per configuration as above.

## Qualification scope and remaining work

This is a fixture correction and functional evidence for unchanged60 composition;
it is not a product bug fix, latency waiver, blanket native acceptance or claim
that every early-navigation ordering was reproduced. No Mac or Windows native
course of this new fixture TU was run; their existing60 product evidence is
unchanged, and the coordinator owns any explicit evidence bridge. Original Linux
full-suite failures are retained. Release timing, native security/trust, signing,
packaging and stable publication remain the coordinator's obligations.
