# RFC-BPR-0093 — process-wide mimalloc

## Intel acceptance amendment — mandate r5

On 2026-10-24 the owner explicitly answered **“Amend Intel criterion”**. The
coordinator relayed the decision to sole implementer `/root/mimalloc_intel_amendment`:
accept the verified universal binary/static x86_64 evidence and native arm64
observations without requiring an Intel run. AC03, BPR-1759 and the live
`pinned-macos-integration` exit now carry that exact acceptance boundary. No
native Intel execution occurred or is claimed. This changes acceptance, not
observations; all other criteria, work selectors, dependencies and the immutable
source pack remain unchanged.

The source basis is the [current r4 Mac candidate](#current-mac-candidate--mandate-r4):
app producer `75baca275aa8f052997a231a83ebc7d77d27ab6d`, CLI/product source
`df6e8be48ff17271e2258f0040e4ca87795f44bd`. Their package audits and external
native arm64 ownership/boundary observations are retained with their original
scope. Read-only r5 `lipo -archs` and per-slice `otool -L`/`otool -l` checks
confirm arm64 and x86_64 in all six selected app/CLI images and both private
allocator images, each product's allocator dependency and each allocator's
`__interpose` section. All eight image hashes equal the recorded candidates.
These static checks do not establish native Intel runtime behavior.

The `check-amendment` helper was run before publication. It correctly refused
with `amendment must preserve all exit criteria, including pending decisions`
(exit 2): its refinement contract does not authorize replacing acceptance exits.
The explicit owner r5 decision separately authorizes that one requirement
replacement; the refusal is retained, not reported as a passing check. The
proposed/current plan comparison confirms only the macOS exit changed, with
plan identity, scope, work, dependencies, other exits and outcomes preserved.
The current binding is renewed under r5 rather than reusing the old fingerprint.

Documentation gate and all 67 documentation tests pass. The focused consistency
check confirms exact AC03/closeout agreement, only the authorized macOS exit
replacement, correct remaining card/RFC states, unchanged immutable source/receipt,
unrelated-file hashes and retained candidate/artifact hashes. Plan validation and
regenerated-view comparison pass. Other product behavior reuses its unchanged
native evidence; this documentation-only round requires no product rebuild.

BPR-1759 and `pinned-macos-integration` remain incomplete on the actual
Developer-ID signed-installed course and delivered native Cocoa acceptance.
BPR-1762/AC07/AC09 and BPR-1764/RFC closeout remain pending. No local ad-hoc
seal, pre-main observer or static proof substitutes for those remaining courses.
No signing, release, desktop or Keychain operation was performed in r5.

Runtime records are `build/mimalloc-intel-amendment-r5/` in the main source/tracker
root: mandate, original/proposed plan, guard refusal, renewed selection/binding,
static proof, focused consistency/preservation checks and final handoff.
The current and prior candidate roots remain protected and unchanged. No review
session, event cursor or server is created for this documentary amendment.

Earlier sections below preserve their r1–r4 scope and observations. Any statement
that Intel execution remains required is historical and superseded by r5 above.

## Historical scope and candidate — mandate r1

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

## Linux milestone result — mandate r3

`linux-integration` / [[BPR-1761]] is accepted for AC05 and the Linux portion
of AC06. The authored outcome and exit are unchanged. Native Intel and signed
installed Mac AC03, final AC07, measurement AC08 and final AC09 remain open.
The source candidate is `0c40cca40e5d93205d83fd65f76af029ac937347`; subsequent
tracker/documentation edits reuse unchanged product evidence. No release,
signing, push or publication occurred.

### Native station and installed candidate

Ubuntu 26.04 x64 on JaegAW WSL2/WSLg reports kernel
`6.18.33.2-microsoft-standard-WSL2`, GCC `15.2.0-16ubuntu1`, glibc
`2.43-2ubuntu2.4`, Qt 6.12.0, CMake 4.4.2 and Ninja 1.13.2. Native drivers use
Python 3.14.4; the configured tool PATH finds Python 3.13.15. All source, cache,
builds, installed trees and diagnostic tools are inside the owner-private root
`/home/mmarcin/bpr-rfc0093-linux-dfzpprt6`. The original
`/home/mmarcin/BetterPullReview` stays clean and supplied only read-only Git
objects and pinned Qt. Native commands use SSH/WSL directly, without another
model or station writer.

The Debug graph selects desktop, `bpr`, MCP helper and supervisor. The
`linux-package` Release/tests-disabled graph also selects `BPR_BUILD_TOUR_CLI=ON`.
`DESTDIR=.../installed cmake --install ... --component Runtime` installs beneath
`installed/usr/lib/betterpullreview`; a separate `TourCLI` component install
uses `cli-installed/bin` and `cli-installed/lib`. No file was manually added to
either payload. Runtime deployment builds the exact hardened ICU producer and
binds the complete source/legal inventory. Standalone CLI final source/legal
release inventory remains BPR-1762's obligation.

### Direct observations

- `ctest --preset dev -R '^mimalloc_' --output-on-failure`: six pass. All four
  real product images observe C/C++, aligned/array allocation, Qt and a
  separately loaded module, 4,096 cross-module iterations, cross-thread release,
  C/nothrow/throwing allocation failure and exactly one allocator. The same
  probe refuses an executable without the override. Provider controls pass.
- Final candidate integration selector: 16/16 pass in 166.00 seconds. It covers
  the six allocator rows, `cycle_cli_public_path`, MCP, real Git and gh process
  boundaries, agent launch canary, application render, ZIP bundle/maintenance
  tests and both architecture boundaries. Local `core-only` configure/build and
  the four focused ZIP/boundary rows pass. No passing full-suite claim is made.
- External GDB 17.1 observes C and global C++ new pointers, loaded-module and Qt
  pointers, safe C/C++ cross-module frees, and C/nothrow/throwing failure behavior
  inside all five actual installed images: Runtime desktop/supervisor/MCP and
  TourCLI `bpr`/supervisor. Each process loads exactly one allocator from its
  own installed prefix, and the actual installed Qt Core. Test modules stay
  outside the payload; no diagnostic seam is shipped. `ldd` resolves every
  selected image without a build-tree loader environment.
- The exact installed desktop runs its shipped `--self-test
  --automated-run-budget-ms 20000` through the installed XCB plugin on a private
  Xvfb: `AUTOMATED_PROFILE_PRIVATE_OK`, `PUBLIC_PATH_OK`, four stage ticks,
  45 ms. A user/mount namespace supplies the private X socket and task-local
  XKB tool; its mapped root is scoped to that namespace. This is composed
  startup/shutdown evidence, not ordinary-user desktop/vault qualification.
  No owner desktop was driven or captured; Xvfb exited and its auth file was
  removed. Runtime bytes remained unchanged.
- Installed `bpr` passes the complete real-process Cycle CLI course in its
  near-empty environment. Installed MCP serves a synthetic frozen file and
  refuses a path outside its snapshot, then exits zero. Installed supervisors
  launch actual Git 2.53.0 and gh 2.97.0 version commands plus an external
  agent probe. `execve` traces show no loader injection variables; the agent
  probe sees no allocator symbol. The CLI supervisor passes the same agent
  check. A disposable deliberately preloaded control is refused (exit 74).
- Full public `verify_release_package.py` passes: 27 files, 20 binaries,
  528 first-party compile units, retained MIT notice, SPDX and separate source
  asset. A private copy without mimalloc is refused with
  `required-packaged-capability-missing:libmimalloc.so.3`; an extra allocator
  is refused with `package-file-set-mismatch`. Original payloads are unchanged.
  Trust is `not-assessed`, legal approval `pending`. No DEB/system installation,
  signing, native trust or final all-platform AC07 is claimed.

| Installed identity | SHA-256 |
|---|---|
| Runtime desktop | `453bb4d1cfd848b53ab4c8f04d37512751121d112b388f8f0303727b1bb02e42` |
| Runtime supervisor | `821a4aa746b81b16818984d36edfd522b73a87ff85969230fd6cde364cdeb507` |
| Runtime MCP helper | `8937459c366b6716a8579ae73ff99fd6cce425e1ec0470512474b8962027bd33` |
| Runtime allocator | `4424527db6dadffdfc3515e490a3acac5922d21b6c199426919be2820f7ff35b` |
| Runtime SPDX | `98329f0fdd100386c3d1c578a16c296f953dbfead5a21d0212bff79240a1fafb` |
| Separate Ubuntu source ZIP | `a8cc1eb75de7ffe3a5d20c60416b023953a25aa6903470177abb01a5645e2cf1` |
| Installed `bpr` | `e14be656210a75ea6c0bff0bf12dfa3da4ebf297e1dd9b69d25d80826c67ea5d` |
| CLI supervisor | `4982efb7ba1eb203a32848fa20add3077d0125d6d9774d6b7c0a1aabcb998019` |
| CLI allocator | `1202bde82c4b104047ba3e0fa455fb7afe23c21709ceefac40543ce3f040afdf` |

The source ZIP retains its generated name
`Source-BetterPullReview-0.3.2-linux-ubuntu-26.04-x64-corresponding-source.zip`.
Runtime deployment sets its private ELF RPATH, so Runtime and TourCLI allocator
bytes differ; both loaded origins and exact file identities are recorded.

### Corrections, verification and limits

The first native probe exposed a real throwing-new abort: upstream's C-built
ELF override cannot throw `std::bad_alloc`. Linux now selects upstream C++
compilation, preserving that failure behavior, with standard ELF hardening.
Linux probes and installed producer/closure declarations were missing; they
now include the real SONAME file. Standalone CLI also installs xdiff and the
supervisor can resolve either supported layout. Nothing adds allocator APIs
inside core/host/adapter contracts.

Native compilation exposed two existing portability defects: a tray enum
shadowed its local names array under GCC's warning-as-error gate, and an
optional-string ZIP payload triggered GCC's optimized uninitialized warning.
The first ZIP initialization-only attempt still failed; keeping owned payload
storage separate from optional compression removes the diagnostic without
changing ZIP behavior. Exact GCC `-O3 -Werror` compilation, ZIP tests and core
boundaries pass. The tray edit is a mechanical rename, with no visual change.
Upstream mimalloc's nonfatal GCC atomic/string-overflow warning remains in the
Release build log; it was not suppressed or converted into a first-party pass.

Harness corrections are retained: the first shortest command omitted the
sandbox executable; a later final selection used a wrong build target and left
two tests unbuilt. Correct targets then passed all 16 rows. GDB needed explicit
C++ expression language and canonical path comparison for `bin/../lib`.
Task-local display/trace dependencies and a private X socket namespace resolved
the minimal WSL tool environment; the host's X socket directory and tools were
not changed. These failed attempts are not reported as product passes.

Package-runtime tests pass all 30 natively and 29 plus one native-Linux skip on
Mac. Package-audit controls pass 10, security controls 20, source-asset controls
24 and native-release controls 29 plus the explicit Windows-only skip.
Documentation's 67 tests, its gate, Python compilation, touched format/tidy 22
and `git diff --check` pass. Existing Windows/global isolation limitations,
BPR-0967 memory limits, native/vault/crash/vulnerability gaps and AC08 remain
unchanged; no release qualification or performance improvement is inferred.

### Durable continuation

Run `mimalloc-20261024`, segment `linux-m3`, mandate `r3`; unchanged
`coordinator-implementer` revision 2. Main is the authorized source/tracker.
The live closeout card holds the current checkpoint. Local
`build/mimalloc-linux/` retains selection, amendment receipt, machine binding,
focused logs and copied native identity/observation/control records. Complete
source/builds/payloads/source ZIP and raw diagnostics remain in the isolated
native root. All 40 recorded payload/source files retain their hashes; no
native task image or source writer remains. The station checkout stays clean.
Negative package copies were removed; remaining candidate roots are retained.

No attended session, cursor or review server is needed for this programmatic
milestone. Stop at the Linux boundary and return to the coordinator. Native
Intel/signed-installed Mac and final distribution/measurement/closeout remain
pending; later cards still retain their Mac prerequisites. No pending Mac
candidate or acceptance condition was replaced.

## Independent distribution and measurement — mandate r4, 2026-10-24

The owner authorized continued independent work without an Intel Mac.
[[BPR-1762]] and [[BPR-1763]] now retain the accepted provider/Windows/Linux
prerequisites without waiting for full Mac acceptance. The distribution
milestone already depended only on Windows/Linux; the checked plan amendment
adds an explicit `pinned-macos-integration` prerequisite to final closeout,
matching [[BPR-1764]]'s unchanged Mac dependency. All authored outcomes, work
selectors, exits and AC07/AC08 remain intact. Incomplete required Mac evidence
still prevents the applicable card/milestone and RFC acceptance.

Source basis is the independent platform implementations and installed courses,
the actual package/source inventory, compatible instrumentation path and
non-distribution baseline switch. `check-amendment` passed; only closeout's
prerequisite definition changed. Its receipt remains under
`build/mimalloc-distribution/`. Prior Mac/Windows/Linux candidates and their
pending decisions are retained. Run `mimalloc-20261024`, segment
`distribution-m4`, mandate `r4`, unchanged `coordinator-implementer` revision 2.
Sole leaf `/root/mimalloc_distribution` owns this milestone on main; unrelated
MASK/NEXT_ID/BPR-1766 edits remain excluded. No publication, signing,
credential/Keychain lookup, push or owner desktop operation is authorized.


## Distribution checkpoint — mandate r4

Run `mimalloc-20261024`, segment `distribution-m4`, mandate `r4`; sole leaf
`/root/mimalloc_distribution`, main source/tracker unchanged. Scheduling/Ready/
claims are `540b21fa9`, `d69b52bec`, `4dc9a5fc8`, `b9c598728`. BPR-1762 and
BPR-1763 are In Progress. The benchmark previously kept the toolchain allocator;
`571f96d45` explicitly attaches the selected allocator without adding a shipped
product or changing any course/limit. New standalone CLI preparation and public
source/inventory auditing are `1e269fdd3`, `df3f680a9`, `dce9ff8ce`; native
validation is still in progress, so these are implementation, not acceptance.

The actual AppleClang instrumented graph excludes mimalloc in both product
and fuzzer images and links ASan. All seven existing Qt-free corpus rows pass
under ASan/UBSan (7.39 seconds); the real CLI public-path course passes
(48.71 seconds in the final focused run). The first extra sanitizer selector
named an unbuilt workspace bundle executable; it is not a product failure or
passing coverage. Initial local Release/configure and sanitizer attempts hit
ENOSPC. Removing only task-owned copied Git-bundle bytes and rebuildable fuzz
objects recovered capacity; the later Release build and CLI course completed.
The copied binary and compile-graph identities remain in runtime evidence.
Package tests pass 31 with one explicit native-Linux skip on Mac, public-auditor
controls 11, source-asset controls 24, documentation tests 67 and the gate.

### Matched Release measurements so far

These are observations from one matched course per allocator/platform; no
speedup or universal latency claim follows. Workload source is unchanged from
`571f96d45`; Linux binary source is that commit, Mac source contents match
`dce9ff8ce`. Both are Release with the same toolchain, Qt, fixtures, feature
selection and machine; only `BPR_USE_MIMALLOC` changes. The raw reports retain
load/scheduling, exact SHA-256, timings and all failures.

| Native course | Baseline | mimalloc |
|---|---:|---:|
| Ubuntu process to main / Home | 9.43 / 326.76 ms | 9.65 / 293.14 ms |
| Ubuntu paged review/Git load | 865.67 ms | 649.25 ms |
| Ubuntu allocation-profile peak RSS growth | 177.79 MiB | 272.88 MiB |
| Ubuntu after-destruction RSS growth | 162.43 MiB | 272.12 MiB |
| Apple Silicon process to main / Home | 20.10 / 459.10 ms | 29.44 / 425.25 ms |
| Apple Silicon allocation-profile peak footprint growth | 146.38 MiB | 165.77 MiB |
| Apple Silicon after-destruction footprint growth | 101.88 MiB | 164.73 MiB |

The unchanged six-row Ubuntu selector passes launch/navigation in both.
Baseline fails loaded-close/resources (2/6); mimalloc additionally fails
cache-bytes/cache-entries (4/6). Apple Silicon baseline passes all six;
mimalloc fails loaded-close (1/6) at the unchanged footprint budget. These
additional failures and retained-memory increases are allocator regressions
on these candidates, not pre-existing BPR-0967 failures relabeled as fixes.
Historical BPR-0967 obligations remain separate. No heap trimming or limit
change was introduced.

The existing separate `--allocation-profile` diagnostic completes for both
allocators on both platforms and observes released diff/index/window/document
owners and a drained derived cache at shutdown/destruction. That non-gating
profile is supplemental; it does not turn failed unchanged budget rows green.
Linux glibc heap figures are unavailable for mimalloc, and upstream's Mac
zone `intro_statistics` deliberately returns zero `size_in_use`; zero in the
raw legacy heap fields is not a measured empty allocator. OS RSS/footprint and
native lifetime peaks remain reported. The GDB probe directly observes
mimalloc ownership in the actual Linux measurement image.

### Native installer/source work so far

An unsigned Ubuntu DEB was prepared from the retained exact Linux producer
`0c40cca40e5d93205d83fd65f76af029ac937347`, then extracted privately with
`dpkg-deb --extract`. Its full public runtime/source auditor passes. Installer
SHA-256: `cce54830fafc0de2c4c5aa42879fcc7b5aa7c4deb096c20d52d036a4e1d8ee30`;
separate source ZIP: `a8cc1eb75de7ffe3a5d20c60416b023953a25aa6903470177abb01a5645e2cf1`.
Original installed/source candidate hashes remain unchanged.

An unsigned Windows MSI was prepared from the retained exact source
`c5693fd05e9a673381cae5aa76c355db33e39182`. WiX 5.0.2 extracts the actual
cabinet; reconstructing its authored INSTALL_ROOT privately passes the full
public runtime/source/PE audit with bound maps and the existing SDK specimen.
MSI SHA-256: `bd01bc1a342d3d7957bee173284fa47c12af777f2dd5065cb2b0ae79e95aaa69`.
No MSI/DEB system installation, registry change, release signing or native trust
was performed. Offline legal approval remains pending. Existing native/vault/
crash/security qualification gaps remain open.

### Active ownership and continuation

Local evidence/scripts: `build/mimalloc-distribution/`. New native roots:
`/home/mmarcin/bpr-rfc0093-distribution-hrxjvs5j` and
`C:/code/bpr-rfc0093-dist-6165b97d2cea`. The earlier candidate roots are retained;
the old exact producers supply the new installer staging directories, not a
new source identity. No source writer owns those earlier trees.

At this checkpoint the native Linux standalone CLI build/preparation, native
Windows Release measurement build and local universal standalone CLI
configuration/build are active. No review session/server exists. Main's
unrelated MASK/NEXT_ID/BPR-1766 edits remain excluded. Next: complete native
CLI preparation and installed public/omission/extra/removal checks; complete
Windows matched measurements; reconcile every AC06/AC07/AC08 record. Final
Mac Intel/signed-installed and all-platform AC07 remain unaccepted.


### Fresh-configure defect found during CLI acceptance

The new Linux CLI passes real-process Cycle work and C/C++/Qt ownership, but
its installed throwing-new negative allocation aborts. Native inspection shows
`MI_USE_CXX:STRING=DEFAULT` and C compilation in that fresh graph. Upstream's
CMake 3.18 policy scope applies CMP0126 OLD: creating a string cache entry
removes the provider's normal `MI_USE_CXX=ON` and `MI_OPT_ARCH=OFF`. Earlier
accepted Linux candidates were reconfigured after the C++ correction and retain
their actual passing observations; they are unchanged. The new CLI is not
accepted. The first measurement candidates above also came from fresh graphs
and are superseded pending corrected candidate runs; their real failures stay
recorded, but they do not qualify the corrected runtime.

The correction preserves normal provider settings in the upstream subtree
with `CMAKE_POLICY_DEFAULT_CMP0126=NEW` and explicitly selects C++ for
Linux/MSVC (C for Mac interposition). A fresh minimal product executable now
actually throws/catches `std::bad_alloc`; its provider test also challenges
caller C++/architecture settings and inspects compiled architecture flags.
No source pin or allocation-failure requirement changes. Native focused
verification and rebuilt CLI/measurement candidates remain pending.


### Corrected candidate validation in progress

Source `b449356dd4f9ae81ea3a9792517fe33309d8ecab` fixes the first-configure
policy issue. The fresh executable/architecture regression passes on native
Mac, Windows and Linux. All seven Mac product/provider/control rows pass
again in 22.13 seconds; all seven Windows rows pass in 39.10 seconds.
The corrected Ubuntu standalone CLI now passes its full source/runtime audit,
real-process public Cycle course, installed C/C++/Qt and throwing-new/OOM
observations in both images, exactly one allocator from its installed prefix,
and missing/extra/relocation/removal controls. A first supervisor-module
harness loaded the fixture's provider Qt; explicitly loading the installed Qt
before that external fixture corrects the harness, and loaded origins are now
asserted. No payload bytes changed during the controls.

Universal Mac standalone CLI source/runtime audit, real-process Cycle course,
actual runtime-archive expansion/audit/relocation/removal, source binding and
missing/extra allocator controls pass. Both executable/helper and both private
libraries contain arm64 and x86_64 slices; only native arm64 execution is
observed. The ad-hoc seal supplies no signed-install or Intel acceptance.

Corrected Release comparisons remain measurement reports with real budget
failures, not performance approval. Windows baseline passes all six; mimalloc
fails the four memory/resource rows. Its initial direct diagnostic profile
lacked CTest's declared loader environment and exited before main; applying
the existing inventory environment makes both profiles complete. The source
workloads/limits are unchanged. Mac/Linux corrected reports supersede their
first fresh-library candidates; unchanged baseline evidence is reused with
identical common C++/fixture/toolchain inputs. Final measurements are still
being collected for the criterion-linked handoff.

Windows standalone CLI preparation reached the exact PE auditor and refused
`pe-map-symbol-address-mismatch`: its caller omitted the Runtime auditor's
explicit SDK specimen. Inspection with the already reviewed specimen
SHA-256 `9fe31df255d17b0339391ff80a71bf2c5d442cc301bd2fe6eeea00b626f10785`
passes the same bound map guard. The CLI route now forwards explicit
`--windows-import-library` and `--windows-redist-root` inputs through that
same guard, with a regression for argument propagation; no map/symbol rule is
weakened. Native Windows CLI preparation/control checks remain pending.


### Windows CLI map diagnosis

The bcrypt input resolves the supervisor, but `bpr.exe` has a second discarded
SDK record: static `.idata$6`, section 2, offset `ff939000`, image-base RVA
`0000000140000000`, owner `User32:USER32.dll`. Its map retains the allocator's
null import descriptor; the actual PE imports mimalloc and Kernel32, and no
USER32 DLL. The separately inspected native SDK 10.0.26100.0 x64 `User32.lib`
is 167,864 bytes, SHA-256
`f45759349ad08b848bdd4eee39cd9b428b2a7ae63a09889c09a197fe7b01421d`.
`dumpbin /ALL` shows its import-name section 3: twelve bytes `USER32.dll` plus
two NULs, static `.idata$6`, value/type zero. This is metadata independently
inspected from the SDK, not linker-selection proof.

The new explicit `--windows-user32-import-library` route admits only that
separately pinned owner with the exact static/zero-RVA/wrapped-offset,
initialized nonexecutable data shape, the real allocator import terminator
and mimalloc/Kernel32 observations. Actual USER32 imports or name payloads,
unknown/changed specimen bytes or owners, ordinary functions and malformed
addresses stay refused; the original bcrypt guard is unchanged. All names
remain in fixture-seam scanning. Existing security controls plus the new
User32 owner/hash/shape/import/absence controls pass (22 total). The public
CLI propagation control also covers the new explicit input. Actual native
CLI preparation is being retried with both reviewed SDK specimens.

## Distribution result — mandate r4

Run `mimalloc-20261024`, segment `distribution-m4`; sole engineering writer
`/root/mimalloc_distribution`, recipe `coordinator-implementer` revision 2.
Source/tracker is main at `/Users/jaeger/Documents/GitHub/BetterPullReview`.
The checked scheduling amendment `540b21fa9` permits independent work while
retaining every authored outcome, selector, exit and final Mac dependency.
BPR-1763 completes AC06/AC08's engineering and reporting scope. BPR-1762 and
this milestone remain incomplete on the actual Mac installed/signed/native
qualification; BPR-1759's Intel, Developer ID/notarization/Gatekeeper and
delivered Cocoa obligations are unchanged. RFC closeout is not authorized.

### Final package and source evidence

Current standalone CLI producer is
`df6e8be48ff17271e2258f0040e4ca87795f44bd`. `package_tour_cli.py` builds the
real CLI and supervisor, installs their private xdiff/allocator/Qt closure,
adds notices/SPDX and a separate versioned corresponding-source asset, then
runs the public package auditor. The CLI selects its own runtime-input map,
minimal Qt Core/Network source inventory, native Windows CRT and Ubuntu's
existing pinned hardened ICU producer. The private runtime prefix remains
replaceable and removable; no global allocator injection is used.

| Native CLI archive | SHA-256 | Audited files / binaries / first-party compile units |
|---|---|---|
| `BetterPullReview-0.3.2-macos-universal-tour-cli.tar.gz` | `12ad8a8f73f207704435bd675db38a00360ee720a0f1b1fa2af161e28104deb6` | 20 / 7 / 539 |
| `BetterPullReview-0.3.2-windows-x64-tour-cli.tar.gz` | `9e7aa769384cf9455510c606f28aff11a8f0090f6e318a87f50bcacc5bd5cf3f` | 18 / 13 / 531 |
| `BetterPullReview-0.3.2-linux-ubuntu-26.04-x64-tour-cli.tar.gz` | `add1fc5cf41a271c58bee9d743ce98652526350f3de63e4c05eed638a3dcf216` | 16 / 10 / 529 |

Each archive's separate asset is named
`Source-BetterPullReview-0.3.2-TARGET-tour-cli-corresponding-source.zip`, with
`TARGET` equal to that archive's native target. The exact third-party source
bytes have SHA-256 `590403ccc0a0e78d2f32f9d586bfe8b3a2e847645994bf5bf8905203d7ef19cc`
on Mac/Windows and `16a15ea179d12ce4c1e11228f0acc8942f5d3384b9b55e10aa58394643e1507b`
on Linux (which also carries ICU). Installed source references bind the
producer revision, distinct asset name/URL and digest. The unmodified source
ZIP can have the same bytes across producers; installed provenance still
names the exact producer and target. All include the pinned mimalloc 3.5.3
source and MIT bytes stated above. Byte/source inventory is engineering
evidence; legal distribution approval remains pending.

All three actual archive expansions pass the full CLI public auditor and
relocated `bpr --help`; all three installed CLIs pass the real-process Cycle
course. Private copies missing the allocator or carrying an extra runtime
are refused by the existing closed file/producer audit. The relocated copies
are removed successfully, and the source payloads remain byte-identical.
Missing private plugin directories and code-loading Qt arguments are refused.
`df6e8be48` reuses the existing installed-runtime guard for the CLI before Qt
construction, rather than relying on `qt.conf`: ambient selectors are cleared,
canonical private paths are selected, and Windows keeps its DLL search guard.
An external Linux debugger observer sees only `runtime/lib/bpr` and
`runtime/bin`, and `QT_PLUGIN_PATH` absent, after setting a foreign path.
The observer is not shipped. The package graph requires the CLI's installed
runtime selection. Linux installed C/C++/Qt/OOM observations in both CLI images
also pass with one allocator and Qt Core from this prefix. Mac has both slices,
but only arm64 execution and an offline ad-hoc seal; it supplies no Intel or
signed-installed acceptance.

The actual unsigned DEB and MSI prepared above were extracted privately and
fully audited against their retained exact producers, not relabelled current
source: Linux `0c40cca40`, Windows `c5693fd05`. Their installer/source hashes
remain as recorded above. No system install, registry action, upgrade/uninstall
qualification, release signature or native trust approval is claimed. Final
all-platform AC07 remains partial because the required Mac native/signed
candidate is unavailable. The complete final runtime maps, SPDX, source
references, PE/ELF/Mach-O observations and negative controls remain in the
candidate roots below.

### Composed, failure and instrumented paths — AC06

`b449356dd` fixes fresh-configure CMP0126 selection loss without changing the
pin, public allocator API or failure contract. Its fresh executable actually
catches throwing-new `std::bad_alloc` and challenges caller C++/architecture
settings. This passes on all three stations. Current Mac seven-row probes
pass in 22.13 seconds, Windows seven rows in 39.10 seconds, and current Linux
six rows at `df6e8be48` in 29.05 seconds. These exercise actual selected images,
C/C++/Qt/loaded-module ownership, cross-module/cross-thread release, 4,096
boundary iterations, exactly one allocator, ordinary allocation failures and
missing-override/redirect controls where applicable. Earlier native installed
and public integration evidence remains bound to its unchanged candidates.

The actual AppleClang ASan/UBSan plus libFuzzer graph excludes mimalloc and
retains the toolchain allocator. All seven selected corpus courses pass
(7.39 seconds); the instrumented public Cycle course passes (48.71 seconds).
The real images load ASan and no allocator runtime, and the build has no
mimalloc output. This is the focused instrumented path, not a full test matrix.
Later changes affect noninstrumented provider/package selection and the
installed-only CLI guard; no unchanged corpus course is relabelled as native
release acceptance.

Paired tests-on Release desktop `application_epoch_maintenance_public_path`
passes its four real-process seed/prune/restart/delete/restart phases in both
modes on all three stations. Baseline/mimalloc wall durations: Mac
8.91/9.16 seconds (`f85760a25`), Windows 14.26/18.29 and Linux 7.38/5.48 seconds
(`df6e8be48`). These are observations from one pair, not speedup estimates.
Windows' first invocation failed before launching the app because SSH supplied
an unresolved `WORKGROUP` account to the existing ACL helper. Supplying the
actual `System32/whoami.exe` token domain/user in this course's environment
makes both runs pass. A first attempt found Git's unqualified `whoami` instead;
that diagnostic is retained. No product ACL or course requirement was weakened.

### Matched Release report — AC08

The six existing selectors are `application_large_review_` plus `launch`,
`responsiveness`, `cache_bytes`, `cache_entries`, `loaded_close`, `resources`.
All workloads, fixtures and limits are unchanged. Each pair uses the same
native hardware, Qt 6.12.0, Release toolchain and feature/fixture inputs;
`BPR_USE_MIMALLOC` is the allocator difference. The composed measurement image
now explicitly attaches the selected allocator (`571f96d45`), while remaining
outside the shipped-program inventory. Other test tools retain their allocator.
Corrected mimalloc candidates are `b449356dd` on every station. Windows baseline
is the same revision; Mac baseline `dce9ff8ce` and Linux baseline `571f96d45`
reuse unchanged measured C++/fixture/toolchain inputs. Their common measured
sources did not change in the intervening provider/package-only commits.
The earlier fresh-graph mimalloc measurements are superseded as stated above.

| Platform / allocator | Process→main / Home ms | Paged review / large selection ms | Unchanged six-row result | Diagnostic peak / destroyed footprint growth MiB |
|---|---|---|---|---|
| Mac arm64 baseline | 20.10 / 459.10 | 1603.13 / 365.30 | 6/6 | 146.38 / 101.88 |
| Mac arm64 mimalloc | 19.16 / 409.90 | 1722.90 / 398.08 | 3/6; cache bytes, loaded close, resources fail | 158.59 / 157.56 |
| Ubuntu x64 baseline | 9.43 / 326.76 | 865.67 / 374.86 | 4/6; loaded close, resources fail | 177.79 / 162.43 |
| Ubuntu x64 mimalloc | 9.76 / 303.30 | 648.27 / 330.90 | 2/6; both cache rows, loaded close, resources fail | 284.34 / 283.41 |
| Windows x64 baseline | 17.87 / 592.14 | 2279.84 / 545.96 | 6/6 | 126.96 / 28.86 |
| Windows x64 mimalloc | 19.05 / 574.38 | 2260.59 / 569.24 | 2/6; both cache rows, loaded close, resources fail | 365.46 / 341.66 |

Mac is native arm64 macOS 26.6.2 on the M4 Pro/48 GiB station. Windows is
native Windows 11 25H2 x64 (build 26200); Ubuntu 26.04 is the selected
WSL2/WSLg x64 station (kernel 6.18.33.2-microsoft-standard-WSL2), 20 logical
Intel i9-10900KF processors and about 62.7 GiB guest memory. No Intel Mac
comparison is inferred. Mac and Linux shared-host load is recorded; the
course's attributed timings retain raw wall/wait measurements. These single
pairs are descriptive and do not establish a general latency improvement.

Every failing budget row above reports `sampled fixture footprint growth exceeded its platform budget`; none is an allocation-failure abort.

The supplemental existing `--allocation-profile` loaded-close diagnostic is
nongating: it lets shutdown/destruction finish after the real budget rows
retain their failures. Every tracked diff/index/window/document owner releases
in both modes on every station, the derived cache reaches zero, and shutdown
completes. Higher retained process footprint is observable despite that release;
it is not proof of a logical-owner leak. Mac's mimalloc diagnostic also records
a paged-load input-budget miss. Peak is the OS lifetime peak at recorded
samples, not a proof against all unobserved transient peaks. Windows uses
private commit for footprint; Linux uses resident memory; Mac uses physical
footprint. No cross-platform memory comparison is intended.

The upstream Mac mimalloc zone reports zero `size_in_use`; glibc and UCRT heap
queries also do not count the complete mimalloc heap. Their zeros cannot be
claimed as savings or budget acceptance. The unchanged budget rows keep their
actual outcomes. Linux baseline failures remain the existing platform/course
limit problem tracked with BPR-0967; new cache failures and extra retained
footprint in the mimalloc candidate are recorded separately. No memory limit,
allocator trim behavior or BPR-0967 implementation was changed.

Benchmark identities (baseline → corrected mimalloc, SHA-256):

- Mac: `a97b029afa3406cc96120251be1d33761ce4544c1eebf020b4761b6c3c83b296`
  → `10222161ee5a00168c8cc6ad026912fb7c0f563074c466b4e4b0834931e1ff4f`.
- Linux: `aa0c89f399404b59ea9964d9183f210e559ed95ce801d1c210b5b713f276f5d2`
  → `fda5e2d0e2ff73d49a8994f14303f0ac0f2aeb2a0296e9db18797e65f3c13c68`.
- Windows: `1e662c79f0c48cc5e5455e584b8bc8d4821092ec33af5638c813f7f9340f0e7d`
  → `1830903d1b4e71fd9f968307bda4b307d775546ee4f6063f0de81fcb8ba4bb7b`.

### Ownership, verification and continuation

Runtime evidence is `build/mimalloc-distribution/`: final package preparations,
collected native records, exact binary identities, raw six-row JSON/logs,
`measurements-final-summary.json`, instrumentation logs, protected-candidate
rehashes and final machine checkpoint/observation. Native complete evidence:
`C:/code/bpr-rfc0093-dist-6165b97d2cea` and
`/home/mmarcin/bpr-rfc0093-distribution-hrxjvs5j`; local CLI is in
`build/mimalloc-distribution/mac-cli-source` and `mac-cli-artifacts-final`.
Prior Mac/Windows/Linux roots are retained and every previously recorded
installed/CLI/candidate hash was rechecked unchanged. Source/build revisions
and earlier superseded/failed candidates are not overwritten.

Final focused controls pass: runtime tests 31 (one explicit native-Linux skip
on Mac), public package audit tests 13, PE/security tests 22, source asset tests
24, documentation tests 67, exact format 22 and focused native-arm tidy 22.
The universal compile database makes clang-tidy request two compiler jobs;
retaining its exact arm64 command for analysis resolves that tool limitation.
Both actual universal slices still compile. Required documentation/board/plan
checks are recorded at the final checkpoint; no full-suite pass is claimed.
Task-owned temporary negative copies are removed. Local ENOSPC was recovered
only by deleting this task's transport copy and regenerable fuzz objects/archive
outputs after retaining its binaries/logs; earlier roots and unrelated data were
never removed. Fuzz build objects must be regenerated before rebuilding it.

No review session/URL/cursor/server exists for this objective evidence milestone.
No command requires owner desktop access. Native Intel and the authorized signed
Mac candidate still need their real course; no available arm result, archive
or offline audit substitutes for them. Continue BPR-1762 only when that candidate
is available, recheck the affected exact source/runtime and signing boundaries,
then reconcile AC07/AC09 and BPR-1764. Main's unrelated `.mask.json`,
`.mask.lock.json`, `docs/issues/NEXT_ID` and BPR-1766 reservation remain untouched
and excluded. No push, publication, keychain/credential lookup or desktop capture
occurred in this milestone.

## Current Mac candidate — mandate r4

The coordinator resumed `pinned-macos-integration` under the owner's r4 steering:
continue agent-capable work without Intel, while retaining the actual Intel
criterion. This round changes no product source, acceptance criterion, allocator
pin, memory limit or existing AC08/BPR-0967 result. Sole leaf
`/root/mimalloc_m1`, run `mimalloc-20261024`, segment `macos-m1-current-r4`,
recipe `coordinator-implementer` revision 2, mandate `r4`.

A new clean task-owned worktree and branch preserve earlier candidates:
`/Users/jaeger/.codex/worktrees/mimalloc-macos-r4/BetterPullReview`, `codex/mimalloc-macos-r4`.
The exact clean producer is `75baca275aa8f052997a231a83ebc7d77d27ab6d` (current product/package source
`df6e8be48ff17271e2258f0040e4ca87795f44bd`). Source/tracker/runtime facts are bound in
`build/mimalloc-macos-r4/checkpoint-initial.json` and the final checkpoint.
Existing MASK locks were deployed offline to this worktree, without an upgrade.
Main's unrelated `.mask.json`, `.mask.lock.json`, NEXT_ID and BPR-1766 reservation
remain untouched. The earlier `eb28c0187` local ZIP rehashes unchanged to its
recorded `9ca6dec7…` digest; no earlier root or candidate was overwritten.

### Current bundle, source and public composition

`tools/release.sh --version 0.3.2 --prepare-only --skip-tests --output-dir
build/mimalloc-macos-r4/local-candidate` completed from clean source. The app,
all three selected helpers and allocator contain arm64 and x86_64 slices;
tests are disabled and distribution Release is selected. There is no phantom
CLI in this app graph. The existing prepare workflow uses only an offline
ad-hoc seal. No Developer ID operation, Keychain access, notarization,
Gatekeeper assessment, publication or owner-desktop operation occurred.

The relocated offscreen assessment clone passed private-profile and production
bundle-identity checks. The delivered bundle stayed unchanged during assessment,
fingerprint `2cad684c18797c4f9f3823183e3da8c24e4b37cb48e5f1b5ac67e0162a3e6129`.
The delivered native Cocoa course remains separate. Full public package audit
passes with scoped LLVM 22: `RELEASE_PACKAGE_OK`, 52 files, 19 binaries and
519 first-party compile units, trust `not-assessed`, legal approval `pending`.
The separate source sidecar passes `RELEASE_SOURCE_ASSET_OK`. Public package
negative controls reject an omitted allocator with
`required-packaged-capability-missing:libmimalloc.3.dylib` and an extra allocator
with `package-file-set-mismatch`; their private copies were removed.

Artifacts are under this worktree's `build/mimalloc-macos-r4/local-candidate/`:

| Artifact | SHA-256 |
|---|---|
| `BetterPullReview-0.3.2-macos-universal-local.zip` | `829bec5d595a2634f466df44f2e95b1f37741c4abab1ad342b0527d3afbac7df` |
| `BetterPullReview-0.3.2-macos-universal-local.spdx.json` | `0ced7b1d336401cd1a21d198026646a89b78d5ebc77cf2bd6f8f940d71104e7d` |
| `Source-BetterPullReview-0.3.2-macos-universal-corresponding-source.zip` | `fc15e56132603872e35f91fb57142c910558c08fa7e4f1e60261ae4d8b5f75e2` |
| App image | `fb772279ff5bcc6fa2cc00d9ee4e51c846ce0dfe19f32fdf070a6259210d04bb` |
| `Contents/Frameworks/libmimalloc.3.dylib` | `2d72e966ed52f2d5c9d3dee7d44425d39165b6e1c89edb7f96aadefade29da83` |

The source sidecar carries the pinned upstream bytes and complete retained MIT
notice. Producer/source references and SPDX bind this producer; reuse of the
same third-party source ZIP bytes does not relabel an earlier app candidate.

### Additional native arm64 observations

A small external, non-shipped observer was compiled under
`build/mimalloc-macos-r4/`. It links no allocator. Only its explicitly launched
BPR children receive `DYLD_INSERT_LIBRARIES`; it runs before main and exits
before ordinary GUI or provider startup. The actual payload is never modified.
Fallback arguments use the existing pre-Qt installed-runtime refusal, so refused
injection cannot open a desktop window. A standalone program without mimalloc
fails this same observer with `mi_is_in_heap_region` absent (exit 42), proving
that the observer does not supply or substitute an allocator.

Every exact staged app process passed the observer: desktop, supervisor, MCP
helper and update helper. It observes the pinned version, exactly one allocator
before and after module calls, C/C++ allocation ownership, aligned/array new,
C ENOMEM/nothrow/throwing-new failure semantics, Qt storage ownership, 4,096
cross-module iterations, cross-thread release and module shutdown. `dladdr`
origins must canonically equal the actual candidate's allocator and Qt Core.
The separate boundary fixture links Qt and never mimalloc. Qt Core is explicitly
loaded from the candidate for the otherwise Qt-free helper observations; no
claim is made that those helpers normally link Qt. All 52 bundle file hashes
remain unchanged. This is external pre-main process evidence, not a claim that
normal application main or delivered Cocoa was exercised by the observer.

The retained current standalone CLI producer `df6e8be48ff17271e2258f0040e4ca87795f44bd` was observed read-only
by the same arm64 method, with its own prefix-bound fixture. `bpr` and its
supervisor pass all ownership/boundary/failure observations, resolve allocator
and Qt Core from their own private CLI prefix, and all 21 expanded file hashes
remain unchanged. Their image SHA-256 values are
`ee17aef047aa33abf705df7b66d628383b423d9b8fb3b0c4666f9d6795822d8c` and
`8139db91c1eca4867a56789b51890e2ed5eadc3e76ec4bdac4d016d805582426`.
This supplements the existing CLI public course without relabeling its producer
as the new app producer or supplying Intel/signed-install evidence.

Without the observer, all four staged executables also reach their expected
safe invocation refusals: desktop installed-runtime guard, supervisor invalid
invocation, MCP unexpected argument and update-helper argument count. The exact
packaged MCP helper passes `audit_terminal_diagnostics.py`: healthy JSON-RPC,
closed/full pipes, bounded secret-free diagnostics and restored SIGPIPE behavior.
These direct courses are additional lifecycle evidence, not a whole updater or
supervisor acceptance matrix.

### Ownership and continuation

All task-owned commands are quiescent; no review server/session/URL/cursor exists.
Runtime evidence and reproducible external observer sources/commands remain in
`build/mimalloc-macos-r4/`: prepare/audit logs, artifact/binary identities,
`installed-observations.json`, `cli-installed-observations.json`,
`observer-negative.log`, `startup-refusals.json`, MCP terminal course,
negative package controls, prior-candidate rehash and final checkpoint/observation.
The external observers are outside every payload and are not product features.
This branch changes only the three RFC/card/verification documents. Preserve
the worktree/build/current candidate and all earlier roots for continuation.

BPR-1759, pinned-macos-integration, BPR-1762 and RFC closeout remain incomplete.
Native Intel, the actual authorized Developer-ID signed-installed course and
delivered native Cocoa observations remain pending. The r4 instruction permits
continued work without Intel; it does not waive that exit. No local ad-hoc seal,
arm64 observation, source audit or archive is substituted for those criteria.
The coordinator owns integration and the next authorized native/signed round.
