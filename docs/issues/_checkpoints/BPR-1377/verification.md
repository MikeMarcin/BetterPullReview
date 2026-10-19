# BPR-1377 verification

Assignment `/root/tour_ready`, local main; mandate `owner-ready-backlog-20261018-r2`.
The owner's amendment says older cards may be obsolete: current sparse-source
reproduction and the still-unreachable unpinned CLI profile established this
card's relevance. Insights/RFC-BPR-0086/BPR-1608–1615 remain excluded.

Source candidate: `530a7b94d4a196130320724a14988b65a2c1eab4`. Initial defect baseline:
`55ea64194211eb42194d6dd9fc3e7ae11c9aefa4`. RFC-BPR-0044's existing execution
RFC_BASE_SHA remains `89f23eeece835ecafb9f8752b524644b22fbf281`; this defect
closes no parent RFC or live tour qualification follow-up.

## Criterion evidence

| Criterion | Direct evidence | Result |
|---|---|---|
| AC1: add/show exact pinned source over sparse gh trees without desktop or broader blob authority | Public `materialize_review_tour_source` through sparse fake cache/transport for LEFT and RIGHT; per-call sealed endpoint checks; live CLI below | Pass |
| AC2: sparse fixture fails before the fix and passes after | Replaced only the source helper's new call with the former ordinary `materialize` call, rebuilt epoch host suite, and observed the exact source assertion fail (CTest exit 8). Restored bytes, rebuilt and reran | Pass |
| AC3: live BPR-Test add and source readback | `bpr tour connect/open/add/show --source --transport gh` on MikeMarcin/BPR-Test#49, isolated private data root | Pass |

The regression's host fixtures cover forged endpoint path and comparison key,
256 KiB refusal, pre-request Stop, ordinary reads before and after source
materialization, comparison replacement and account/review invalidation. The
source-specific remote read verifies physical commit/tree presence and the Git
blob digest, and final publication rechecks current authority. It creates no
selected-file or prepared-file grant.

The required CLI path also exposed a current prerequisite failure:
`tour_remote` created an empty `CliAccountLogin`, which the shared strict
factory rejects before running gh. The coordinator authorized its narrow
repair. Existing bounded inventory now selects exactly one active, successful,
keyring-held login on SourceEffects and pins it before strict transport
admission. Three isolated executable stubs verify refusal of inactive,
environment-only and ambiguous active accounts (exit 5 authentication_required).
There is no global account switch, credential fallback or new token capture.

## Commands and observations

- Lean Debug graph: `cmake --preset tour-cli -B build/bpr1377-tests -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TEST_BUILD_DEPENDS=OFF`.
- Built `app_bpr`, `test_better_pull_review_application_epoch_host_tests`, `test_tour_cli_composition`, `test_review_tour_host`.
- `ctest --test-dir build/bpr1377-tests -R '^(application_epoch_host|application_review_tour_host|tour_cli_composition)$' --output-on-failure`: 3/3 pass, 4.51 s after restoring the negative control.
- `_coding-standard/scripts/format.py --dry-run` on six touched C++ files: clang-format 22.1.8 passes. Scoped `git diff --check` passes.
- CMake's round-trip contract in provider-inventory mode: `ASYNC_MUTATION_HOST_INVENTORY_OK`. Current full app/manifest pins were deterministically resealed after inspecting the concurrent BPR-1619 factory/header wiring. Its engine acceptance remains that card's responsibility; the production mutation digest is unchanged.
- `python3 scripts/verify_documentation.py --root .`: DOCUMENTATION_OK after the concurrent BPR-1619 verification target was published. Its earlier missing-link failure was recorded and not repaired in this scope.

Live course: tests-enabled, non-GUI development `bpr`, no fixture selected.
GitHub target: https://github.com/MikeMarcin/BPR-Test/pull/49.
Pinned head `2e1d8b415ce33ad1284b2f2ca34e82734c07ddc1`, path
`bpr-2acct/identity/review-me.txt`, RIGHT lines 1–2, blob
`a4c178354da79ee0a3b425d7f0f319e5defe5fba`. `add` exited 0 in 38.87 s;
`show --source` exited 0 in 34.87 s and matched exact API source text, commit,
path and side. Only local draft state changed. No GitHub content write or
desktop was used. Response-header budget observations: 4978/4977 before,
4745 after. The final helper extraction preserved the selection predicate;
final rebuilt composition/host checks and account-refusal controls passed.
This supplies the card's read course, not package/release qualification.

## Failures, scope and cleanup

Initial commands included an unavailable GUI-only haves target, a concurrent
xdiff path move during compilation, two local compile diagnostics corrected
before verification, and a fixture setup missing the accepted sparse PR shape.
All were diagnosed and corrected or scoped away before the passing run.

The old full gh loopback harness was explored but has unrelated setup drift
(vault, denied trace, help, named credentials and merge-base/epoch setup).
Exploratory harness edits were removed. The committed public sparse host fixture
and live CLI read course satisfy the original alternative fixture-or-loopback
criterion. No loopback success is claimed.

Raw logs remain outside Git at `/tmp/bpr-closeout-20261018/tour-*.log` for the
coordinator. No credentials, build outputs or raw logs are committed. No human,
native-device or independent acceptance remains for this defect. No review
server or protected candidate belongs to this milestone.
