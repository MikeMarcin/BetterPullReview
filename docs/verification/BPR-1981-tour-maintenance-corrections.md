# BPR-1981 published tour maintenance corrections

Assignment `maintenance-corrections`, mandate
`tour-destination-repairs-r1-maintenance-corrections`: the owner's post-review
authorization to repair both P2 findings in BPR-1979. Baseline
`69f745a9914428046f030a88617dbe68b7f2ca99`; verified repair candidate
`b5644f4a98cb3850ce50722262dad396d87cd5e4`. The original card's AC1–AC7 and its
historical acceptance remain intact; this follow-up corrects the two demonstrated
maintenance regressions. No capability, visual design or live-host scope changes.

## Source receipt and maintenance

Initial publication now records the frozen plan's source as each stop's confirmed
`Destination`. Replacement publication does the same. The immutable `Original`,
byte-bearing `Mapped` range, mapping classification and explanation remain intact.
Maintenance compares an author-selected source with that confirmed destination;
legacy records without one retain the original-source fallback. A later explicit
selection of the original range is still a retarget if the remote subject differs.
Provider verification reads that confirmed subject, independently of subsequent
local mapping. Local observed-change comparisons also use the published subject,
so a cold import of it does not invent a website source change.

The permanent repaired-tour composition first publishes ten native comments in
one COMMENT review with thirteen writes. It saves the receipt, checks both
repairs are no longer pending retargets, retains original and mapped byte
identities, then uses Preview again. With no changes, Publish is disabled and
there are no writes. An actual explanation edit previews and publishes one body
update to the same repaired discussion: fourteen writes total, ten comments,
one review creation/submission and no compatibility comments. All remote IDs,
original ranges and repaired mappings survive. The core course separately checks
legacy fallback, confirmed-source no-op planning, subsequent retargets and cold
import source equivalence.

## Comment identity namespaces

Maintenance keeps review and conversation observations separate and chooses the
collection using the retained remote kind for both stops and overview. Numeric
IDs are only searched within that namespace. Exact node, URL, actor, ownership,
reply, body, review association and line/file subject checks remain required.
Files stay in the review-comment namespace and retain their file subject check;
conversation comments have no review association or source subject.

The production-session fixture intentionally assigns issue comments IDs
601/602/603, overlapping review comments 601/602, while keeping distinct `IC_*`
and `PRRC_*` nodes and issue/discussion URLs. Every original mixed Stop/lost
response/restart course now runs with that overlap and subsequently maintains
both kinds. Eight negative controls refuse mismatched node, URL, actor,
namespace, source, file/line kind and review association with zero writes.
A separate imported conversation-overview course collides with review comment
601, rejects wrong overview node/URL, then updates its two review stops and
conversation overview while preserving their identities.

## Evidence and limits

Both permanent new regressions were built and run against the unmodified product
baseline. `staged_review_session` failed mixed maintenance; the repaired composed
course failed after its thirteen successful initial writes because the repairs
still needed destinations. Runtime evidence is external under
`/tmp/bpr-tour-maintenance-fixes`, including `regression-baseline.log`.

The first corrected run passed core update and repaired composition. Its session
course exposed a hardcoded initial write-count assumption in the new overview
control; the test now captures the confirmed prefix. LLVM22 tidy also required
an explicit default in the mismatch-control switch. Those failures are retained,
not counted as passing evidence. The subsequent session/restart/core run passed
4/4; final exact-candidate checks and commit identities are recorded in the
[durable checkpoint](../issues/_checkpoints/BPR-1981/handoff.md).

All effects use synthetic production-port fixtures and offscreen composition on
macOS arm64, Debug, Qt 6.12.0. No live remote writes, owner desktop operations,
new native appearance claim or human decision occurred. Existing independent
live/native qualification gaps remain outside this correction. No Debug timing
course is used as acceptance.

## Final acceptance

All records bind source/test candidate `b5644f4a98cb3850ce50722262dad396d87cd5e4`.

| Criterion | Result |
|---|---|
| AC1 | Pass: the composed repaired course completes the no-op second Preview and actual one-write text maintenance, retaining all source bytes and discussion identities. Core update covers legacy fallback and later retargets. |
| AC2 | Pass: production mixed Stop/lost-response/restart/maintenance courses run with overlapping IDs; eight stop mismatch and two overview mismatch controls refuse before writes. The colliding conversation overview updates last. Existing standalone-file composition passes with exact file subject checks. |
| AC3 | Pass: final nine-course integration, core-only checks and both architecture boundary checks, LLVM22 format/tidy, documentation gate and 67 documentation regressions. Both original baseline failures are retained as discriminating controls. |

Final commands and results:

- `cmake --build --preset dev --target test_staged_review_session test_review_tour_publication_public_path -j 6`: pass (`final-link.log`). The core destination/update targets were built in `fix-build.log` and `session-build-2.log`.
- `ctest --preset dev -R '^(staged_review_session|staged_review_restart|core_review_tour_update|core_review_tour_destination|application_review_tour_publication_(native|compat|repairs|mixed|file))$' --output-on-failure`: 9/9 pass, 24.05s (`final-integration.log`).
- `cmake --build --preset core-only --target test_review_tour_update test_review_tour_destination -j 4`: pass (`core-build.log`). `ctest --preset core-only -R '^(core_review_tour_update|core_review_tour_destination|architecture_boundaries|architecture_boundary_contract)$' --output-on-failure`: 4/4 pass, 207.65s (`core-only-final.log`). The negative architecture contract took 184.62s; no timing acceptance is inferred.
- LLVM22 `format_check.py`: all eight changed C++ paths pass (`format-final.log`). `tidy.py --warnings-as-errors`: all seven changed translation units pass, using the six unchanged successes in `tidy.log`, the corrected session in `session-tidy-2.log`, and final adapter in `adapter-tidy-final.log`.
- `python3 scripts/verify_documentation.py --root .`: pass (`docs.log`); `python3 tests/scripts/verify_documentation_tests.py`: 67/67 pass (`doc-tests.log`). Final tracker/documentation closeout is recorded in the checkpoint.

Self-review checked immutable original and blob identities, creation/route order,
confirmed versus selected subjects, legacy storage fallback, subsequent retargets,
separate provider namespaces, review membership, line/file kinds, exact source
verification, and zero-write mismatch refusal. No independent review was repeated;
the owner's correction authorization requested narrow rechecks. No required
correction remains. All owned commands exited and their results were consumed.
