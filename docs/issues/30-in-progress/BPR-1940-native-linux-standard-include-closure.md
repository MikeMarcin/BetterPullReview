---
schema: "implement-rfc.card.v1"
id: "BPR-1940"
title: "Close native Linux missing standard includes through full development builds"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"portability","found_by":"v0.4.2 build39 native Ubuntu GCC Release compilation","related_cards":["BPR-1939"],"milestone_plan":{"id":"bpr-1940-linux-include-closure","path":"docs/rfcs/plans/bpr-1940-linux-include-closure/milestone-plan.json"}}
---

# BPR-1940: Native Linux standard include closure

## Acceptance

- AC1. Give `defect_identity.hpp` direct `<cstdint>` ownership for its public
  fixed-width integer declarations, preserve its existing `<cstddef>`, and
  correct any further missing standard includes exposed by the full native
  build only at their direct owners. No behavior, flags, assertions or unrelated
  style changes.
- AC2. Finish complete native Linux Release and Debug development builds with
  keep-going, retaining every initial failure and correction iteration. Bind
  private source revisions/patches, configured features and original compiler,
  optimization, hardening and warning flags; do not stop at a single-TU proof.
- AC3. Keep frozen Mac and Windows build39 inputs/output bytes untouched. Any
  explicitly agreed tests-off feature build uses an ordinary unsealed
  development identity; never forge build39, bypass a release proof or claim
  release acceptance for development output.
- AC4. Focused source/compile checks, formatting and documentation pass, then
  integrate the complete bounded include correction into authoritative main.
  Retain full release qualification as a separate coordinator obligation.

## Diagnosis and authorized development boundary

`defect_identity.hpp` declares `std::uint64_t` and `std::uint8_t` without
`<cstdint>`; it already owns `std::size_t` via `<cstddef>`. Native GCC exposes
this missing declaration, then emits cascading aggregate-initializer errors in
the implementation. The declaration header was introduced by the profile
runtime split (`240c2d4d6a`). The initial correction is one direct include; the milestone
also owns only additional missing-standard-include closure found by the full
native compiler.

The coordinator preserves frozen source
`b5dc26e2cb2ca28435806e4f3e3aa72ccc4a2439`, original failure receipts and
accepted Mac/Windows artifacts. The failed Linux checkout may be explicitly
handed over as a private development branch after archival and idle proof.
Until that handback there is no native access. Reused Release/Debug build
caches keep their normal flags and features and run with keep-going. A numbered
release cohort is not reused for changed source; ordinary local configuration
has an empty cohort/producer and build number zero. No package, signer,
publication or cohort allocation is part of this card.


## Initial correction and verification binding

The first source commit `fe05c06dc` adds `<cstdint>` to the declaration header;
its existing `<cstddef>` is retained. The matching implementation compiles on
macOS and focused clang-tidy22/format22 pass. The application source-content
contract correctly rejected the changed header before its binding was updated.
An independent recomputation first reproduces the original ordered 142-file
manifest digest, then proves that only this header changed. Its new digest is
resealed without changing any inventory member, production mutation-branch
pin, assertion or grant. The original composed source contract then passes
(1.93 s). The actual drift failure and before/after manifests are retained in
external `candidate39/implementer1940/`.

Native development is pending full compile closure. The original build39
source and three cache metadata sets were archived by the coordinator before
handing over the rejected Linux checkout. The initial SSH-context launcher
correctly refused the missing D-host storage proof before any source switch or
compiler ran; the maintained guard is not weakened. The exact same driver is
to run through the established Limited interactive context. It creates a
private branch, records the source commit and configured feature/flag choices,
and serially finishes Release/Debug and the supported tests-off graph with
keep-going. Source changes remain unlanded on authoritative main until these
full development builds complete or a concrete out-of-scope error is reported.
