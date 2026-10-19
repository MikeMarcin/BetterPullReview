# BPR-1618 handoff

Role: rfc-implementer | Model: gpt-6.1-sol | Effort: xhigh

Assignment `core-re2-v1/core-re2`, mandate `core-re2-r1`, one nondelegating
milestone in the BetterPullReview main checkout. Parent remained read-only.

## Result and exact candidates

- Baseline: `85d6ed329`.
- Plan/reservation: `2ec8f259d`; claim: `68add7dfd`.
- Production implementation: `507d89d0fc2acfa7ee1468f972b2577114ef36d7`.
- Final engineering candidate: `3a57eefb9` (provider module-path isolation,
  offline fixture and documentation; production C++ unchanged).
- Board/evidence completion candidate: `2a08a59166991cf2c70547a828ba5baf80d2acde`.
- BPR-1618 is Done. All four acceptance criteria and four milestone exits
  pass; no required human/native/independent evidence remains for this scope.
- Existing RE2/Abseil pins now privately implement the core engine behind
  `default_source_regex_engine()`. Entire regex adapter/target/wiring removed.
  Same options, spans/order/errors/limits/cancellation and host injection.
- Audit: no production worker creation. Real internal Abseil synchronization,
  contention clocks and address/hash state are scoped exceptions. No general
  first-party core effects approved, and no purity/speedup claim.

## Verification and failures reconciled

Committed curated records: `docs/issues/_checkpoints/BPR-1618/verification.md`,
`re2-audit.md`, `acceptance.json`; RFC-0077 ledger/architecture/provenance updated.
Raw non-secret command receipts remain in this runtime directory.

- Core-only build and early public composition: 5/5 pass.
- Final core/source host tests pass, including concurrent matcher (13.58s),
  independent reference, historical epoch corpus, host and PR search.
- Three fresh final core-only processes pass all repeated/shared/four-caller
  tests with identical numeric output and zero stderr; actual factory and
  RE2::Match symbols present, and Qt absent from dynamic linkage.
- Dev public pane Find, PR Find, historical scope and retry: 4/4 pass (36.19s).
  Actual app built, `binary_dependency_surface` passes.
- Core boundary and contract, complete architecture contract, package/runtime,
  release-security and binary audit contracts pass.
- Initial architecture scan raced our dev configure's disappearing CMakeScratch
  path. Quiet recheck passed (45.84s). Initial source-policy test binary was
  omitted from the targeted build; built it and its CTest recheck passed.
- Dependency integration caught Abseil loading caller CTest modules and offline
  fixtures missing RE2/Abseil archives. Provider now clears module path locally;
  five focused isolated corrective methods pass (13.24s), with unchanged passing
  evidence reused. Two unrelated syntax-provider cases skipped in core-only.
- An early dev build attempted before configure completion reported missing
  CMakeCache; sequential final build passed. No remaining in-scope failure.
- Format22 (all touched C++), tidy22 (production wrapper), documentation gate,
  67 documentation tests and diff whitespace checks pass. Post-finish docs and
  board/binding checks are in the final runtime receipts.

## Review, ownership and commands

No human review was required, and no server/candidate was prepared or touched.
No writer/subagent besides this leaf; no protected historical packet changed.
All task-owned builds, CTests, configure subprocesses, fresh-process tests,
format/tidy and documentation commands finished. No process/server handed off.
No live GitHub mutation, push, owner desktop/browser/private-data access or
package publication occurred. Temporary edit scripts were removed; selection,
plan/checkpoint/observation and useful evidence retained. Repository writer
ownership is released at handoff after final status/check validation. The
committed handoff is a bookkeeping-only descendant of the candidate above.

## Continuation and limits

Report this completed bounded mandate. No engineering successor assigned.
Original RFC/native Windows/Linux and complete package/vault/crash/vulnerability
qualification obligations remain with their existing owners; no broader release
or platform-matrix acceptance implied. No new pins or UX changes.

## Changed paths

```text
M	AGENTS.md
M	CONTRIBUTING.md
M	README.md
M	cmake/BprDependencies.cmake
M	cmake/BprRe2Provider.cmake
M	cmake/VerifyCoreBoundary.cmake
M	docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md
M	docs/architecture/source-search.md
M	docs/architecture/system-architecture.md
M	docs/development/build-and-test.md
A	docs/issues/60-done/BPR-1618-core-re2.md
M	docs/issues/NEXT_ID
A	docs/issues/_checkpoints/BPR-1618/acceptance.json
A	docs/issues/_checkpoints/BPR-1618/re2-audit.md
A	docs/issues/_checkpoints/BPR-1618/verification.md
A	docs/issues/_reservations/BPR-1618--BPR-1618--42c00e7c1279.json
M	docs/rfcs/RFC-BPR-0077-find-and-find-in-files.md
A	docs/rfcs/plans/core-re2-v1/milestone-plan.json
A	docs/rfcs/plans/core-re2-v1/source.json
M	docs/verification/RFC-BPR-0077.md
M	packaging/dependencies.json
M	src/adapters/CMakeLists.txt
D	src/adapters/regex/CMakeLists.txt
D	src/adapters/regex/include/bpr/adapters/regex/re2_source_regex_engine.hpp
M	src/app/CMakeLists.txt
M	src/app/src/github_application.cpp
M	src/core/AGENTS.md
M	src/core/CMakeLists.txt
M	src/core/include/bpr/core/source_matcher.hpp
R067	src/adapters/regex/src/re2_source_regex_engine.cpp	src/core/src/source_regex_engine.cpp
M	tests/application/CMakeLists.txt
M	tests/application/epoch_application_host_tests.cpp
M	tests/core/CMakeLists.txt
M	tests/core/source_matcher_tests.cpp
M	tests/scripts/core_boundary_tests.py
M	tests/scripts/dependency_modernization_tests.py
M	tests/support/source_regex_engine_fixture.hpp
M	third_party/re2/PROVENANCE.md
```
