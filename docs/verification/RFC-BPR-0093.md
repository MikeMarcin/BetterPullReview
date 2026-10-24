# RFC-BPR-0093 — process-wide mimalloc

## Scope and candidate

Mandate r1 (2026-10-24) implements the supplied pack and the owner's confirmed
process-wide scope. Source/tracker: `/Users/jaeger/Documents/GitHub/BetterPullReview`,
branch `main`. Immutable RFC base: `e82537ad1` (full SHA on [[BPR-1764]]).
Imported/adopted in `5829f354e`; executable composition is `b2c4f7f1d` and the
corrected probe is `81ee1b4ef`. No push, release publication or release approval.

The first milestone is **not accepted** until its native Intel and signed
installed obligations are met. [[BPR-1758]] owns acquisition/graph policy;
[[BPR-1759]] owns macOS native observations. [[BPR-1762]] and [[BPR-1763]] retain
final package/source and measurement acceptance.

## Acquisition and graph evidence

At `b2c4f7f1d` (provider unchanged at `81ee1b4ef`):

- Downloaded official v3.5.3 archive: 1,455,942 bytes,
  SHA-256 `3b4a15153a59905995f7070296ed604bb5ccc00cabb8b93446931aff77224d47`.
  Its MIT `LICENSE` has SHA-256
  `82ade5d7d9b029044b5fbbc326207fc47c2d44ee4dd71e8a559c36d728217de9`.
- `python3 tests/scripts/mimalloc_provider_tests.py ROOT CACHE`: pass.
  Offline verified archive, missing/wrong cache, four source-substitution names,
  four target collisions, baseline, distribution refusal, sanitizer/fuzzer
  exclusion, selected product attachment and edited-extraction restoration.
  The initial harness matched an unwrapped CMake diagnostic; normalizing its
  whitespace repaired that harness assertion before the passing run.
- Actual offline repository configures: core-only (Qt/agent/tests off) has no
  allocator source/target; Release baseline CLI has `bpr` and its supervisor,
  with no allocator; distribution agent-free/automation-off has desktop,
  supervisor, update helper and mimalloc, without MCP/CLI targets. Each selected
  executable's generated link command names the shared allocator.
- The actual Release baseline `bpr --help` exits zero and its Mach-O dependency
  list contains no allocator runtime. This is a working baseline, not a
  performance comparison. The first dependency assertion mistakenly searched
  the command's path header (which contains this task's `mimalloc-m1` name);
  checking only dependency rows corrected the assertion.

## macOS native course

Host: macOS 26.6.2 (25G83), arm64, AppleClang 21.0.0.21000101,
Qt 6.12.0, Debug, tests enabled. `BPR_BUILD_TOUR_CLI=ON` selects all five
product executables. The five `mimalloc_*` product rows invoke those actual
images with a tests-only pre-main diagnostic unit; ordinary startup/shutdown
is verified separately. The loaded fixture links Qt Core and never mimalloc.

The initial seven-row `ctest --preset dev -R '^mimalloc_' --output-on-failure`
passed at `b2c4f7f1d`: provider, desktop, `bpr`, MCP helper, supervisor, update
helper and missing-override negative control. Each positive row observes
mimalloc ownership of C/C++, Qt and separately loaded-module pointers, safe
cross-module frees, 4,096 boundary iterations, cross-thread release, and C and
nothrow-C++ allocation failure. A separate executable without the override
fails the same probe with missing ownership symbol, as required.

Clang-tidy found a realloc assignment in the test probe which could lose the
original pointer on failure. `81ee1b4ef` fixes that and extends the course to
array/throwing new and exactly one loaded allocator image. Focused tidy 22
passes after the correction. Full native Intel and signed installed evidence
remain pending; development results do not satisfy them.

## Runtime evidence and continuation

Task-owned transient evidence is under `build/mimalloc-m1/` (untracked):
selection/checkpoint JSON, configure logs, graph observations, build logs,
provider/native/integration logs and baseline result. No credentials or personal
application data are retained there. Current run `mimalloc-20261024`, segment
`macos-m1`, mandate `r1`; selected recipe `coordinator-implementer` revision 2.
The durable current frontier is on [[BPR-1764]].

## macOS candidate prerequisite amendment

On 2026-10-24 the coordinator moved only the macOS package prerequisites for
AC03 into [[BPR-1759]]: the exact shared-runtime producer/deployment inventory,
MIT notice and pinned corresponding-source entry. This resolves the cycle where
the native card required an installed candidate before [[BPR-1762]] could start.
The release script, package-input producer map and closed private-library audit
are the source basis. The authored outcome and exits are unchanged. Final AC07
for all platforms and standalone CLI remains on [[BPR-1762]].

The `publish-release` skill was loaded for this preparation. Only its local
`--prepare-only --skip-tests` route is selected for the unpublished 0.3.2
candidate. Its ad-hoc signature and relocated launch are development evidence;
Developer ID, notarization, Gatekeeper and signed-installed AC03 are pending.
No keychain lookup or Apple submission is part of prepare mode.

The corrected native course at `81ee1b4ef` passes all seven rows in 11.14 seconds.
The 17-row focused integration passes in 356.72 seconds: allocator rows, real
CLI entry/Review Cycles, MCP process/terminal behavior, desktop offscreen render,
update transaction and architecture/core boundary checks and their contracts.
Documentation's 67 tests and the gate pass. Touched C++ format and tidy 22 pass;
tidy's initial realloc finding is retained in the transient `tidy.log` and its
passing correction in `tidy-fixed.log`.
