# BPR-1620 handoff

Role: rfc-implementer | Model: gpt-6.1-sol | Effort: xhigh

Assignment `external-determinism-v1/architecture-policy`, mandate
`external-determinism-r1`, leaf `/root/external_determinism_docs` in the shared
BetterPullReview main checkout. Parent `/root` remained read-only. No subagents.

## Result and candidates

- Baseline `80f280ee8c9946b98c6527a79c65c878d0a6a4d1`.
- Plan/reservation `6def4191c`; claim `e16756831`.
- Exact policy candidate `4773277f981d429c29862e297c0e31c3d5e90335`.
- Acceptance/board completion `f5fc3a6be115b7ec8110c74a3af5fa774435bc5b`.
- BPR-1620 is Done; all three criteria and milestone exits pass.

The base architecture §2.1 defines external determinism for first-party and
library-backed computation. Internal CPU dispatch, caches, allocation,
synchronization, contention clocks and hash seeds may vary timing/layout without
changing promised semantics. A separate system-architecture inventory preserves
adoption approval, pins, licensing, private wrappers and use/link ownership.
Root/core instructions and current contributor/build/product guidance link these
rules; the adopted RFC amendment supersedes library-specific exception rationale.
Host effects, scheduling/cancellation/lifecycle and Qt independence remain.
Resource failure, stop timing and cross-platform path/random identity are
explicitly qualified. Product behavior, dependency graph and enforcement patterns
are unchanged. Prior owner answers, RFC assets/plans and evidence are preserved.

## Verification

[Verification](verification.md), [acceptance](acceptance.json) and
[original binding](milestone-binding.json) retain exact commands and evidence.
All focused tests were bound to policy candidate `4773277f9`:

- Documentation gate: DOCUMENTATION_OK; documentation contracts: 67/67.
- Direct core and architecture boundary scans pass.
- Existing core boundary contracts: 11/11; focused architecture contracts: 10/10.
- All nine touched CMake files have identical non-comment executable content.
- Twenty new rule/inventory links resolve to the current headings; the public
  reading path README Architecture → base rule → implementation inventory works.
- Scoped whitespace check passes; historical RFC assets/plans remain unchanged
  except the new additive plan. No application rebuild or exhaustive mutation
  matrix was needed for this documentation/comment-only change.

Final plan/board checks and the post-finish documentation gate are recorded with
this checkpoint. Helpers check consistency, not engineering evidence truth.

## Review, ownership and commands

No required human/native/independent acceptance remains in this bounded scope.
There is no review session, event cursor, protected candidate or review server.
All owned documentation, boundary, contract and bookkeeping commands completed;
no process/server is handed off. No live GitHub, desktop, browser/private-data,
remote or release operation occurred. The consumed edit script was removed;
useful receipts and the authored plan/selection remain in
`/tmp/bpr-external-determinism`.

Unrelated BPR-1378 code/tests/docs were preserved and excluded from owned commits.
Its source `2d56775f0` and completion `bddf584fb` are interleaved in main; its
untracked `binding.json` and `continuation.md` remain under its own checkpoint.
This is not a clean-checkout claim. The policy candidate's owned paths are
unchanged by the bookkeeping commits. This leaf releases writer ownership after
committing its final checkpoint; parent can report the milestone complete.

## Continuation and uncertainties

Stop at this assignment boundary; no successor milestone is assigned. There are
no remaining in-scope obligations or pending decisions. Original Windows/Linux
hash/parser/diff evaluation and wider native/package/vault/crash/vulnerability
and release qualification remain on their existing cards. No wider RFC closure
or new dependency permission is implied. Source guards remain textual/build
checks, with their semantic/effect-analysis limits stated in the architecture.

## Owned policy paths

```text
AGENTS.md
CONTRIBUTING.md
README.md
cmake/VerifyCoreBoundary.cmake
cmake/BprDtlProvider.cmake
cmake/BprFmtProvider.cmake
cmake/BprHaclSha2Provider.cmake
cmake/BprRapidFuzzProvider.cmake
cmake/BprSha1dcProvider.cmake
cmake/BprUtf8procProvider.cmake
cmake/BprZlibProvider.cmake
docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md
docs/architecture/github-host-implementation.md
docs/architecture/product-contract.md
docs/architecture/system-architecture.md
docs/development/build-and-test.md
docs/rfcs/RFC-BPR-0001-product-contract-native-architecture.md
docs/rfcs/RFC-BPR-0077-find-and-find-in-files.md
docs/rfcs/RFC-BPR-0080-hashing-replacement.md
docs/rfcs/RFC-BPR-0082-diff-engine-evaluation.md
docs/rfcs/amendments/RFC-BPR-0001-external-determinism-r1.md
src/core/AGENTS.md
src/core/CMakeLists.txt
```

Additional owned bookkeeping consists only of BPR-1620's reservation/counter,
card, additive plan/source receipt and checkpoint records.
