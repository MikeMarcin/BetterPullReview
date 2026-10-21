# BPR-1673: command-owned correctness qualification

Mandate `release-30min-portfolio-r1`; milestone `command-owned-correctness` in the
[portfolio plan](../rfcs/plans/release-qualification-portfolio/milestone-plan.json).
The [design](../development/release-qualification-portfolio.md) records the owner’s
thirty-minute goal and the preserved acceptance boundaries. RFC-BPR-0038 remains open.

## Candidate and scope

Engineering candidate `a06e5bf3ee24f55e69fc8234c2aae6df6b8ade8c`, based on
`a1a91fa68fb42ce1138ec8cd3ffe72f4665996f3`. Activation is Windows Debug with the
standard tests-on Qt/agent feature set and checked MSVC iterators. Custom diagnostic
macro flags, unknown features, optimized configurations, unknown commands and stale
audit inputs retain exclusivity. macOS/Linux activation awaits focused native proof.

The registry accounts for 56 commands across workbench, diff-workbench, composed
Overview summary, source-find and review submission. Forty-six are command-owned
correctness courses; ten retain their existing exclusive status. Together with 31
prior declarations, the candidate Windows manifest contains 77 isolated courses.
The complete current manifest has 786 rows, including one new registry contract and
one pre-existing main addition beyond BPR-1670’s historical 784-row graph. No row is
removed. The earlier native full result remains scoped to its original source and
accepted recovery; this milestone does not claim a fresh full-main result.

Windows workbench inventory: 44 invocations, 481 ordered case/render-helper entries,
143 distinct names. macOS/Linux: 43/479/141. The full default keeps all 115 calls at
1.0, 1.25 and 2.0. All 193 direct call occurrences in the original `main` retain their
order, and every test function body is byte-identical to the base. The common source
prefix SHA-256 is `e261a7fb349bb55092affc1975c719b0f302e3acc946ad05be44f2eefa7b45a3`.

Each execution still has its own QApplication and original ordered group; fresh
widget/profile/store lifetimes, restart groups, work counts, readiness deadlines,
thresholds and geometry/input/focus assertions remain. This milestone performs no
case consolidation or conditional verdict reuse. `--list-cases` inspects the real
selector path and cannot print a passing execution marker. Unknown/multiple selectors
fail. Optional tracing records entry and successful return for each direct call.

## Resource and configuration audit

A named declaration binds exact logical argv, scale and output environment, test and
local-helper/composition source hashes, Debug and standard feature options. CMake
reconfigures when those inputs change and defaults to serial when the audit drifts.
The runner independently checks the emitted command tail, configuration label,
source inputs, render paths and required environment unsets. Existing locks,
fixtures, containment, failure expressions and timeouts survive.

The default workbench’s render helpers use its scale-specific `BPR_QT_RENDER_PATH`
basename. The fixed `ribbon-settings.png` writer belongs to `--center-ribbon-width`,
whose registered rows request no render path. Optional sheet/tour/agent/Home and URL
capture roots are unset for admitted commands; a developer’s inherited render root
cannot become shared qualification output. Diff-workbench’s optional capture roots
receive the same treatment. Composed summary/source-find output directories and
review-submission output arguments remain distinct per row. Their private profile
roots and lifecycle groups are unchanged. Resource locks name each actual output
namespace; sharing a source binary or build directory alone is not a collision.

The 100 ms drawer assertion remains in the exclusive `--conversation-reveal` course.
Thread-frame sampling, native geometry/menu courses and the existing explicit badge
exclusion remain exclusive. Default large-file timings remain factual measurements,
with every existing work-count and geometry assertion active. A Debug declaration
cannot be inherited by a Release course with additional timing/memory obligations.

## Verification checkpoint

The five native targets built successfully: initial affected warm build 164.04 s;
final incremental confirmation 29.23 s. These are distinct from a cold bootstrap.
Actual source tripwire, documentation structure and contracts, allocation contracts,
13 runner controls and seven registry controls pass. The real native case-list
inspection passes all 44/481 occurrences and observes `NDEBUG=0` with
`_ITERATOR_DEBUG_LEVEL=2`. Source architecture and clang-format 22 checks pass.

Controls reject missing/duplicate/extra/reordered/zero case credit; unknown commands;
argv, source/helper, feature, scale and output drift; missing optional-root unsets;
QPA overrides; and Debug declarations applied to Release/RelWithDebInfo/unknown
configurations in both single- and multi-configuration CMake generators. The
actual manifest inspection initially exposed a multiset-construction error, which
was corrected and covered by the complete 44-row control before costly cases ran.

Pending: exact-binary four-command serial/four-slot contrast; the remaining 42 audited
correctness rows; the retained exclusive drawer course; native selector/failure/scale
controls; image/output and binary identity inspection; final integration and board
acceptance. No timing improvement or milestone completion is claimed at this checkpoint.

## Durable evidence and ownership

Private raw evidence is retained under the task’s ignored
`.mask/sessions/release-qualification/m1/` and the transferred native
`C:/Users/mikem/bpr1670-suite/evidence/m1*` roots. Preparation failures and the
quiet-host refusal remain separate raw results. The first source/build handoff starts
at 2026-10-21 01:43:02 UTC; the matched serial course starts at 02:05:59 UTC after the
quiet-host admission. Preparation, repair and waiting remain in the implementation
ledger; future release measurements still begin at their actual request.

The old native recovery source has the preserved `bpr1670-preserved-recovery` ref.
Published v0.3.1 and its frozen producers are untouched. Main and chat-primary MASK
working-file hashes/index state were captured before integration and must survive.
Only exact task-owned commands/tasks may be stopped or removed. The coordinator
receives the final lease, candidate, evidence and pending frontier in
`/tmp/bpr-release-portfolio/m1-handoff.md`.
