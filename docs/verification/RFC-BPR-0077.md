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

## M3 candidate and scope

[[BPR-1421]] supplies exact retained-tree membership, epoch-owned full documents
and streamed PR search. Initial product is `4b6bc16e50d6e134efc3570c2dccff74898e0d65`;
self-review corrections are integrated at exact tested product
`df4b42d6942d131caebc2343a6ed360c8023bc0d`. Run, segment, mandate and authored
criteria remain unchanged. The [acceptance](../issues/_checkpoints/find-and-find-in-files-v1/m3-acceptance.json)
binds all four bounded exits; [self-review](../issues/_checkpoints/find-and-find-in-files-v1/m3-self-review.json)
records findings, corrected development failures and evidence reuse.

The shortest public composition runs search host → PR worker → actual epoch
host with fake narrow cache/catalogue ports. A historical-only path survives an
accepted remote-head change, and both full sides come from the requested retained
pair. Missing exact commits/catalogues are unavailable. The installed Git
SHA-1/SHA-256 contracts verify exact raw rename/copy paths, unchanged empty pairs
and refusal of a missing commit. A disposable mutation replacing the requested
historical head with today's accepted head fails the public fixture. Product
bytes remain unchanged and the mutant is removed.

Membership ignores Files filters, receipts and opened state. Path policy is
compiled before reads; either rename side includes, and exclusion wins. Added,
removed, modified, renamed and evidenced copied entries retain their exact sides.
Binary, invalid UTF-8, missing material, symlink/gitlink and size/line refusals
qualify individual sides. The opposite searchable side and accepted result
prefix remain usable. A same-authority retry keeps the original corpus scope.
Identical same-entry blobs share scanning while retaining both locations.

Immutable reads remain on the existing epoch actor. A replacement that still
needs an in-flight blob joins one read/job even after the old query caller stops.
Clear, authority invalidation and shutdown cancel unneeded work. Selected-source
work and pane Find can yield background reads and scanning. The existing actor
retention remains 256 blobs/16 MiB; background entries cannot evict foreground
material. No competing cache, checkout, whole-corpus materialization, syntax or
diff computation is introduced.

The worker admits at most 10,000 ordered results; an actually observed extra hit
qualifies the visible prefix as capped. Publications contain at most 128 records,
with one coalesced progress closure and a terminal on the decision lane. Progress
is throttled to at most ten updates per second; terminal-before-progress and
late closures cannot restore stale state or falsely report quiescence. One
thousand result traversals perform no read or worker call. Core stays 1,592 bytes;
catalogues, documents, matcher programs and records remain outside it. A 192 MiB
conservative admission envelope accounts for two documents, the matcher, existing
cache, catalogue stages and result/publication copies. It is an owned-capacity
bound, not a whole-application RSS measurement or final AC08 timing claim.

Evidence:

- [Core build](../issues/_checkpoints/find-and-find-in-files-v1/m3-core-build.json), [six-case core/host/architecture integration](../issues/_checkpoints/find-and-find-in-files-v1/m3-scoped-integration.json) and [correction checks](../issues/_checkpoints/find-and-find-in-files-v1/m3-correction-verification.json).
- [Installed Git build](../issues/_checkpoints/find-and-find-in-files-v1/m3-git-build.json), [process/cache contracts](../issues/_checkpoints/find-and-find-in-files-v1/m3-git-contracts.json) and [corrected exact-copy process course](../issues/_checkpoints/find-and-find-in-files-v1/m3-final-git-process.json).
- [Format 22](../issues/_checkpoints/find-and-find-in-files-v1/m3-format.json) and [corrected format](../issues/_checkpoints/find-and-find-in-files-v1/m3-final-format.json).
- [Initial core/host tidy](../issues/_checkpoints/find-and-find-in-files-v1/m3-tidy.json) and [initial Git tidy](../issues/_checkpoints/find-and-find-in-files-v1/m3-git-tidy.json) remain failed receipts. Corrections pass; [changed-line Git tidy](../issues/_checkpoints/find-and-find-in-files-v1/m3-scoped-git-tidy.json) excludes two byte-identical baseline diagnostics outside M3. No whole-file or global tidy pass is claimed.
- [Historical negative control](../issues/_checkpoints/find-and-find-in-files-v1/m3-negative-control.json) and [small foundation-suite measurement](../issues/_checkpoints/find-and-find-in-files-v1/m3-measurement.json); the latter is not the large-review public application benchmark.
- [Documentation](../issues/_checkpoints/find-and-find-in-files-v1/m3-documentation.json) and [documentation regressions](../issues/_checkpoints/find-and-find-in-files-v1/m3-documentation-tests.json).

M3 changes no UX or rendering and requires no attended decision. M4/M5 still own
shipped commands, decoration/reveal composition, native judgement and final warm/cold
public-path performance. The failed M1 269/272 full-core result remains retained;
it was not rerun or replaced by a global passing claim.

## M4 native pane Find

[[BPR-1422]] accepts all three pane-find exits on exact product candidate
`a7fb9af0a2e0f0a21ab0de86c3a938a70af28626`, under the unchanged mandate/run/segment. The
[acceptance](../issues/_checkpoints/find-and-find-in-files-v1/m4-acceptance.json)
and [self-review](../issues/_checkpoints/find-and-find-in-files-v1/m4-self-review.json)
bind the shipped source path. Original AC01/AC02 pass; AC07's pane slice passes.
The required frontend-design skill was applied before UI work, retaining native
palette roles, UI/source fonts and controls without QSS.

