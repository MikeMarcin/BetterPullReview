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

## Windows milestone result — mandate r2

`windows-integration` / [[BPR-1760]] is accepted for native allocator behavior.
The first macOS milestone remains incomplete; no Intel or signed-installed
criterion was waived. Linux scheduling is unchanged. AC07's final installer,
standalone CLI source/legal inventory and all-platform closure, AC08 and the
existing native/security gaps remain open.

Source/tracker is main at `/Users/jaeger/Documents/GitHub/BetterPullReview`.
The native Windows 11 x64 course uses the task-owned root
`C:/code/bpr-rfc0093-866ec50e723c`, with its own `src`, cache and builds.
The existing `C:/code/git/BetterPullReview` station source/branch was preserved.
It supplied read-only Git objects and pinned Qt; the normal configure applies
the existing AppContainer read grant to that Qt prefix. Commands run in native
Windows Python/MSVC, without a delegated model or WSL compiler.

The host reports Windows build 26200, MSVC 19.51.36260.0 (VS 18), Qt 6.12.0
and CMake 4.4.2. Runtime candidate
`0ccf31bc4a668a9cc71be93afb6664a0bc70225b` supplies the default distribution
install (`installed-final`) and the four-product Debug course. The separate
tests-disabled CLI build is from
`c5693fd05e9a673381cae5aa76c355db33e39182`; its product/build inputs are unchanged
from the runtime candidate. Its `TourCLI` install (`cli-assessment/bin`) is
assessed with byte-identical Qt/CRT/TLS files from the audited runtime and its
own xdiff producer. This is native CLI interception evidence; the standalone
CLI's final distributable inventory remains on [[BPR-1762]].

### Native observations

- `ctest --preset dev -R '^mimalloc_' --output-on-failure`: seven pass. All
  four actual product images observe C/C++, aligned/array allocation, Qt and a
  separately loaded module, 4,096 cross-module iterations, cross-thread frees,
  C/nothrow/throwing failure semantics and exactly one allocator. Missing override
  and disabled CRT redirection are refused. Import tables put mimalloc first and
  use the dynamic CRT. Static CRT selection/flags are refused.
- The provider row passes again with the new native regression: replace only a
  private fixture's deployed DLL, build its up-to-date EXE, observe no relink and
  require restored producer bytes. Every real Debug runtime copy also equals its
  producer (`888eb8c82d8825438c4b929201452f0043ea4cdc3837579564008225d3062fc9`).
- External Microsoft CDB 10.0.26100.9457 observes actual allocation returns in
  the tests-disabled desktop, supervisor, MCP helper and CLI. A same-process
  `mi_is_in_heap_region` call returns true for C and global C++ new pointers.
  The desktop and CLI also show Qt allocation and an owned Qt-to-mimalloc free
  returning to Qt. Disabling redirection refuses the C++ observation. Symbols
  remain outside the payload; no allocator diagnostic seam is shipped. CDB was
  obtained through a signed Microsoft SDK layout and administrative extraction
  inside this root, without installing a system feature or changing signatures.
- The installed desktop's shipped `--self-test --automated-run-budget-ms 20000`
  reports `AUTOMATED_PROFILE_PRIVATE_OK` and `PUBLIC_PATH_OK` (68 ms, five stage
  ticks). The isolated launch retains the required Git executable on PATH.
  The installed CLI passes `tests/scripts/cycle_cli_tests.py` in its near-empty
  environment. Debug `cycle_cli_public_path`, `application_render` and
  `application_mcp_context_helper` pass through real processes/composition.
- Full public `verify_release_package.py` passes for the runtime: 31 files,
  23 binaries and 511 first-party compile units, with full bound producer maps,
  exact Qt/CRT bytes, MIT notice, SPDX and separate source asset. Trust is
  `not-assessed`, legal approval `pending`. No MSI or release signature is claimed.
  Private copies missing either allocator DLL are refused with
  `required-packaged-capability-missing`; an extra allocator is refused with
  `package-file-set-mismatch`. Original installed bytes remain unchanged.

| Runtime identity | SHA-256 |
|---|---|
| Desktop | `b85bc76a49fb69015544afb593ab994a28f95ee918caef0081e16e0f8b59184e` |
| Supervisor | `16084d9ebedd3985f9233c77680d033357dd7616ca93bbda5294949917281058` |
| MCP helper | `e7251dfb66c52687f9e60e9e6d96d1c5f7c4c91d343eb589ee30c45764428dd4` |
| Runtime `mimalloc.dll` | `a0a86b2eae7c457fd0359b015f0fd54f2b5b75b4fcf9982171c8dc61c6e733dc` |
| Upstream x64 redirect DLL | `bc3dfdf51fbc80b88cb62cdb6a6784ecc894164bdd4ae7f3885048d870809491` |
| Runtime SPDX | `de43b7f28c3f9c7fdd7143abbd7543e77883f7cf899f0d609d510ded192584fe` |
| Separate Windows source ZIP | `817072787958cf7a7eabbaa14563d439d8fb2682e097a1af2967ebc9f2527e97` |
| CLI `bpr.exe` | `7ba166deee47a81719eeed0b9227ea0fd5b64d1c28425e5e9ca186fd95ae53a5` |
| CLI `mimalloc.dll` | `56e4839f1959628dad6224079671f6b91c66a5ecb6b184a3be39384ea6a0524d` |

