# RFC-BPR-0077 verification ledger

Mandate: `find-and-find-in-files-v1-r1`; run `find-and-find-in-files-v1`,
segment `coordinator-implementer-001`. Immutable RFC_BASE_SHA:
`aa3d71fe470627e1e78488eb51a9114a788f33bd`. The
[plan](../rfcs/plans/find-and-find-in-files-v1/milestone-plan.json) retains all
five outcomes; the final [[BPR-1424]] card owns the complete original acceptance.

## M1 candidate and scope

[[BPR-1419]] supplies the compact deterministic session and bounded Qt-free
host lifecycle. Product commit `34256fb55` is integrated at
`7efc8c2d547bc74208069f9e7331e0f0253bc107` after adopting exact main
`bc4b4aafe6f51020aa469c519e936d616e124e89`. The
[refresh record](../issues/_checkpoints/find-and-find-in-files-v1/m1-integration-refresh.json)
preserves the initial historical-counter refusal and its integrator-authored
reconciliation. No Find source changed in the merge, and no main product,
counter, index or reservation was authored by this lane.

The [source search contract](../architecture/source-search.md) describes the
implemented boundary. Core state is trivially copyable and measured at 1,528
bytes on this Apple Silicon build, within its compile-time 2 KiB ceiling.
Query text and work packets remain with the host: at most one active effect and
one latest replacement, plus at most 64 deferred ingress operations. Matcher,
results, previews and corpus/cache aggregate memory remain an M2/M3 obligation.

Focused public core/host checks passed before integration. They exercised the
4 KiB/UTF-8 boundary, empty no-work, exact/reversed/duplicate completion
acceptance, query ownership across unavailable-scope/account changes, qualified
empty/partial/limited coverage, wrap, reentrant projection and stop callbacks,
worker/submission/projection exceptions, clear/close/reset and shutdown.
101 admitted rapid edits produced exactly 101 terminals while only the first
and latest queries called the worker. 1,000 match traversals called no worker
and posted no effect. These are structural session checks, not source matching
or performance measurements.

The [self-review](../issues/_checkpoints/find-and-find-in-files-v1/m1-self-review.json)
records corrected identity, coverage and projection issues. Its disposable
negative control removed exact request equality from the core completion guard;
the same core tests then failed with `reverse completion B refused`. The mutant
source and executable were removed. Clang-tidy 22 passed all four touched
translation units with warnings as errors after correcting two optional
conversions in the fixture.

Exact integrated evidence:

- [Core-only build](../issues/_checkpoints/find-and-find-in-files-v1/m1-core-build.json).
- [Scoped core/host and both architecture checks](../issues/_checkpoints/find-and-find-in-files-v1/m1-scoped-integration.json): all four pass on the integrated candidate.
- [Full core-only integration](../issues/_checkpoints/find-and-find-in-files-v1/m1-core-integration.json): 269/272 pass, exit 8. This command is recorded as failed.
- [Documentation gate](../issues/_checkpoints/find-and-find-in-files-v1/m1-documentation.json).
- [Documentation regression tests](../issues/_checkpoints/find-and-find-in-files-v1/m1-documentation-tests.json): 67 tests; their negative-fixture diagnostics are expected.
- [Touched C++ format](../issues/_checkpoints/find-and-find-in-files-v1/m1-format.json): clang-format 22.1.8.

The [integration failure dispositions](../issues/_checkpoints/find-and-find-in-files-v1/m1-integration-failures.json)
retain three failures outside Find's changed surface. The global format gate
rejects the adopted main's `cpp_identifier_context.cpp`; its selected committed
main export reproduces that failure. The exact GitHub fixture inventory omits
the existing `header-source-v1` and `cpp-identifiers-v1` trees, also reproduced
on exact main. The dependency modernization test attempts an uncached pinned
LibArchive download from inside its network-denied Qt-default fixture; this
owned core-only cache contains SQLite alone. The test/provider/fixture files
are byte-identical to adopted main. No external fixture/provider repair is
claimed, and the full passing-suite requirement for final AC08 remains open.

The required frontend-design skill was located and read at
`/Users/jaeger/.claude/plugins/cache/claude-plugins-official/frontend-design/f359d43b4060/skills/frontend-design/SKILL.md`.
M1 changes no native UI. Later UI work must retain the RFC's inherited palette,
native UI font, configured source font and native control constraints.

## M2 candidate and scope

[[BPR-1420]] supplies the shared core matcher, path filters, actual-version
unified plan and exact-record pane/host completion. Product/ownership-document
candidate is `c88131c9d0ca331da95eab1d701de718ef557f9e` under the unchanged
run/segment/mandate. The [source-search contract](../architecture/source-search.md)
documents the deliberately supported RE2 subset and its Unicode 17 provenance.
No new runtime dependency or source-cache acquisition is added to core-only.

