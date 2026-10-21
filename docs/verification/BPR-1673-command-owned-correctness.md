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

The exact-binary matched pilot executes the three complete default workbench scale
courses and diff-workbench at scale 2.0. All four commands pass in both schedules,
with 345 expected workbench entries entering and returning in each execution.

| Schedule | Wall seconds | Summed test seconds | Actual maximum concurrent commands |
|---|---:|---:|---:|
| Serial | 710.031 | 709.915 | 1 |
| Four slots | 220.079 | 729.679 | 4 |

The matched wall reduction is 489.951 seconds (69.0%); summed work rises 2.78%.
This measures scheduling on unchanged binaries, with setup/teardown inside CTest.
It is neither a source-change comparison nor a full-suite/release estimate.

The remaining 42 admitted correctness rows pass in 284.139 seconds wall /
1134.065 summed seconds, with maximum concurrency four and all 101 expected
workbench entries complete. The retained exclusive conversation-reveal course
passes alone in 20.884 seconds wall / 20.774 summed seconds, with all 16 entries
complete and its original 100 ms assertion active. Native unknown-selector,
multiple-selector, duplicate-list and wrong-scale controls each exit 1 without a
passing marker; the wrong-scale course reports its actual failed assertion.
All five binary hashes and clean source identity remain unchanged. Poisoned optional
inherited output roots contain only their original sentinel.

The process-start observer began at 02:09:09 UTC, after the serial admission at
02:05:59 UTC. Its initial snapshot records the serial CTest and two pre-existing WSL
shells; all subsequent observed build/test starts belong to the named courses.
The prior quiet-host refusal is retained. This is scoped process-start evidence,
not a claim of continuous whole-host idleness before observation. Observer cleanup
reported a null background-job ID after unsubscribing; the observer process exited,
and its raw observation log survives. Its exit is not a qualification result.

The pilot image sets have 661 equal names and dimensions; 660 byte hashes match.
The exact scale-2 default row was repeated serially in 214.934 seconds, passed all
115 entries on the same binary, and recovered the original serial PNG hash. The
remaining image is `workbench-window-200-navigation-labels-selected-902.png`,
1804 × 1640 pixels. Its entire delta is 258 pixels in the half-open box
`(1128, 1326)–(1138, 1354)`, at the right edge of the lower horizontal scrollbar
thumb. Navigation text, diff content and every other pixel are identical.
Existing Qt scrollbar/layout timing variation is an inference; its precise cause
is unproved. This is not evidence of a shared-output overwrite: each of the four
pilot commands has a distinct explicit render basename, the source audit finds
no second reachable writer to this path, and the poisoned inherited roots stay
empty. Both artifacts retain complete content and the unchanged geometry, raster
spacing and scale assertions. Raw pixels, crops, hashes and the exact-row repeat
are retained without claiming universal pixel identity.

The engineering candidate was safely merged into main `e9023554e` and chat primary
`7591db7ff`. Both original dirty MASK declaration hashes are unchanged, and the
unrelated main `scripts/capture_promo_assets.py` remains untracked and untouched.
Final evidence/board upkeep is integrated separately.

## Durable evidence and ownership

Private raw evidence is retained under the task’s ignored
`.mask/sessions/release-qualification/m1/` and the transferred native
`C:/Users/mikem/bpr1670-suite/evidence/m1*` roots. Preparation failures and the
quiet-host refusal remain separate raw results. The first source/build handoff starts
at 2026-10-21 01:43:02 UTC; the matched serial course starts at 02:05:59 UTC after the
quiet-host admission. That native preparation/repair/wait interval is 22 min 56 s;
it is not excluded from the implementation ledger or represented as a release run.
The four retained preparation failures were a mistyped external allocation-control
path, a CMake resource-name dot rejection, Windows UTF-8/path handling in the new
registry control, and the case-list multiset bug. Each failed phase and later repair
has its own child exit/source/elapsed record. Successful unchanged preflight inputs
are explicitly marked reused across preparation attempts. The observer cleanup
failure and bounded image repeat are retained as well. Future release measurements
still begin at their actual request.

At 02:31:39 UTC the final native command audit found no task-owned processes and
confirmed the observer had exited. All eight exact task actions were inspected
inactive and then unregistered. The retained archive contains 121 entries (758787
bytes), passes ZIP integrity, and has SHA-256
`36e7f3dcbe9e8b45a5c0060df5c8df3d3446b12ebc14a7c0074bc815a83a6fe2`.

The old native recovery source has the preserved `bpr1670-preserved-recovery` ref.
Published v0.3.1 and its frozen producers are untouched. Main and chat-primary MASK
working-file hashes/index state were captured before integration and must survive.
Only exact task-owned commands/tasks may be stopped or removed. The coordinator
receives the final lease, candidate, evidence and pending frontier in
`/tmp/bpr-release-portfolio/m1-handoff.md`.


## Evidence identities and continuation

The complete matched/focused result SHA-256 is
`3838234f8e435e6a9f219ae309906792631fa19416744d031d3a7e5d447ca4db`;
the five-binary identity receipt SHA-256 is
`17a60f33995cbbe1efc845a95bfb1d6a35717f405b7a9edc925ffda8dd53b1c3`.
The native workbench image is
`fbdb5e7d83131fa0110f911dff511c70e3d280e4fb34345a4fc9ec2731d1d93a`.

M2 reuses `C:/Users/mikem/bpr1670-suite/repo` at clean source `a06e5bf3e` and
its warm `build/windows-dev` multi-configuration tree. Only the five affected Debug
targets have current-candidate build evidence; prior other binaries and tests-off
composition caches retain their earlier source scope. Qt is 6.12.0, MSVC comes
from Visual Studio 18 Community, CMake/Python from their existing Program Files
installations, and the owned build cap is eight. Fixture paths still refer to this
native source root. Preserve the source/build tree, old recovery ref and raw evidence.

BPR-1674 is the next fresh milestone. Its bounded tests-on Release discriminator
and explicit Debug residual precede any route adoption; complete input closure and
the eight-method composition matrix precede conditional verdict/build reuse. M1
does not reduce the historical shared-PC work floor or prove a thirty-minute full
release. macOS/Linux activation and stabilized producer timing remain deferred.
