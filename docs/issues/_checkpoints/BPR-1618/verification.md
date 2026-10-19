# BPR-1618 verification and checkpoint

Assignment: `core-re2-v1/core-re2`, mandate `core-re2-r1`, main checkout.
Baseline `85d6ed329`. Implementation `507d89d0fc2acfa7ee1468f972b2577114ef36d7`;
final engineering candidate `3a57eefb9` adds provider module isolation and the
offline-cache fixture correction, plus documentation. Production C++ bytes are
unchanged between those candidates. Plan/reservation `2ec8f259d`; claim `68add7dfd`.

## Accepted result

| Criterion | Evidence and outcome |
| --- | --- |
| AC1 | [Pinned-source audit](re2-audit.md): production RE2 creates no workers; Abseil TLS identity is caller bookkeeping, and its test-only thread pool is excluded. Internal locks/clocks/hash state are scoped implementation effects; ordinary error logging stays off. Root/core instructions and architecture describe the owner's exception without admitting arbitrary effects. |
| AC2 | Core privately links the existing verified RE2/Abseil pins in every graph and implements `default_source_regex_engine()` behind the existing interface. The adapter's directory, target, class API and wiring are removed. Core-only compiler/scanner and historical epoch-host search run the real engine; symbol inspection confirms the core factory and `RE2::Match`, and `otool -L` shows no Qt. The actual composition supplies the same factory. |
| AC3 | Fixed Unicode/case/LF/Whole Word/zero-width/ordering/syntax/error-offset, limit and cancellation cases pass. New tests share compiled query and path programs across four caller-owned threads, interleaving fresh compilation and warmed matching. Three separate final core-only processes pass with identical numeric stdout and empty stderr. Host/coalescing/historical-corpus tests and the independent portable Python span oracle pass. |
| AC4 | Focused core/host/public composition, both boundaries/contracts, package/security/binary contracts, provider isolation/offline correction, format 22, tidy 22 and documentation checks pass as detailed below. Existing broader native/release obligations are preserved. |

## Commands and actual outcomes

- `cmake --preset core-only`; targeted core-only build of `test_source_matcher`,
  `test_source_search_host`, `test_source_pr_search`,
  `test_better_pull_review_application_epoch_host_tests` and
  `test_source_search_policy`: pass. No QApplication or regex adapter needed.
- Early public-path exercise: `core_source_matcher`,
  `core_source_matcher_reference`, `application_epoch_host`,
  `application_source_search_host`, `application_source_pr_search`: 5/5 pass.
- Final focused core-only integration selected the six source/policy/host tests,
  both boundary checks/contracts, `package_runtime_contract`,
  `release_security_audit_contract`, `binary_dependency_audit_contract` and
  `dependency_modernization_contract`. Eleven of fourteen passed immediately,
  including the concurrent matcher (13.58 s), reference and host courses.
  Three failures were corrected/rechecked below; none remains unresolved.
- `architecture_boundaries` initially overlapped the dev configure and tried to
  read a disappearing CMakeScratch file. After configuration was quiet, the
  unchanged gate passed in 45.84 s. The complete
  `architecture_boundary_contract` passed in 235.38 s.
- `core_source_search_policy` initially had no executable because the targeted
  build omitted it. Built its target; its isolated CTest recheck passed.
- Dependency integration exposed Abseil's upstream `include(CTest)` consulting
  a hostile caller module path, plus three offline-fixture failure subcases
  because the cache helper did not yet copy the newly core RE2/Abseil archives.
  The provider now clears `CMAKE_MODULE_PATH` only in its function scope, and
  the fixture copies both exact pinned archives. Five isolated corrective
  methods pass (13.24 s): hostile module path, offline SQLite reuse/refusal,
  poisoned-header staging, configured audit and tests-disabled Qt-free graph.
  Unchanged passing cases are reused. The original run's two syntax-provider
  cases were skipped because this is a core-only tree; no native syntax claim.
- `cmake --preset dev`; build `test_source_find_public_path`,
  `test_source_pr_find_public_path`, `test_source_pr_search_scope_path` and
  `app_better_pull_review`: pass. An initial build attempted before configure
  finished reported missing CMakeCache; the sequential retry above passed.
- Dev CTest `application_source_find`, `application_source_pr_find`,
  `application_source_pr_search_scope`, `application_source_pr_search_retry`:
  4/4 pass (36.19 s), serial/offscreen through real composition, epoch actor
  and installed Git. `binary_dependency_surface` on the actual app: pass.
- Three fresh core-only `source_matcher` processes: all pass, exact identical
  numeric output, zero stderr. This varies process address/hash state while
  exercising the repeated/shared/concurrent fixed-answer cases.
- clang-format 22.1.8 dry-run on all six touched C++/header paths: pass.
  clang-tidy 22.1.8, warnings as errors, on the relocated production wrapper:
  pass (one upstream/system warning suppressed). `git diff --check`: pass.
- `python3 scripts/verify_documentation.py --root .`: pass.
  `python3 tests/scripts/verify_documentation_tests.py`: 67 tests pass.
  Final tracker/docs validation is recorded with the completion commit.

The final provider-only correction changes named-module resolution for caller
supplied module paths. It leaves the normal build's empty module-path resolution,
compiled sources, options, targets and production C++ unchanged. Its focused
configure/offline tests verify that changed behavior; existing binary evidence
remains applicable. Package licenses and pins are unchanged; manifest linkage and
RE2 provenance now name the core-private implementation. `NOTICE_FILES` retains
both licenses.

## Scope, review and continuation

No search syntax, Unicode, ordering, scanner/path policy, host limits,
cancellation, UI, persistence or dependency pin changed. No speedup is claimed
and no benchmark was required. A focused self-review compared the relocated
wrapper's options/error mapping with the baseline and checked lifetime,
interface boundaries, unconditional core registration and production wiring.

No human/native/independent acceptance is required for this bounded relocation;
no attended candidate or review server was created or modified. Original
RFC-BPR-0077 evidence is preserved. Native Windows/Linux and complete package,
vault, crash/vulnerability/release obligations retain their existing cards and
are not certified by these macOS courses.

The leaf owns only this milestone. Parent remained read-only; no other repository
writer or subagent was used. All command sessions must be quiescent before the
final handoff at `/tmp/bpr-core-re2/handoff.md`. No live GitHub mutation, push,
owner desktop access, browser/private application data or package publication.
Raw command receipts remain privately under `/tmp/bpr-core-re2/`; committed
records contain curated non-secret results only. No successor milestone is
assigned; the coordinator can report this completed mandate.
