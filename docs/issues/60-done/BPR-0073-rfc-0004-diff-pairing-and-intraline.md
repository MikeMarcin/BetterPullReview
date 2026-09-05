---
schema: "implement-rfc.card.v1"
id: "BPR-0073"
title: "Implement deterministic diff pairing and intraline ranges"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0072"]
---

# Implement deterministic diff pairing and intraline ranges

Implement deterministic text decoding/line splitting, primary diff
canonicalization, monotonic secondary line pairing, internal replacement
anchors, and valid original-text intraline ranges. Preserve final-newline and
line-ending facts and make whitespace policy part of the exact option digest.

Weak pairings remain explicit and moved blocks remain deletion plus insertion
in the primary path with separate relation metadata.

## Required direct evidence

Cover identical, insert/delete, 4:7 and 7:4 replacement, internal anchors,
repeated similar lines, tabs/whitespace modes, LF/CRLF/final newline, Unicode,
empty files, rename identity, binary/unavailable, and moved-block fixtures.
Tie breaking and stable IDs must repeat under replay and generated cases.

## Implementation

- Added a standard-library-only public computation boundary over exact
  optional byte spans and the BPR-0072 comparison key. Blob presence,
  normalized options, authority, and exact file identity must agree before
  computation.
- Retained byte-faithful documents with logical content/terminator spans, LF,
  CR, CRLF, and final-newline facts. Strict UTF-8 rejects continuation,
  truncation, overlong, surrogate, and out-of-range encodings; NUL is a typed
  binary result and absent added/deleted sides create no invented line.
- Added bounded deterministic primary LCS alignment, monotonic scored
  secondary pairing with weak rejection and stable tie order, internal exact
  and similar anchors, original-line-relative UTF-8-boundary intraline edits,
  and unique exact whole-segment move metadata over the monotonic
  delete-plus-insert path.
- Finalized segment IDs after refinement, built the mechanical maximal
  contiguous changed-run groups required by the canonical validator, and
  published only results that pass both alignment and retained
  document/intraline validation.
- Charged every allocated primary, secondary score/decision, similarity, and
  intraline matrix against one overflow-safe computation budget. Byte, line,
  matrix, and record limits fail with no partial semantic records.

## Direct verification

- Final `core-only` focused matrix: 6/6 passed in 31.61 seconds
  (`core_diff_compute`, `core_diff_model`, both core dependency/boundary
  gates, and both architecture boundary gates).
- Final `dev` focused matrix: the same 6/6 passed in 32.87 seconds.
- Final Darwin ASan/UBSan focused matrix: the same 6/6 passed in 33.13
  seconds with fail-fast runtime options. Leak detection stayed disabled
  because Darwin AddressSanitizer does not support that mode; address and
  undefined-behavior instrumentation remained active.
- `core_diff_compute` directly covers empty/identical, exact start/middle/end
  insertion/deletion spans, complete 4:7 and 7:4 refined paths, internal
  exact/similar anchors, deterministic repeated-line tie selection and replay,
  weak rejection, whitespace policies, LF/CR/CRLF/final-newline identity,
  retained display bytes, multibyte/combining/emoji intraline boundaries,
  malformed UTF-8 classes, empty/absent/binary/unavailable states, rename-
  stable identity, unique/ambiguous moves, exact and one-cell-below limits,
  invalid retained-result mutations, and a generated replay corpus.
- Documentation validation, GitHub-workflow absence (0 tracked, 0 on disk),
  architecture/dependency scans, and `git diff --check` pass after the final
  tracker transition.

## Review, simplification, and deferral

Two initial read-only scouts changed no files and selected the minimal public
API, fixture matrix, and group-ownership boundary. The exclusive writer stayed
within its five owned code/build/test files; root independently reconfigured,
rebuilt, inspected, strengthened, documented, and verified the result.

The fresh critical review found missing retained-result validation,
under-counted secondary grids, and weak unequal/tie assertions. The public
computed-result validator, full grid charging, mutation-negative tests, and
complete expected path assertions remediate those findings; the reviewer's
focused replay reports no remaining blocker/high finding.

The required simplification pass removed unused includes, replaced repeated
group lookup with one segment-to-group projection, retained explicit bounded
algorithm phases, and aligned the new files with local style. BPR-0080 now
explicitly retains scalable/indexed move discovery and exact moved sub-block
decomposition. BPR-0073 therefore claims only deterministic unambiguous exact
whole-one-sided-segment move metadata; it does not overclaim general or
large-scale move detection.

## Documentation disposition

The diff architecture, system allocation table, build guide, fixture catalog,
RFC-BPR-0004 claim ledger, issue board, and scale card describe the live
computation/validation boundary and its proof limits. No Qt, host, provider,
packaging, GitHub mutation, operator, or agent-instruction surface changed.
Mapping/navigation/detail remains BPR-0074; host acquisition/cache/cancellation
remains BPR-0075; rendered and scale claims remain on their existing cards.
Windows execution remains intentionally open under the repository's existing
target-native Windows evidence policy and does not block this slice.

## Result

BPR-0073 is complete. BetterPullReview now computes one deterministic,
validated semantic alignment and its retained text/pair/intraline facts from
exact inputs without Qt, effects, fake rows, competing mappings, or crossing
primary move paths. BPR-0074 is ready to derive mapping, navigation,
selection, and detail semantics from those same stable IDs.