The course opens a synthetic PR through the real application, account/PR/Review
entry, exact epoch/cache loading and installed Git. Physical Find/F3/Shift-F3 and
View actions reach both full split documents, including offscreen unchanged
source. Unified Find searches unchanged rows once and actual removed/added
versions; opposing visual rows cannot manufacture a multiline occurrence.
Native caret/selection/escaped regex seeding, multiline/IME input, fixed original
selection bounds, Unicode/CRLF and terminal EOF spans, match options/counts,
invalid/unsupported patterns, input limits, wrap and focus are demonstrated.

One thousand traversals preserve the same records, diff and semantic index with
zero matcher calls and zero endpoint conversions. Closing and Next reuse a
bounded complete exact-scope/query/options collection and cached original spans,
then advance once. Qt has no source-read/diff/matcher effect capability. Shared
selection/reveal uses actual-version fragments; explicit Copy yields original
text around opposing unified rows. Painting indexes visible highlights and scroll
track pixels without adding occurrence widgets or changing comparison geometry.
The existing reader course still rejects a duplicate change-navigation presenter.

Native full-window rasters were inspected at actual DPR 1.0, 1.25 and 2.0, with
full-size physical frames 1280×860, 1600×1075 and 2560×1720. A requested 720-pixel
window is constrained by the existing supported minimum to 900 logical pixels;
narrow frames are 900×640, 1125×800 and 1800×1280. All bar controls remain visible
and accessible, with native font/no QSS and inherited accepted dark palette.
This is offscreen macOS/Qt evidence, not owner desktop, VoiceOver, physical
scaling, Windows/Linux device or large-review performance evidence.

Exact candidate evidence:

- [Four native/public-reader courses](../issues/_checkpoints/find-and-find-in-files-v1/m4-native-public-path.json): all pass; 40.217 seconds. [Render identities and observations](../issues/_checkpoints/find-and-find-in-files-v1/m4-native-render.json).
- [Five Qt-free source cases](../issues/_checkpoints/find-and-find-in-files-v1/m4-core-integration.json), [architecture and network manifest](../issues/_checkpoints/find-and-find-in-files-v1/m4-architecture.json), and [fixture provenance](../issues/_checkpoints/find-and-find-in-files-v1/m4-fixture-provenance.json) pass.
- [Seventeen-file format22](../issues/_checkpoints/find-and-find-in-files-v1/m4-format.json), [three new-TU tidy22](../issues/_checkpoints/find-and-find-in-files-v1/m4-new-tidy.json), [five changed-line composition TUs](../issues/_checkpoints/find-and-find-in-files-v1/m4-scoped-tidy.json) and [final source/reader-guard tidy22](../issues/_checkpoints/find-and-find-in-files-v1/m4-final-scoped-tidy.json) pass. No global/full-file tidy pass is claimed.
- [Initial integration setup failure](../issues/_checkpoints/find-and-find-in-files-v1/m4-integration-setup-failure.json) retains the absent Qt-tree executables; their built core-only correction passes separately. Architecture negative contract passed in that initial run and is reused for unchanged boundaries.
- [Fixture inventory](../issues/_checkpoints/find-and-find-in-files-v1/m4-fixture-inventory.json) remains failed: only baseline header-source-v1/cpp-identifiers-v1 omissions. New source-find-v1 is explicitly registered and provenance-checked; no bypass or new omission is hidden.

The M1 full 269/272 failure and M3 full-file tidy findings remain retained. Final
pack integration must resolve baseline fixture registrations narrowly while
preserving exact inventory and sanitization rejection. Documentation closeout
receipts are recorded with the guarded milestone finish and canonical handoff.

## Whole-RFC acceptance remains pending

| Criterion | State | Remaining evidence |
|---|---|---|
| AC01 | Pass (M4) | Shipped split/unified native course proves both exact targets, offscreen source and actual-version multiline order. |
| AC02 | Pass (M4) | Shipped native bar/course proves seeding/options/multiline/fixed bounds, exact Unicode/CRLF/EOF, admission, focus/wrap and invalid/unsupported syntax. |
| AC03 | Pending | M3 foundation proves full exact/historical membership; shipped Search composition must demonstrate unopened, filtered, excluded and renamed files. |
| AC04 | Pending | Real activation/F4 through exact tab/file/span/comparison reveal, filters and identity-proven deduplication. |
| AC05 | Pending | Composed query/tab/epoch/authority races and pending-reveal cancellation. M1 session rejection alone is insufficient. |
| AC06 | Pending | M3 foundation proves material/transport/cancellation/resource qualification and exact-authority retry; composed visible coverage remains. |
| AC07 | Partial (M4); pending full | Pane command/key ownership, accessible native bar and decoration coexistence pass; PR Search/F4 surface remains. |
| AC08 | Pending | Large public-path warm/cold/cancellation/memory measurements, thresholds and usable navigation during cold scanning. Core-only foundation success alone is insufficient. |

M1–M4 accept only their named milestone scope. Original AC03–AC06, complete
AC07/AC08 and whole-RFC closeout remain pending. No human decision is required
for M4; no attended candidate/server or owner desktop capture is held.
