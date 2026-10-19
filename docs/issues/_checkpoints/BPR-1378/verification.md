# BPR-1378 verification

Assignment `/root/tour_stop`, milestone `bpr1378-tour-stop/truthful-stop-progress`,
local main; mandate `owner-ready-backlog-20261018-r2`.
Source candidate: `4773277f981d429c29862e297c0e31c3d5e90335`. Reassessment baseline: `dd7671107`.

## Relevance and scope

The owner's older-card amendment was applied before repair. Current main still
passed an already-stopped token into verification and returned `conflict` when
those reads refused; the Stop fixture still discarded the returned result.
The strengthened fixture reproduced the defect. No later owner decision made
this behavior obsolete. The original two criteria remain unchanged.

This is a standalone defect, not parent RFC closeout. RFC-0041/42's immutable
RFC_BASE_SHA `3e12138ae8c603315b3d55c34872d7a4fc777a71` and owner-amended terminal
scope remain unchanged. BPR-1222's historical observations are preserved, as are
its live/native/human qualification gaps. Insights/RFC-BPR-0086 is excluded.
No live API, desktop, credential, provider protocol, schema or presentation change
was needed; existing public states and their CLI presentation are reused.

## Criterion evidence

| Criterion | Direct observation | Result |
|---|---|---|
| AC1: Stop between writes reports stopped/partial progress, not conflict; real drift stays conflict | Production `review_submission_session::publish_tour` returns staged/published/description-attention states after all five native writes, partial/published states after all three maintenance writes, and partial/description-attention/published states after all six compatibility writes. Records retain exact identities, patch/update counts and completion flags. Remote body drift during Stop still returns conflict; existing actor/head/source/contamination controls pass. | Pass |
| AC2: fixture asserts returned state at every stop point | Fourteen write-stop points now assert exact state and confirmed progress. Every one is reopened, reconciled read-only, and explicitly resumed to the exact original total without another create/submit. | Pass |

Additional cancellation rows stop initial observation (zero writes, unavailable),
pre-write verification after native create, and pre-write verification after a
maintenance edit. Prepared, untransmitted next steps stay durable; cancellation
during read-only verification also preserves the exact record. Resume never
repeats a confirmed effect. Existing unknown-response and strict-operation tests
continue to retain unknown witnesses instead of replaying writes.

## Negative controls and final verification

1. With only new native/maintenance assertions and the old production code,
   `staged_review_session` fails: "Stop reports confirmed staged, published or
   description progress, never a false conflict" (CTest exit 8). The public
   `tour_cli_publication` composition passes in the same early run.
2. After only the early cancellation return, native Stop rows pass and the new
   maintenance assertion fails (CTest exit 8): the old status helper still
   calls a partially applied maintenance plan ready. Returning `partial` for
   confirmed maintenance edits resolves it.
3. New verification-read cancellation rows fail against that intermediate
   candidate (CTest exit 8). Cancellation-aware refusal classification resolves
   the timing window without weakening real-drift or unknown-result controls.
4. Final candidate: `staged_review_session`, `staged_review_restart`,
   `tour_cli_publication`, `tour_cli_terminal_publication`: **4/4 pass**, 9.78 s.
   The process-death course covers 15 native and 9 maintenance boundaries,
   checks read-only restart, bounded Resume totals and SQLite integrity.
5. Scoped format: clang-format **22.1.8**, pass. Scoped clang-tidy **22.1.8**
   with warnings-as-errors, both changed translation units, pass; only
   non-user header warnings were suppressed. Scoped `git diff --check`, pass.
6. `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK`.

Commands used the owned `build/bpr1378-tests` graph: `cmake --preset dev
-B build/bpr1378-tests -DBPR_BUILD_TOUR_CLI=ON
-DBPR_BUILD_AGENT_INTEGRATIONS=OFF -DCMAKE_TEST_BUILD_DEPENDS=OFF`, then
`cmake --build build/bpr1378-tests --target test_staged_review_session
test_tour_publication_cli -j 6`; CTest selected the four exact names above.
The initial lean tour-cli graph omitted GitHub contract targets and the first
build refused its unknown target before execution; reconfiguration selected the
correct dev graph. No product change was used to fix build setup.

## Self-review, limits and cleanup

The coordinator returns only durable confirmed progress or an unknown witness
when stopped. Post-write readback still reconciles the effect before returning;
known body/actor/head/source drift retains conflict. No new state, schema,
authority, fallback, write or automatic Resume is added. The operator guide
records the corrected Stop semantics. Existing witness, operation scope,
child-authorization and one-submit boundaries are unchanged.

No independent, human or device acceptance is required by this card. No release
or package qualification is claimed. Raw non-secret logs stay outside Git at
`/tmp/bpr-closeout-20261018/stop-*.log`. The final handoff records command
quiescence and removal of the task-owned build tree; shared dependency caches
and concurrent work are preserved.