The source ZIP keeps its generated name
`Source-BetterPullReview-0.3.2-windows-x64-corresponding-source.zip`.
No artifact was signed, pushed or published. The `publish-release` skill was
loaded for the local runtime/source audit; release qualification was not run.

### Corrections and retained limitations

The initial native build found missing SDK declarations and include ordering in
the portable probe; those were corrected. CMake fixture paths now use forward
slashes and the shared owner-private scratch helper. The debugger exposed stale
helper DLL copies after a DLL rebuild with unchanged import-library bytes.
`0ccf31bc4` makes deployment a product build dependency, and the exact candidate
passes again with matching copies. The first Release debugger harness used
`gu`, which stopped at an inlined frame; using the actual machine return address
corrected that harness. A desktop `--help` launch is a typed argument refusal;
the separate shipped self-test supplies normal composition/startup/shutdown.
The initial self-test lacked required Git on its deliberately minimal PATH;
adding only the installed Git path passes.

The first package audit refused the MCP helper's SDK `.idata$6` record: putting
mimalloc first selected its shared null import terminator. `8467eaaed` accepts
only that exact mapped data owner with an actual mimalloc import, retaining the
existing independent SDK specimen pin, zero-RVA/static/section/absence checks
and full symbol scanning. The unchanged native SDK specimen digest is
`9fe31df255d17b0339391ff80a71bf2c5d442cc301bd2fe6eeea00b626f10785`.
Unknown owners, missing allocator imports and the previous malformed controls
remain failures. The redirector itself has its independent binary pin and is
excluded from first-party re-signing; the allocator requires its full map.
The payload auditor also refuses `BPR_MIMALLOC_PROBE`.

On this host the five Qt ownership/control rows fail contained at Qt DLL
initialization (Win32 1114 / `0xc0000142`). Plain `cmake --version` independently
fails contained with `0xc0000142`. The six measured registrations therefore run
uncontained; the unattached negative control stays contained. The global
isolation manifest remains failed independently: its contained CTest child
cannot initialize, and a direct run names three existing unmeasured rows
(`sqlite_agent_setup_storage`, `workspace_clone_folder_scan`,
`application_agent_finding_codex_found_contract`). They were not changed. On
main, unrelated `.mask/sessions` evidence files also fail that global scan.
This is not reported as a passing full suite.

Focused local package tests pass (30, one Linux skip); Windows package tests
pass (30, two Linux/POSIX skips). Security controls pass 20 on both hosts;
package-audit controls pass 10, native-release mock controls 30 (one native
Windows-only skip on Mac), and source-asset controls 24. Documentation's 67
tests, the documentation gate, touched format/tidy 22, Python compilation and
`git diff --check` pass. Unchanged passing evidence is reused.

### Continuation state

Run `mimalloc-20261024`, segment `windows-m2-native`, mandate `r2`, recipe
`coordinator-implementer` revision 2. The installed recipe changed externally
during setup (attendance hosting text). After native configure stopped, the
unchanged milestone was rebound using the read current recipe; original and
current selection snapshots are retained. No requirement or accepted evidence
was replaced by that reconciliation.

`build/mimalloc-windows/` holds the local selection/binding, amendment receipt,
focused logs and copied native identity/observation/control records. The native
root retains complete source/builds, installed runtime, CLI assessment, CDB
commands/logs, source ZIP and producer maps. `final-identities.json` and
`cli-identities.json` bind every assessed file. Task images are quiescent,
negative-control copies and the superseded runtime tree were removed, and no
review session/server exists. Keep the remaining native root for follow-up.
Unrelated MASK and BPR-1766 allocation edits on main are preserved. Return at
the Windows milestone boundary; the coordinator owns the next scheduling
choice. Mac Intel/signed-installed and all remaining RFC obligations stay open.

## Independent Linux continuation — mandate r3, 2026-10-24

The owner's instruction to keep working without an Intel Mac now selects the
independent Linux course. Only `linux-integration`'s prerequisite on complete
Mac acceptance is removed. Its authored outcome and exit and every Mac native
criterion remain unchanged. Accepted BPR-1758 still gates BPR-1761. The checked
amendment and current recipe selection are retained under
`build/mimalloc-linux/`; run `mimalloc-20261024`, segment `linux-m3`, mandate
`r3`. The selected station reports Ubuntu 26.04 x64 on WSL2; source/build
isolation and actual native/installed observations remain to be established.
