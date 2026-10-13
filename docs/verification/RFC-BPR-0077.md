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
