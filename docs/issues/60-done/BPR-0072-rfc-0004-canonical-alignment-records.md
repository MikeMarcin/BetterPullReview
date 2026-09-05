---
schema: "implement-rfc.card.v1"
id: "BPR-0072"
title: "Implement canonical diff and alignment records"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0071"]
---

# Implement canonical diff and alignment records

Add the Qt/provider/effect-free comparison identity, normalized option digest,
half-open line spans, line/boundary references, ordered alignment segments,
line pairs, change groups, move relations, diagnostics, and stable identities.
Validate exact left/right coverage, bounds, canonical adjacency, monotonicity,
kind/span consistency, and nonempty combined extent.

Use compact immutable publication values and deterministic IDs derived only
from exact comparison inputs and canonical structure. Do not include measured
pixels, Qt indexes, paths-as-authority, worker completion order, or live scroll
state.

## Required direct evidence

- Table and generated/property cases cover empty files, equal text, insertions,
  deletions, replacements, invalid spans, overflow/bounds, and deterministic
  replay.
- A public validator rejects every malformed invariant and accepts canonical
  empty-side segments without inventing source lines.
- Core-only/dependency, sanitizer, documentation, workflow-absence, and diff
  hygiene gates pass.

## Implementation

- Added one standard-library-only core model for retained normalized diff
  options and their digest; exact full-authority/repository/PR/anchor/
  optional-blob/file/options/algorithm
  comparison keys; half-open spans; line and boundary references; ordered
  segments; contiguous group intervals and summaries; secondary pair records;
  separate move metadata; and typed content diagnostics.
- Domain-separated, endian-defined SHA-256 encodings produce comparison,
  segment, pair, group, and move identities. Encoders never hash C++ object
  layout, `std::hash`, paths, pointer values, allocation order, Qt state, or
  worker completion order.
- The public validator requires exact monotonic coverage of both documents,
  valid kind/span shapes, nonempty combined extents, one group per contiguous
  changed run, group/member/summary agreement, option-compatible monotonic
  replacement pairs, directed one-to-one equal-extent delete-to-insert moves,
  canonical move order, no orphan empty-result records, and stable IDs.
  Same-kind segment boundaries are rejected unless accepted pair or move
  metadata requires the finer semantic identity.
- A group stores a compact half-open interval into the immutable ordered
  segment vector. Its ordered member IDs are exactly that slice, so the model
  does not allocate or permit a competing member list.

## Direct verification

- Clean `core-only` build and focused boundary matrix: 5/5 passed in 34.08
  seconds (`core_diff_model`, both core dependency/boundary gates, and both
  architecture boundary gates).
- Clean `dev` build and the same matrix: 5/5 passed in 34.24 seconds.
- Clean ASan/UBSan build with fail-fast runtime options: 5/5 passed in 34.95
  seconds. Leak detection was disabled because Darwin's AddressSanitizer does
  not support that mode; address and undefined-behavior instrumentation
  remained active.
- `core_diff_model` covers exact empty/equal/insertion/deletion/replacement/
  mixed/move tables, complete added-file empty-side identity, zero and
  `UINT32_MAX` boundaries, malformed invariant mutations, compatibility-aware
  adjacency, full-authority and option-policy mismatches, orphan records,
  canonical group/move order and topology, known-answer digest/record-ID
  vectors, and seeds 1 through 512 with deterministic valid and rejected
  malformed derivatives.
- The repository documentation validator, 104-test architecture scan,
  GitHub-workflow absence checks (0 tracked, 0 on disk), and `git diff --check`
  passed after tracker transition.

## Independent review and simplification

Three read-only reviews changed no files. The first wave's three high findings
identified arbitrary changed-segment splits, touching group partitions, and
producer-ordered move relations. All three were reproduced and remediated
with compatibility-aware coalescing, a single contiguous changed group, and
lexicographic source/destination move order plus direct regression cases.
Known-answer identity vectors and missing-blob diagnostic coherence also close
that wave's medium/low evidence gaps. The fresh-context final review then found
that opaque options alone could not reject disabled pair/move records, exact
moves did not require equal extents, and the key did not bind its explicit IDs
to a full authority. Retained normalized options, policy/digest validation,
one-to-one equal-extent move topology, and full-authority key checks remediate
all three; an independent root audit also added orphan-record rejection for
empty text.

The required simplification pass retained the explicit canonical encoders and
ordered validation phases because they encode the fail-closed contract. It
removed only avoidable insertion ceremony and documented why pair/move
metadata may retain an otherwise coalescible boundary.

## Documentation disposition

The diff architecture, build guide, fixture catalog, and RFC-BPR-0004 claim
ledger now describe the live record boundary, focused command, generated
corpus, compact group representation, and exact proof limit. D-02 and D-03
have their structural foundation; D-01 remains open for BPR-0073's actual text
computation, and mapping/navigation, host, Qt, rendered, and performance claims
remain open on their existing cards. No agent instruction or public operator
surface changed.

## Result

BPR-0072 is complete. BetterPullReview now has one deterministic, compact,
Qt-free semantic publication contract and validator for the canonical
alignment records consumed by the remaining RFC-BPR-0004 slices. BPR-0073 is
ready to implement actual text diff, secondary pairing, and intraline output
without creating a competing mapping model.