The compact state is 1,592 bytes, including fixed single-side selection bounds.
The matcher has a 64 MiB owned-storage reservation and 16 MiB result/preview
ceiling, 8,192 instructions, 64 nesting levels, 1,000 counted repetitions,
1,048,576 compile steps and 134,217,728 entry-scan steps. Callers may tighten
limits only. Initial/1,024-step stop polling covers compilation, preparation,
regex execution and result assembly. A public 1,048,582-byte long-line fixture
uses 3,145,833 counted steps with a conservative 34,781,751-byte owned bound.
This is capacity/work evidence, not RSS, cold acquisition or full AC08 timing.

Exact source tests cover Unicode simple folding without normalization or
multi-scalar expansion, Unicode Whole Word, LF-to-original CRLF spans, nonoverlap,
terminal scalar progress, greedy/lazy/group/alternation/class/flag semantics,
invalid/unsupported input and fixed selection. A disposable official RE2
`google-re2` 1.1.20251105 oracle passes 4,891 exact span comparisons. The portable
Python subset excludes nullable-loop priority cases on which Python and RE2
differ; those cases are checked against RE2 itself. No external regex engine is
linked into BPR. [Self-review](../issues/_checkpoints/find-and-find-in-files-v1/m2-self-review.json)
retains the corrected priority and source-plan findings, actual development
failures and reference-module/notice fingerprints.

Path tests cover nested braces, commas inside classes, literal escaping,
Unicode wildcards, basename/root anchoring, globstar zero/multiple directories,
either rename side and exclude wins. Path evaluation retains resource refusal
as a typed decision. Unified tests use the existing validated semantic index,
keep actual versions separate, merge exact unchanged occurrences and omit
left text hidden by whitespace-equal right rows. Different PR blobs remain
separate; identical same-entry blobs/bytes scan once and retain both locations.

The public pane worker/host path accepts exact original records only through
its serialized current completion. Invalid regex, wrong blobs, oversized or
location-free payloads and stale generations cannot retain current records.
1,000 traversals share the same collection and call no matcher. Fixed selection
remains in exact request identity. The CRLF negative control removes the span
mapping in a disposable compiled object; the public test then fails with a
span mismatch and exit 1. The candidate source is unchanged and the mutant is
removed. [Unicode regeneration](../issues/_checkpoints/find-and-find-in-files-v1/m2-unicode.json)
is byte-identical after format 22, verifies both inputs against the pinned UCD
archive, and rejects altered input.

Candidate evidence:

- [Core-only build](../issues/_checkpoints/find-and-find-in-files-v1/m2-core-build.json).
- [Scoped core/host and both architecture checks](../issues/_checkpoints/find-and-find-in-files-v1/m2-scoped-integration.json).
- [Format 22](../issues/_checkpoints/find-and-find-in-files-v1/m2-format.json) and [nine-TU tidy 22](../issues/_checkpoints/find-and-find-in-files-v1/m2-tidy.json).
- [Actual RE2 reference](../issues/_checkpoints/find-and-find-in-files-v1/m2-re2-reference.json), [negative control](../issues/_checkpoints/find-and-find-in-files-v1/m2-negative-control.json) and [structural measurement](../issues/_checkpoints/find-and-find-in-files-v1/m2-measurement.json).
- [Package notice/inventory regression](../issues/_checkpoints/find-and-find-in-files-v1/m2-package.json): 19 tests, one explicitly skipped native-Linux CPack case; no release/native qualification follows.
- [Documentation gate](../issues/_checkpoints/find-and-find-in-files-v1/m2-documentation.json) and [67 documentation regressions](../issues/_checkpoints/find-and-find-in-files-v1/m2-documentation-tests.json).

M2 changes no UX or rendering, requires no attended decision and holds no review
server/candidate. Full source acquisition, aggregate corpus/cache/result budgets,
batched progress, command/UI/reveal composition and warm/cold public-path performance
remain later milestones. The failed M1 269/272 full-core result remains retained
above; the exhaustive suite was not rerun and no broad passing claim replaces it.

## Whole-RFC acceptance remains pending

| Criterion | State | Remaining evidence |
|---|---|---|
| AC01 | Pending | Real split/unified source matching, offscreen unchanged text and version-contiguous multiline semantics. |
| AC02 | Pending | Shipped Find bar, seeding, options, selection bounds, exact Unicode/LF spans, focus and invalid-regex behavior. |
| AC03 | Pending | Full both-side exact/historical corpus including unopened, filtered, excluded and renamed files. |
| AC04 | Pending | Real activation/F4 through exact tab/file/span/comparison reveal, filters and identity-proven deduplication. |
| AC05 | Pending | Composed query/tab/epoch/authority races and pending-reveal cancellation. M1 session rejection alone is insufficient. |
| AC06 | Pending | Actual material/transport/cancellation/resource failures, honest visible partial coverage and exact-authority retry. |
| AC07 | Pending | Native command/key ownership, accessible layouts and rendered decoration coexistence through the application. |
| AC08 | Pending | Large public-path warm/cold/cancellation/memory measurements, thresholds and usable navigation during cold scanning. Core-only foundation success alone is insufficient. |

No matcher, source-corpus, source-span, Qt, native, installed-package, human or
full-RFC acceptance is inferred from M1. There is no human decision required
for this bounded foundation, and no attended candidate or server is held.
