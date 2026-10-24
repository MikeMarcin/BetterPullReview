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

## Final unattended checkpoint, 2026-10-24

Latest product-source commit: `aadf2d91d`. Reconfigured/rebuilt the selected
Debug product graph and reran all seven allocator rows: pass in 15.90 seconds,
including exactly one loaded allocator and the real major-version runtime file.
Package runtime tests: 30 total, 29 passed and one native-Linux-only case skipped; package
audit tests: 9 pass; source-asset tests: 24 pass. The initial package test run
found the expected component/count assertions still describing the old inventory;
those expectations were updated to include mimalloc, then the suite passed.
Documentation tests (67), documentation gate, shell syntax, Python compilation
and `git diff --check` pass. No repeat of unchanged comprehensive integration
was required after a library output-name/package-inventory change.

Local preparation is blocked at its clean-source preflight: concurrent changes
to `docs/issues/NEXT_ID` and the BPR-1765 reservation are unrelated and untouched.
No `local-candidate` app/archive was created; no release signature, Keychain
lookup or Apple submission occurred in that invocation. Native Intel,
signed-installed AC03, final AC07 and AC08 remain open. The prerequisite
amendment is recorded on BPR-1759; no acceptance wording, exit or authored
milestone dependency was waived. The first milestone remains unaccepted.

## Clean-worktree continuation, 2026-10-24

The coordinator authorized a task-owned clean worktree to continue while main's
unrelated NEXT_ID/reservation remain dirty. Source/tracker/runtime root is
`/Users/jaeger/.codex/worktrees/mimalloc-macos-candidate/BetterPullReview`, branch `codex/mimalloc-macos-candidate`.
Run `mimalloc-20261024`, segment `macos-m1-clean-worktree`, mandate `r1`.
The product source candidate is `eb28c0187a49143f5470e4400c2f1b3d0cb03de0`; no product source repair was
needed. This continuation supersedes the dirty-source blocker for this branch.
Main and its unrelated allocation files remain untouched.

`tools/release.sh --version 0.3.2 --prepare-only --skip-tests --output-dir
build/mimalloc-m1/local-candidate` completed successfully from clean source.
The existing native workflow built universal tests-disabled distribution Release,
deployed Qt and `libmimalloc.3.dylib`, normalized bundle-relative dependencies,
created notices/SPDX/separate source ZIP and applied its offline ad-hoc seal.
No Developer ID operation, Keychain access, notarization, Gatekeeper assessment,
publication or owner-desktop operation was performed.

The relocated offscreen assessment clone passed private-profile and production
bundle-identity checks. The delivered bundle stayed byte-identical during that
assessment (bundle fingerprint
`6abb5cf7f428ebe9801cca0ada237c1043c4e9b87dea5d7c755e859e4edc6f4f`).
The delivered Cocoa launch remains separate. Both slices contain the allocator's
interpose section; that is build evidence and does not supply native Intel proof.

- Full `scripts/verify_release_package.py` audit: `RELEASE_PACKAGE_OK`, 52 files,
  19 binaries, 519 first-party compile units. Trust is `not-assessed`, legal
  approval `pending`. The default LLVM 23 `llvm-nm` could not start because its
  Z3 dylib is absent. Selecting the existing `/opt/homebrew/opt/llvm@22/bin`
  for this command fixed the tool prerequisite without changing global settings.
- `packaging/verify_release_sources.py`: `RELEASE_SOURCE_ASSET_OK` for the exact
  clean producer revision and separate source ZIP, including pinned mimalloc.
- The public package auditor refuses a private copy missing `libmimalloc.3.dylib`
  with `required-packaged-capability-missing`; it refuses a copy with an extra
  allocator dylib with `package-file-set-mismatch`. The original is unchanged.
  The first local control harness used a noncanonical `/var` root; resolving
  its temporary root repaired that harness assertion before both controls passed.
- All selected product images and the allocator have arm64 and x86_64 slices;
  the tests-only `BPR_MIMALLOC_PROBE` marker is absent from delivered images.

Artifacts are under `build/mimalloc-m1/local-candidate/` in this retained worktree:

| Artifact | SHA-256 |
|---|---|
| `BetterPullReview-0.3.2-macos-universal-local.zip` | `9ca6dec76680d7a368314659bdcae3c8e3a93bcddf564f1b8e54cdbebfec9660` |
| `BetterPullReview-0.3.2-macos-universal-local.spdx.json` | `54815048e932a709072ccee40a2ee9dd1507364f67ed94ff56f6052e0c66c33b` |
| `Source-BetterPullReview-0.3.2-macos-universal-corresponding-source.zip` | `fc15e56132603872e35f91fb57142c910558c08fa7e4f1e60261ae4d8b5f75e2` |
| `Contents/MacOS/BetterPullReview` | `cfeb620c9c601a496b0b82e9a4b0750b5064145040938ad624d82074884dfeec` |
| `Contents/Frameworks/libmimalloc.3.dylib` | `2d72e966ed52f2d5c9d3dee7d44425d39165b6e1c89edb7f96aadefade29da83` |

The first two files are local preparation artifacts; their names do not grant
release status. Full producer/helper identities and command outputs are retained
in `build/mimalloc-m1/candidate-identities.json`, `prepare-candidate.log`,
`package-audit-llvm22.json`, `source-asset-audit.json`,
`negative-controls-public.json` and `producer-interpose.json`.
Initial tool/harness failures remain in their separate stderr files.

BPR-1759 and this milestone remain incomplete. Native Intel allocation ownership,
the authorized Developer-ID/signed-installed course, delivered native Cocoa
qualification and final all-platform AC07 remain pending. AC08 is untouched;
existing BPR-0967 limits are not relabeled. No attended session is ready and no
review server exists. All task-owned commands are finished. Preserve this
worktree, its branch, build and candidate artifacts for the next engineering
round; the coordinator owns integration after main's other writer reconciles.

## Independent Windows continuation — mandate r2, 2026-10-24

The owner said, “I'm pretty sure we can keep working without an intel mac.”
Only Windows' authored scheduling dependency on full macOS milestone acceptance
is removed; its accepted provider prerequisite [[BPR-1758]] is retained.
The source basis is the independent Windows provider/CRT route and separate
AC03/AC04 courses. `check-amendment` passed with only `windows-integration`
affected and every outcome/work selector/exit preserved. Native Intel and
signed-installed macOS AC03 remain open, with the existing candidate retained.
Linux scheduling, final package/source acceptance and measurement remain open.
Run `mimalloc-20261024`, segment `windows-m2`, mandate `r2`, unchanged recipe.
