# RFC-BPR-0093 — process-wide mimalloc

## Post-review reconciliation — round r10

Fresh engineering leaf `/root/mimalloc_post_review`, run `mimalloc-20261024`,
segment `macos-post-review-r10`, mandate `r9-signed-macos-authorized`, preserves
coordinator-implementer revision 2 and immutable RFC base `e82537ad15b32871a8375b6143abe08e16a68b87`.
Main source/tracker is `/Users/jaeger/Documents/GitHub/BetterPullReview`.
No version change, push, tag, publication or owner-desktop operation is included.
Earlier candidates and unrelated MASK edits remain protected.

### Exact human intake and package reconciliation

The companion confirms session `3a0425eb3756874560105a4cff2b0b64` handed back
at sequence **6**, with no pending item. Human decision
`958b2988-5f73-4ebf-beea-cdffa5cbe603` is **Accept**, with no note, saved
`2026-10-25T06:13:46.695667+00:00`. It applies only to candidate
`BPR-1759-signed-cocoa-r9`, fingerprint
`a68fe8b3cd8c62f9287c0a8db30529a765947f8bf757641bf7fbab2fe93d5bd5`,
source `0f42035a1758a33c729317d66d3aaaac0e771fb1` and desktop SHA-256
`98c0feff5ad5e3b5ead9b8fe25fb02214ea628791dbda5a965c3f29d4b665d92`.
The delivered Cocoa local-file review, return/reopen and Quit judgment passes;
it grants no allocator-pointer, memory, security or release acceptance.
The consumed cursor 6 is durable before dispatch. The companion stopped this
session's Python host PID 60503 and private Serve PID 60519 after verified
intake; the candidate, verdict and external session state are retained.
No capability URL is stored in tracked documentation.

AC07 is reconciled independently of AC03 pointer proof: the r9 signed Mac
ZIP/DMG/private relocated installation agrees with its 53-file/19-image audit,
notices/SPDX, separate source ZIP and missing/extra allocator controls. The
[distribution result](#distribution-result--mandate-r4) binds actual privately
extracted MSI/DEB and all three standalone CLI archives to their own producers,
full runtime/source/notice inventories, relocation/removal and negative controls.
Those unchanged passing observations retain their original identities:
Mac app `0f42035a1`, Windows `c5693fd05`, Ubuntu `0c40cca40`, CLI `df6e8be48`.
This completes BPR-1762's package/source scope; ordinary-user install/upgrade/
uninstall, recipient access/retention, legal and release approval stay separate.

### Signed-process observation frontier

The original external pre-main observer still passes on the retained r4 ad-hoc
app with C/C++/Qt/module/thread/OOM and one allocator. Against every exact r9
hardened installed image it produces neither PASS nor FAIL: the images ignore
the observer injection and reach their normal safe refusals (desktop 1,
supervisor 125, MCP 1, updater 1). All production file hashes remain unchanged.
A direct LLDB launch of only the owned signed supervisor is denied by macOS
(`attach failed: Not allowed to attach to process`); no entitlement, signature
or platform policy is weakened. These are failed observation routes, not
allocator failures or passing pointer evidence. Source identity and allocator
statistics alone are not substituted for the missing signed-process course.

The coordinator authorized a separate tests-enabled signed diagnostic candidate
using the existing pre-main probe, with no production hook or new entitlement.
Exact source `0f42035a1758a33c729317d66d3aaaac0e771fb1` is retained in the
task-owned detached worktree `build/mimalloc-post-review-r10/diagnostic/src`;
its build is `diagnostic/build`. Native arm64 Release, tests enabled and all
five product executables are selected, with the same pinned allocator and
offline verified dependencies. The full selected native build passes.

The first configure correctly refused tests-enabled distribution mode. The
supported development graph keeps its authored `(tests)` display name and
`io.github.MikeMarcin.BetterPullReview.tests` identifier. Its native package-
notice attempt correctly refused the absent distribution-only input map;
no release guard is bypassed and no package qualification is claimed for this
candidate. Mechanical target/output-name mistakes are retained separately.
The same pinned third-party notice/source-offer text is retained. Deployment,
private rpath normalization and signing use the exact committed release helper
functions; only diagnostic path/identity selectors are supplied.

### Signed diagnostic native result and AC03 judgment

Candidate `BPR-1759-signed-diagnostic-r10` has non-symlink installed-file
fingerprint `646544c450afff7a8ce6b2a200ac69f78ba08697b8b9609b3e7d7820e8657c1e`
(SHA-256 of the sorted compact JSON path-to-SHA-256 inventory). All **22 Mach-O
images** have native arm64, Developer ID team `YTQKP2V2A8`, secure timestamp,
hardened runtime and empty entitlements. No library-validation, DYLD-environment
or debugger exception is added. Dependencies/rpaths resolve only inside the
private app or to system libraries; the boundary module links Qt, never mimalloc.
Apple submission `fc66d44d-a0ef-4382-abd5-3dbec30bf7f7` is **Accepted**. Staple,
strict signature and Gatekeeper checks pass before and after private relocated
installation, and again after the native course.

All five actual signed installed diagnostic product images pass the existing
`tests/mimalloc/process_probe.cpp` contract before ordinary main: C allocation
family, global/array/aligned C++ new, loaded Qt QByteArray storage, 4,096
cross-module iterations, cross-thread release, exactly one allocator, pinned
version 30503 and C/nothrow/throwing allocation-failure semantics. The signed
`mimalloc_unattached` control, built with the identical probe and no allocator,
correctly fails at `mi_is_in_heap_region` (exit 1). No loader injection is used.
The native probe exits before GUI/provider startup; an invalid Qt platform
fallback prevents accidental desktop use. No owner desktop is driven/captured.
All installed file hashes and the seal remain unchanged by the course.

| Signed diagnostic image | SHA-256 |
|---|---|
| Desktop `better_pull_review` | `03326b81a029dae63eb867ab25ef8e15c2df600c7cf2cfec86b1554325dff6a8` |
| `bpr` | `04ef02875b470979174104825a74a4b4d50219c5651a64b66dd11448cfeecb32` |
| Supervisor | `bcd28b25dc043e71efe930412f878179e3dae236c01337156ac06e9be48fc0ef` |
| MCP helper | `8f717932ad54062ebc16b28cd5782ad9d7644ef9e5fab343e5dfc8a879afa6f2` |
| Update helper | `085486e8fd18fbcc8a77a3c6707f6435f37d897c47435c3320ce7e82ec188cd2` |
| Allocator | `705c56755576a9963742cb951a698d5d76de7b149a6bdbc707c7a02ced0ff066` |
| Loaded boundary module | `b1e712c04e9d1d8d7c8addc251588a3d766048d5104114a7b4dde75c7198563d` |

This completes AC03 through two precisely distinct courses: the signed installed
diagnostic demonstrates actual pointer interception and safe release under
signing/hardening; the reviewed tests-disabled universal production candidate
demonstrates delivered Cocoa behavior, trust and package closure. Prior r4
native CLI/private-prefix observations retain their own producer, and r5
supplies the explicit universal/static architecture acceptance amendment.
The diagnostic CLI shares this app's private Qt/allocator closure for its signed
pointer course; it is not a newly qualified signed standalone CLI archive.
No direct pointer observation of the exact reviewed production bytes is claimed.
AC03 requires native courses including a signed installed candidate; both courses
retain their exact scope and identity. Pointer proof comes from the diagnostic
observation, rather than source identity or allocator statistics.
No release, native Intel execution, ordinary install/uninstall, legal/security
or memory qualification follows from the diagnostic result.

Raw commands, logs, identities, receipt, closed dependency observations,
`native-summary.json`, immutable installed candidate and source/build roots are
under `build/mimalloc-post-review-r10/diagnostic/`. Sanitized review intake,
production payload/artifact preservation, passing 13 package-audit controls,
67 documentation tests and a fresh exact tests-disabled production MCP public
terminal course are under `build/mimalloc-post-review-r10/`. Every prior
candidate remains retained with its original identity.


### Final RFC reconciliation — round r10

BPR-1759 is Done at `e12ad52ef4efea730409d99fd505ec0063b84712`; BPR-1762 is
Done at `e71b3349461baeeb6c44d1cbd21a45c87fac54a8`. The remaining closeout
claims use the actual criteria, not a changed memory/performance requirement.
The immutable RFC baseline remains `e82537ad15b32871a8375b6143abe08e16a68b87`.
Production/provider/package/version and probe inputs at this round's main
candidate match source `0f42035a1`; all earlier unchanged evidence remains
bound to its original producer, not relabeled as this administrative head.

| Criterion | Accepted direct scope and producing evidence |
|---|---|
| AC01 | Pin/licence/offline/substitution/collision/extraction controls at `81ee1b4ef`; provider semantics remain unchanged. |
| AC02 | Selected normal/distribution/minimal/core/baseline graph and actual baseline CLI observations at `81ee1b4ef`, with later corrected native fresh-configure controls at `b449356dd`; no phantom executable requirement. |
| AC03 | Above r10 signed installed five-product diagnostic pointer/Qt/module/free/failure proof and signed negative control; distinct r9 production Cocoa Accept/trust/universal closure at `0f42035a1`; retained `df6e8be48` CLI/private-prefix course and explicit r5 architecture amendment. |
| AC04 | Actual Windows 11 x64 product/CRT/Qt/DLL/private installed/CLI interception and public courses at `0ccf31bc4`/`c5693fd05`, with retained limits. |
| AC05 | Actual Ubuntu 26.04 WSL2/WSLg installed ELF, Qt/module/free/failure, CLI/MCP/XCB and clean external-child observations at `0c40cca40`. |
| AC06 | Native boundary/stress/failure courses; actual compatible ASan/UBSan/libFuzzer corpus and CLI; composed maintenance/public paths at `b449356dd`/`df6e8be48`; new signed diagnostic and exact production MCP course supplement unchanged observations. |
| AC07 | Exact signed Mac artifact/installed/source inventory plus retained actual MSI/DEB and all three CLI archives, full notice/SPDX/separate-source agreement, omission/extra and private removal controls; BPR-1762 accepted above. |
| AC08 | Original same-workload six-row Release comparison, native identities/conditions, current/peak memory and actual failures at `571f96d45`/`b449356dd`; later useful-test policy is an explicit owner amendment, not a rewritten comparison or memory improvement. |
| AC09 | Current architecture, build, package and operator scope/limits; passing documentation gate and 67 tests, 13 package-audit controls, exact production MCP public terminal course, retained hostile composition controls and accepted native Cocoa; scoped board/plan checks. |

This completes RFC-BPR-0093's engineering acceptance and authorized closeout.
The r5/r8 acceptance amendments remain explicit; no new criterion is waived.
BPR-1768 remains Backlog independently for unqualified process/allocated-heap
memory risk, and all original numerical/timing misses stay recorded. Broad
release suites were skipped for local Mac validation. Ordinary-user native
install/upgrade/uninstall, vault, crash/privacy, vulnerability, legal recipient
access/retention and release approval remain with their existing owners/workflows.
Private relocated copies and their removal do not supply those qualifications.
No version change, tag, push, publication or owner-desktop operation occurred.
The [r10 checkpoint](../issues/_checkpoints/RFC-BPR-0093-post-review-r10/handoff.md)
binds commits, exact candidates, quiescent commands and retained roots. Runtime
`build/mimalloc-post-review-r10/` retains actual checks and the plan observations.


## Signed local macOS acceptance — authorized round r9

The owner explicitly authorized Developer-ID signing and notarization for local
RFC acceptance after the useful-test handoff. This round uses the unchanged
0.3.2 CMake version solely for local artifact names. No version change, tag,
push, GitHub release or publication is authorized or performed. Intel execution
remains waived under r5, and the separate memory-risk follow-up remains open.
Sole leaf `/root/mimalloc_macos_signed` owns this round on
`codex/mimalloc-macos-signed-r9`, source/build root
`/Users/jaeger/.codex/worktrees/mimalloc-macos-signed-r9/BetterPullReview`.

### Source and preflight

Frozen producer `0f42035a1758a33c729317d66d3aaaac0e771fb1` descends from main
`d2b7f6a3267f781126ab7851c71475e27376ddae`. Every production, provider,
package, native-tool and version path (`src`, `cmake`, `packaging`, `tools`,
`CMakeLists.txt`, `CMakePresets.json`) is byte-identical to `df6e8be48`.
Prior native pointer/cross-module/failure, sanitizer and CLI evidence therefore
retains its original scope and producer identity; it is not relabeled as a
new signed-process pointer course.

The early composition inventory check failed its stale production-manifest pin.
The only changed manifest files since sealed `6c998babb` were
`df6e8be48`'s `installed_runtime.cpp/.hpp`: CLI-only private plugin paths and
a comment. Review confirmed canonical-directory equality, cleared ambient
selectors and forbidden code-loading arguments; desktop, full-application and
mutation-host pins stay exact. Commit `0f42035a1` records the review and reseal;
the positive inventory and every hostile bypass control then pass. Initial
failure is retained. Locked MASK tooling was registered offline without upgrade;
documentation, all 67 documentation tests and both allocation tests pass.
The older deployed checkpoint helper refused the newer recipe fingerprint;
the assigned current helper successfully binds the preserved revision-2 recipe.

### Actual signing, notarization and installed closure

From clean committed source, with scoped LLVM 22 for symbol inspection:

```sh
tools/release.sh --version 0.3.2 --skip-tests --output-dir /Users/jaeger/.codex/worktrees/mimalloc-macos-signed-r9/BetterPullReview/build/mimalloc-macos-signed-r9/signed-artifacts
```

Actual exit 0; UTC interval `2026-10-25T05:50:15.280968Z` to
`2026-10-25T06:04:27.475186Z`, 852.193 seconds. The native workflow builds
universal Release with tests disabled and distribution/mimalloc enabled,
deploys the restricted Qt closure, signs and timestamps every image with
`Developer ID Application: Michael Marcin (YTQKP2V2A8)`, hardened runtime and
no entitlement keys. All 19 delivered Mach-O images have arm64/x86_64 slices;
all four product executables depend on the private allocator, whose two slices
retain `__interpose`. Native execution is arm64 only.

Apple accepted app submission `58d4a0fd-dfea-46b6-ad43-2a4150139c19` and
DMG submission `6650be98-2cff-4c98-b699-594be6a23fd2`. App/DMG `stapler
validate`, strict signatures and their respective execute/open Gatekeeper
assessments pass (`Notarized Developer ID`). The DMG was mounted read-only
without browsing and detached after private installation. ZIP, mounted DMG
and relocated installed app have the same complete file hashes. The installed
copy passes signature, staple and execute assessments again.

The full public package audit passes: **53 files, 19 binaries, 519 first-party
compile units**. The new file relative to r4 is the stapled
`Contents/CodeResources`. Runtime closure, retained MIT notice, SPDX and separate
versioned corresponding-source asset agree; source preflight and all four
checksum sidecars pass. Public negative controls reject the missing allocator
(`required-packaged-capability-missing:libmimalloc.3.dylib`) and the extra one
(`package-file-set-mismatch`). Disposable negative copies are removed. The audit's
own `trust: not-assessed` and `license_approval: pending` fields are preserved;
separate native tools above supply this local trust evidence, not legal approval.

| Final local artifact/image | SHA-256 |
|---|---|
| `BetterPullReview-0.3.2-macos-universal.dmg` | `4cdf060665ec4c265a374bb51892aee2c2e0e81008fd5f07dc193984d2444cd7` |
| `BetterPullReview-0.3.2-macos-universal.zip` | `69d4b6b8f75054ec9a76ba0afc20beee9b5133a075715be131ab7a8f1b933ac7` |
| `BetterPullReview-0.3.2-macos-universal.spdx.json` | `2a5703a96f2d3032337c21c9cc90cdf96e3f27fd57c25273fda52e315c20da3b` |
| `Source-BetterPullReview-0.3.2-macos-universal-corresponding-source.zip` | `fc15e56132603872e35f91fb57142c910558c08fa7e4f1e60261ae4d8b5f75e2` |
| Desktop image | `98c0feff5ad5e3b5ead9b8fe25fb02214ea628791dbda5a965c3f29d4b665d92` |
| Private allocator image | `7581a1b93a92af947f1b0470ad570bbb4383a4da1d3b035fa60ed507f90b4650` |

All eight native outputs remain in `build/mimalloc-macos-signed-r9/signed-artifacts/`.
The source ZIP's unchanged bytes reflect unchanged pinned third-party material;
current producer references remain bound to this exact signed artifact set.
The standalone CLI retains its separately audited r4 producer, not a fictitious
CLI in this app graph.

### Public behavior and human boundary

The native workflow's relocated signed offscreen clone passes its private-profile
and production bundle-identity checks without changing delivery. A second
assessment clone, using the same approved helper functions, produces a readable
Home render and exits 0 with `PUBLIC_PATH_OK`, `RENDER_OK` and composed-host
markers. Only that clone receives the assessment-only offscreen plugin and is
resealed; the original delivered hashes remain unchanged. Both assessment clones
are removed after their checks. This is headless evidence, not native Cocoa.

The exact installed MCP helper passes the real JSON-RPC, closed/full-pipe and
bounded diagnostic course. All four signed product images reach their expected
safe refusals (desktop 1, supervisor 125, MCP 1, updater 1) with process-local
mimalloc initialization diagnostics. These observations do not substitute
statistics for pointer proof. The prior unchanged-implementation pointer course
and this exact signed trust/lifecycle course retain their distinct identities.
No DYLD injection, debugger entitlement or relaxed hardening is added.

The native Cocoa launcher is ready and checks the frozen delivered hashes and
signature before starting. A QtCore path probe found that `HOME` alone does not
isolate macOS `AppLocalDataLocation`; no application profile was opened. The
corrected launcher sets both `HOME` and `CFFIXED_USER_HOME`, and the exact packaged
QtCore probe confirms both home/data paths lie under its new task-owned root.
The launcher records early exits, final exit and payload preservation. It needs
no account and provides a synthetic local folder for the owner to review/reopen
and Quit. No agent drives or captures the owner's desktop. The
`frontend-design` skill and desktop reference were loaded for this presentation
review; the offscreen image was inspected directly.

Fresh attended session `3a0425eb3756874560105a4cff2b0b64` is ready at external
state `/Users/jaeger/.codex/attended-review/bpr-mimalloc-signed-r9-k9f02pev`.
Candidate `BPR-1759-signed-cocoa-r9`, fingerprint
`a68fe8b3cd8c62f9287c0a8db30529a765947f8bf757641bf7fbab2fe93d5bd5`, asks one
Accept/Reject judgment about delivered native Cocoa behavior. Initial consumed
cursor is **3**. Loopback authenticated status returns HTTP 200 with that ready
candidate; the default private Serve route is active too. Capability URLs remain
only in external session state. The coordinator owns intake and the next
engineering round; this leaf interprets no human verdict.

BPR-1759/BPR-1762 remain human-pending, BPR-1764 Backlog and RFC In Progress.
The broad Release/Debug suites were skipped for this explicitly local validation;
this is no release approval. Existing memory failures/risk, native vault,
installation/upgrade/uninstall, crash/privacy, vulnerability and distribution
legal/recipient-access obligations retain their recorded scope. A private
relocated copy is not ordinary-user installation or uninstall qualification.
Raw receipts, identities, commands, render and launcher checks are in
`build/mimalloc-macos-signed-r9/`. The durable
[signed-round handoff](../issues/_checkpoints/RFC-BPR-0093-signed-macos-r9/handoff.md)
accounts for writers, commands, review and continuation. Earlier candidate roots
and main's unrelated MASK edits are preserved.

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

## Retention follow-up — benchmark-only collection

The coordinator resumed the distribution implementer after the r4 report. The
owner then explicitly rejected making purge delay zero a product default merely
to pass the course, and asked for manual purging in tests. That instruction is
binding. The isolated product experiment `645a8125b` was reverted in `676ac3b42`
and never integrated into main. Its interrupted native pipeline is unaccepted.
Only test changes `551f3906e`/`1322c71b5` were integrated as
`fdb7eba8b`/`d217078a2`. Production sources, provider/default settings, packaging,
source pin and version are byte-for-byte unchanged from owner-r5 base
`0f799140c` (`git diff --quiet` over those paths passes). No production purge
hook or trim policy was introduced. The r5 Intel amendment remains in force;
actual signed-installed and delivered Cocoa Mac acceptance remain pending.

### Diagnostic behavior and native evidence

The composed benchmark accepts **`--allocation-profile-purge`** only when its
selected allocator is mimalloc. It runs the existing public loaded-close and
owner-release checks, retains the normal destruction samples and original heap
observation, observes approximately one/five seconds of Qt event-loop idle,
then calls `mi_collect(true)` once. The report records the forced collection's
wall duration and a separate `after_forced_allocator_collect` sample. No
allocator option is set by this code. Normal course flags call no new purge.

This is explicit diagnosis: `Extended.AllocatorCollectDiagnostic=true`,
`Extended.AllocationProfile=true`, `Extended.BudgetsEnforced=false`, and stdout
prints `LARGE_REVIEW_ALLOCATOR_PURGE_DIAGNOSTIC_ONLY`, never a loaded-close
acceptance marker. Earlier samples and lifetime peaks are not reset or rebased.
A successful diagnostic exit means measurement completed; it cannot make an
ordinary memory-budget failure pass. The toolchain-allocator baseline actually
loads no mimalloc and refuses this mode with exit 77 and
`LARGE_REVIEW_ALLOCATOR_PURGE_UNAVAILABLE`, without writing a report.

Final native reports use test producer `1322c71b5862d781247e9866375c42383da62b73`,
Release, Qt 6.12.0, the unchanged large-review fixture, Mac arm64 and the approved
Ubuntu 26.04 x64 WSL2 station. Every tracked diff/index/window/document owner
releases before the purge. Native counters and the incomplete allocator heap
counter limitations remain those recorded above. Figures below are process
footprint growth in MiB; collection time is one observed wall duration, not a
universal latency promise.

| Platform / per-process experiment | After destruction | ~1 s idle | ~5 s idle | After forced collection | Collection ms |
|---|---:|---:|---:|---:|---:|
| Mac, original 1000 ms delay | 144.36 | 144.30 | 144.30 | 43.75 | 3.80 |
| Mac, diagnostic 100 ms delay | 143.05 | 143.03 | 143.02 | 43.75 | 4.18 |
| Mac, diagnostic 10 ms delay | 145.63 | 145.61 | 145.59 | 44.08 | 4.33 |
| Ubuntu, original 1000 ms delay | 283.46 | 283.45 | 283.45 | 97.70 | 5.09 |
| Ubuntu, diagnostic 100 ms delay | 257.97 | 257.96 | 257.96 | 123.67 | 3.95 |
| Ubuntu, diagnostic 10 ms delay | 252.68 | 252.67 | 250.60 | 110.74 | 4.53 |

The 100/10 ms settings were supplied only to those benchmark invocations.
They are not product defaults or recommendations. The main/default row observes
`AllocatorPurgeDelayMs=1000` and `AllocatorArenaEagerCommit=2` on both hosts.
Earlier diagnostic rounds remain in the runtime root; their Mac destruction
values varied around 144–160 MiB and do not establish a fixed bound.

The unmodified six-row default course is retained separately at producer
`551f3906e`: Mac **4/6** (cache bytes/resources fail), Ubuntu **2/6** (both cache
rows, loaded close/resources fail). Those failures still report sampled
footprint budget excess. The later change only moves diagnostic collection
outside the original heap sampling, renames the shared skip constant, and fixes
the separate probe call below; its ordinary workload/limits are unchanged.
The r4 baseline and original Mac 3/6 result remain recorded, including normal
run-to-run boundary variability. Neither manual collection nor the discarded
zero-delay experiment is a BPR-0967 fix or an ordinary-workflow pass.

Exact final benchmark/runtime SHA-256:

- Mac: `a975c67a0ecdb21d4346ae8aa0ccba34f6cd11ba125b1c36edc59df676e1a69e` /
  `90c1974c304ab7b5ba283f603b9c3f9f38a64edd03e9c847b7a2fee54e6a4627`.
- Ubuntu: `2ab10fbb1184d9d0f9b8f181dd6326dda37aea0812cb3c06c69ca619721b43c6` /
  `72c4fd05b110d40fb4e49402d2bd39414cf2957017c6e0c144bff606946464ca`.

### Mechanism and decision

The exact pinned v3.5.3 `src/options.c` uses a 1000 ms purge delay and arena
multiplier four; older prose defaults in its documentation are not this build's
observed settings. `src/arena.c` schedules eligible arena purges and performs
them on allocator collection activity. Elapsed time alone is not a background
purger. A separate debugger observation on the original Linux image found RSS
unchanged at 320,440 KiB after six seconds idle, then 156,024 KiB after nonforced
collection and 145,976 KiB after forced collection. The new event-loop timeline
likewise shows substantial retention at five seconds and release on demand.

Linux's overcommit/eager arena commit and transparent huge pages also affect
residency. Bounded upstream-option experiments reduced it with lazy arena
commit or disabled THP. THP disable calls `PR_SET_THP_DISABLE` for the process;
a standalone diagnostic process confirmed that disable state survives child
exec (0 before, 1 after setting and in its child). That broader process/child
policy consequence is unsuitable as an unexamined allocator-only correction. None of these settings was adopted. Zero-delay
experiments returned pages sooner and generated roughly four/five times as
many purge operations in the diagnostic profile, but they do not demonstrate
an actual interactive-workflow benefit or qualify their reuse/page-fault cost.

Manual collection demonstrates a large releasable fraction after the tracked
owners are gone. It does not prove that every remaining allocation is live or
leaked, and cannot erase an earlier in-use peak. The benchmark deliberately
keeps the process alive after fixture-application destruction; full process
exit returns its address space to the OS. This diagnostic is not evidence that
production should purge on every close/prune, nor a substitute for a live
review/reopen/continued-work course. The decision is to keep product policy and
ordinary budget rows unchanged, and provide this separate attribution tool.

### Probe correction and verification boundary

The resumed Mac Release course exposed `malloc failure semantics` failures.
The retained pre-prototype Release supervisor failed identically at original,
zero, 10 and 100 ms delay; this was not introduced by the discarded prototype.
A small native control showed AppleClang `-O0` returning null/ENOMEM for both
calls, while `-O3` folded the direct deliberately impossible malloc observation
(null=false/errno=0) but the opaque actual call returned null/ENOMEM. The probe
now makes that failure call through a volatile function pointer, retaining the
same null/ENOMEM requirement and all other ownership/failure checks. No product
allocation code or failure contract changes. All seven native Mac Release
rows pass (18.72 seconds); all six native Ubuntu Release rows pass (5.81 seconds).
Focused exact format/tidy 22 and the native MSVC syntax control pass. The
first standalone MSVC command duplicated the source's own `NOMINMAX` define;
removing that harness-only duplication makes the unchanged source pass /W4 /WX.
These results are recorded in the runtime checkpoint. No Windows manual-purge measurement is claimed.

Runtime: `build/mimalloc-retention-r6/` contains both actual candidate identities,
all ordinary and diagnostic JSON/logs, option controls, compiler counterexample,
`final-diagnostic-summary.json`, validation and final checkpoint. Complete native
Linux evidence is `/home/mmarcin/bpr-rfc0093-retention-r6`. Source/builds are
retained in `/Users/jaeger/.codex/worktrees/mimalloc-retention-r6/BetterPullReview`
and that Linux root's `source` worktree. A harness initially placed three Mac
final diagnostic folders at main's top level; only those own outputs were moved
into the ignored runtime root and its launcher was corrected. No personal or
unrelated data was moved. All earlier candidate roots and raw evidence remain.
BPR-1763 stays terminal reporting acceptance; BPR-1762, signed-installed/Cocoa
Mac and final AC07/AC09/RFC closeout remain pending. No signing, publication,
version change, push, owner-desktop access or production hook was performed.


## Cleanup-policy reassessment — completed boundaries

The owner asked for an empirical reassessment of test-only collection at
completed cleanup boundaries, with the same limits and every active/peak
failure preserved. The owner suggested that default allocators might behave
like zero-delay purging; that is a hypothesis to measure, not an assumed
property. This follow-up authorizes the separately reported comparison below.
It does not amend AC08's unchanged-course requirement or make collection a
production policy. The rejected zero-delay product prototype remains reverted.

### Exact predicates and the original failures

The six rows are launch, responsiveness, cache bytes, cache entries, loaded
close and resources. Launch/responsiveness gate their own public-path and
latency observations, without resource sampling. The four resource rows gate
sampled process-footprint growth and process-lifetime peak growth at every
sample: Mac 160 MiB / Ubuntu 168 MiB, except cache bytes at 184 / 240 MiB.
Completed cleanup additionally requires live-heap growth at most 84 MiB,
then 32 MiB after application destruction, and zero derived-cache bytes,
entries and outstanding diff/epoch jobs. A separate sampled live-heap peak
limit spans the whole course (160 / 168 MiB; cache bytes 184 / 216 MiB).
Ownership, exact source, preserved Git/catalog data and public shutdown remain
independent assertions. Collection cannot reset any active sample or peak.

An audit found that an ordinary failure's sample was printed to stderr before
the limit check but appended to partial JSON only afterward. Reconstructing
that missing sample from the retained log gives this exact original inventory.
Values are current / lifetime-peak growth in MiB at the first failed sample;
all original raw reports and logs are retained.

| Original pair / row | First failure phase | Current / lifetime peak | Limit |
|---|---|---:|---:|
| Mac mimalloc cache bytes | `cache_byte_selection_16` (active) | 187.11 / 187.14 | 184 |
| Mac mimalloc resources | `after_prune` (cleanup) | 164.84 / 164.86 | 160 |
| Ubuntu baseline loaded close | `large_file_ready` (active) | 171.89 / 171.89 | 168 |
| Ubuntu baseline resources | `large_file_ready` (active) | 171.64 / 171.64 | 168 |
| Ubuntu mimalloc cache bytes | `cache_byte_selection_1` (active) | 293.95 / 293.95 | 240 |
| Ubuntu mimalloc cache entries | `review_loaded` (active) | 193.12 / 193.12 | 168 |
| Ubuntu mimalloc loaded close | `review_loaded` (active) | 190.45 / 190.45 | 168 |
| Ubuntu mimalloc resources | `review_loaded` (active) | 188.78 / 188.78 | 168 |

The original Mac baseline passes 6/6, Mac mimalloc passes 4/6 and Ubuntu
baseline/mimalloc pass 4/6 and 2/6 respectively. Mac mimalloc cache entries and
loaded close already complete cleanup. The first Mac resources miss is indeed
at cleanup, so checking current footprint after explicit collection is a
reasonable alternative policy to test. Its process-lifetime peak also exceeds
the existing ceiling and cannot be erased by collection. Other listed misses
occur before cleanup. Ordinary first-failure behavior means unvisited later
predicates were not measured, not passed. Exact raw references and reconstructed
samples are `build/mimalloc-cleanup-r7/original-predicate-inventory.json`.

### Candidate comparison policy

Test producer `806df31d2e517b27702a416c8046f0a0b8625ddf` changes only the
large-review benchmark, native observer and observer header. Ordinary flags
retain their assertions and now preserve a failing sample in partial JSON.
The opt-in `--allocator-cleanup-collect` suffix accepts only the four resource
courses and is not added to CTest registrations. It collects only after the
existing prune/delete/shutdown/destruction boundary has completed and derived
cache/job counters are zero. Raw before/after samples, their indices, API
result and collection wall duration are retained. The canonical cleanup
current-footprint/live-heap observation is the post-collection sample; every
ordinary active footprint, lifetime peak and whole-course heap peak remains
subject to the original limit. Numeric misses are latched until the completed
course exits 1, permitting later cleanup observations; structural failures
still stop the course. Budgets remain enforced, unlike the earlier allocation
profile diagnostic. No native peak is reset or baseline moved.

Each allocator gets a supported best-effort request: `mi_collect(true)`,
`malloc_zone_pressure_relief(nullptr, 0)` on the Mac baseline, or
`malloc_trim(0)` on glibc. These APIs differ in reach and return type. Pinned
mimalloc `src/theap.c` collects the calling thread's pages and visits arenas
with forced purge; it does not promise to collect every live thread's pages.
The Mac SDK says NULL examines all zones and a zero goal requests maximal
relief. The comparison reports these actual effects rather than treating the
baseline as implicitly zero-delay. A Windows `_heapmin()` implementation
passes native MSVC /W4 /WX syntax checks, but no Windows runtime collection
measurement is claimed here.

The inherited native live-heap counters (malloc zones / glibc mallinfo2) do not
cover mimalloc. Predicate records explicitly mark that lack of coverage;
a zero reading does not establish zero allocated heap. Process footprint and
native lifetime peak still cover the process. This follow-up neither silently
substitutes heap statistics nor certifies the unmeasured heap bounds.

### Native matched results and decision

The full pair at producer `806df31d2` uses the same native M4 Pro/48 GiB
macOS arm64 and Ubuntu 26.04 WSL2 x64 stations, Qt 6.12.0, Release toolchains,
feature/fixture inputs and thresholds recorded above. Each allocator first
runs the original six registered rows, then the four collected variants.
Mimalloc option/injection environment variables are absent. Dependency output
confirms mimalloc present only in the selected allocator graph. Exact benchmark
and runtime bytes are retained before relinking the baseline; both trees are
restored to mimalloc afterward. Runs are serial on each station.

`O/C` below means ordinary / collected, in separate processes. Launch and
responsiveness have no collected variant and retain their ordinary verdict.
These are measured numeric/structural outcomes; mimalloc's uncovered native
heap counters still cannot establish its true allocated-heap bounds.

| Native pair | Launch | Response | Cache bytes O/C | Cache entries O/C | Loaded close O/C | Resources O/C |
|---|---|---|---|---|---|---|
| Mac baseline | pass | pass | pass/pass | pass/pass | pass/pass | pass/pass |
| Mac mimalloc | pass | pass | fail/fail | pass/pass | fail/fail | fail/pass |
| Ubuntu baseline | pass | pass | pass/pass | pass/pass | fail/fail | fail/fail |
| Ubuntu mimalloc | pass | pass | fail/fail | fail/fail | fail/fail | fail/fail |

The new ordinary Mac mimalloc misses are cache bytes at
`cache_byte_selection_17` (184.19 current / 184.22 peak against 184 MiB),
loaded close at `before_loaded_close` (161.98 / 161.98 against 160), and
resources at `after_prune` (164.47 / 164.48 against 160). The collected Mac
cache-byte/loaded-close rows still fail active values of 189.86 / 163.61 MiB
and their retained peaks. The collected resources process has an already-lower
first pre-collection prune sample, 148.73 MiB with peak 157.41. Its pass does
not prove that collection caused the ordinary-to-collected verdict change;
process-to-process variability preceded the first collection. No retry-until-
pass or retrospective relabeling is used.

Every Ubuntu mimalloc collected row still fails active current/peak limits:
cache bytes at selection 1 (283.35 against 240), and cache entries / loaded
close / resources at review load (192.12 / 188.31 / 188.27 against 168).
Current growth also remains excessive after collection at cache-entry shutdown
(174.78), loaded-close shutdown (185.20), and resources prune/delete/shutdown
(235.29 / 237.89 / 196.84 MiB). The baseline's two resource rows still fail
active/peak limits, separately from mimalloc's additional failures.

Representative resources observations in MiB, preserving every raw sample:

| Native allocator / boundary | Before collection | After collection | Collection ms |
|---|---:|---:|---:|
| Mac baseline, prune | 105.22 | 105.22 | <0.001 |
| Mac mimalloc, prune | 148.73 | 117.08 | 1.930 |
| Ubuntu baseline, prune | 173.14 | 117.23 | 3.156 |
| Ubuntu mimalloc, prune | 272.16 | 235.29 | 1.464 |
| Mac baseline, destroyed | 110.38 | 110.38 | 0.001 |
| Mac mimalloc, destroyed | 91.08 | 44.08 | 2.988 |
| Ubuntu baseline, destroyed | 100.14 | 63.54 | 3.631 |
| Ubuntu mimalloc, destroyed | 196.66 | 129.91 | 2.206 |

All ten Mac baseline requests report zero bytes released and no measured
footprint change. All ten glibc baseline requests return 1 and reduce current
RSS. These observations do not establish any default allocator's purge-delay
setting. In particular, the Linux baseline also has substantial releasable
memory at the completed boundary. Mimalloc collection releases a large fraction
but does not make every cleanup current-memory predicate pass on Linux, and
cannot erase the active or lifetime-peak failures on either host.

Forty completed collection boundaries across sixteen variants retain both
samples, zero cache/job counts, successful shutdown and destroyed diff/index
owners. Loaded-close/cache variants also release the window/documents. Every
numeric miss agrees with the exit verdict. The native heap-counter coverage
flag remains false for mimalloc rather than treating zero as heap acceptance.

### Peak-preserving correction and exact verification

Linux raw `VmHWM` growth decreases across two observed requests: loaded-close
shutdown 271.5078 to 271.3438 MiB and resources prune 272.1641 to 271.1406.
No counter-reset API was called. The final producer
`84a1772145bfda3c127fe1772a9f1bb21a7fea2b` conservatively uses the larger
pre/post native peak for that boundary's peak predicate and retains both raw
readings. Earlier active failures remain latched. Re-evaluating all forty raw
pairs with this stronger guard changes no full-matrix row verdict.

Focused exact-final-source courses retain a useful Mac counterexample:
ordinary resources happens to pass (largest observed peak 159.88 MiB), while
the separately launched collected resources process reaches 162.33 current /
162.34 peak before prune collection. Collection reduces current growth to
119.69 MiB and later destruction to 44.11, but the original 160 MiB lifetime-
peak gate correctly fails. Final collected loaded close also fails its active
161.97 MiB and peak. Final Ubuntu ordinary resources and both collected
resources/loaded-close fail their numeric limits while the collected courses
complete every ownership/cleanup assertion. No failing row is relabeled.

The full-matrix benchmark SHA-256 values are:

- Mac mimalloc `ce0fd423d8caa9ded598de8766a6318ce60c3325d72bf2b37f8449da467ffed1`;
  baseline `ccc1eb5977244dd8887f480520a9ea0f5e9128a7698e02f2bddc5f94b938a0cc`.
- Ubuntu mimalloc `ddc63e1414ad06ecf93f42061523aba24384338fc7d34b57e98be364fb1ed1e9`;
  baseline `e0a94e1bdf6c26b080f7251c6df0bca04a40bf8210175f23dcedc13ed01d6326`.

Final focused benchmark SHA-256: Mac
`04dded40c19aa057926746417b3262a805e91c68b2e7b8cbb5202d20db044b20`,
Ubuntu `9672fe27db71c9357df80acb0c87a3db88db9173cade34b069f6091b99cd02bb`.
Both cohorts use the unchanged runtime hashes from the preceding follow-up:
Mac `90c1974c304ab7b5ba283f603b9c3f9f38a64edd03e9c847b7a2fee54e6a4627`,
Ubuntu `72c4fd05b110d40fb4e49402d2bd39414cf2957017c6e0c144bff606946464ca`.

The bounded test change is integrated as `c6be37f34` / `42f0ea86b`. No
production source, option, pin, package, version or CTest registration changed.
The opt-in comparison is a useful allocator-release policy with real numeric
verdicts; it is not sufficient to repair these legacy whole-row failures.
AC08's original comparison/reporting acceptance and all historical failures
remain intact. Any normative gate redesign needs its own explicit owner
amendment and risk justification. The owner's subsequent instruction to audit
whether the tests are useful and make mimalloc pass authorizes that next
investigation; it does not retroactively turn this cohort into passes.

Runtime `build/mimalloc-cleanup-r7/` holds the full inventory, identities,
ordinary/collected JSON and logs, `final-comparison-summary.json`, final
`peak-guard/` native courses, control logs and validation. Native Linux records
also remain in `/home/mmarcin/bpr-rfc0093-cleanup-r7`; the source/build tree is
its `source` worktree. Mac source/build is
`/Users/jaeger/.codex/worktrees/mimalloc-cleanup-r7/BetterPullReview`.
Earlier candidates and evidence remain protected. Signed-installed/delivered
Cocoa Mac, BPR-1762 and final RFC closeout remain pending under owner r5.
No release, signing, credentials, push or owner-desktop operation occurred.

## Useful resource tests — owner verification amendment

The owner requires mimalloc and useful passing tests, and asked whether the
memory rows protect real user risks or merely encode a particular allocator.
After the audit and one bounded live reopen investigation, the coordinator
explicitly authorized retiring only the baseline-specific process RSS/footprint
and native lifetime-peak pass/fail predicates to telemetry if a fair replacement
did not validate. No new green trend metric is invented. This amendment changes
the four default resource CI commands, not their historical outcomes or the
plain legacy qualification path. AC08 remains a truthful comparison/reporting
criterion, not a promised speedup, RAM bound or memory saving.

### Purpose audit and disposition

The resource courses are useful. Their exact cache, ownership, source and
lifecycle checks previously exposed real defects. Their process envelopes were
calibrated as a native baseline observation plus 10%, rounded to 8 MiB, with
one run and confirmation (`BPR-0967-station-budgets-2026-10-01.md`). That
calibration is not an allocator-neutral live-memory contract. BPR-0893/1032
separated a real approximately 70 MiB Qt per-line retention from freed pages
kept dirty by malloc. BPR-1371 then observed a 30–44 MiB footprint spread at
identical live heap, including misses in the original repaired code. BPR-1653
separately amended Linux byte-pressure RSS from 216 to 240 MiB while retaining
its 216 MiB allocated-heap ceiling. All that evidence and every original limit
remain unchanged.

| Course / original path | User risk and retained CI gates | Disposition |
|---|---|---|
| Launch; composed path `e9da7abca`, later BPR-1495 launcher | Process/main/Home timing, actual readiness/markers and launch limits | Retain unchanged; no resource envelope was part of this row |
| Responsiveness; `e9da7abca`, native batching `49aefc1cf`, BPR-0967/1598 | Exact source/render/anchors; real input/navigation/drawer/theme and completion/input limits | Retain unchanged |
| Resources; prune fix/course `316390ce1` | Public prune/delete remove the exact refs/catalog state, clear projection and derived cache/jobs, release diff/index, preserve required diagnostics | Keep contracts; physical envelope becomes explicit telemetry |
| Loaded close; `5590d3ff9` | Close a genuinely loaded large comparison; public shutdown, window/doc/diff/index release, retained Git/catalog state | Keep contracts; physical envelope becomes explicit telemetry |
| Cache entries; `470adea7d` | 65 selections exercise the default 64-entry ceiling, exact source/blobs, oldest eviction and normal-job revisit | Keep and strengthen: track every observed pressure diff/index through destruction |
| Cache bytes; `1681994af` | Default 64 MiB accounting/eviction before entry pressure, prepared share, exact revisit and held-B/stale-A supersession, retained epochs | Keep and strengthen observed owner release; physical envelope becomes telemetry |
| Timing control; BPR-0967/1598 | Known synchronous/final/contended 120 ms stalls remain visible; scheduler attribution of 40 ms work remains bounded | Keep clocks; explicitly mark the uncovered heap sub-observation unavailable |

No cache capacity, byte-accounting formula, fixture input, source/anchor,
eviction/revisit, supersession, cleanup, shutdown or latency limit is weakened.
The physical limits stay Mac 160/184 MiB, Windows 160/208, Ubuntu 168/240
(ordinary/cache bytes). The covered allocated-heap cleanup limits remain
84 MiB while the application lives and 32 after destruction; covered sampled
heap peaks remain the original per-platform limits, including Linux cache
bytes at 216 MiB. Only current process footprint and its process-lifetime peak
cease to decide the four CI contract rows.

### Counter coverage and supported-option observations

Native malloc-zone/UCRT-process-heap/glibc counters exclude mimalloc. A new
standalone control against the exact pinned runtime allocates and touches
64 MiB: a main-thread query observes it, but a query while a different live
thread holds it reports only 40,987,168 bytes until that worker explicitly
collects, then 67,136,032. `mi_stats_get` is therefore not substituted as a
qualified all-thread oracle. Pinned `src/arena.c` explicitly assumes the caller
is the only thread running in that heap when visiting blocks. No concurrent
heap walking or new production synchronization hook is introduced.

Same-byte upstream-supported option comparisons use test environment settings
only, with no zero delay, THP disable or process/child global policy. All eight
separate native response courses pass their existing budgets. These loaded-
close peaks are MiB before the first cleanup collection:

| Native setting | Mac peak | Ubuntu peak |
|---|---:|---:|
| Original defaults | 160.56 | 293.52 |
| Lazy arena commit | 162.97 | 187.43 |
| Lazy commit + 100 ms purge delay | 156.36 | 181.82 |
| Lazy commit + 10 ms purge delay | 158.48 | 176.56 |

Linux improves substantially, but every variant still exceeds the original
168 MiB envelope. Mac results are near its 160 MiB boundary. This is not a
reason to choose a default merely to hit a legacy number. No option is adopted;
product/provider/package sources and defaults remain unchanged. Windows has
no new option-profile claim. Raw identities/argv/options, complete public-path
reports, original current/peak samples and response results are under
`build/mimalloc-memory-r8/option-*` and its `linux/` counterpart.

### Rejected process-memory gates and direct fault control

The first prototype (`4dbbb8981`) recreates full composition in a QApplication
that production would exit. Its five-cycle initial-variation rule is rejected:
Linux baseline 90.41→107.03 MiB fails its 5.79 MiB allowance, while mimalloc
142.73→183.91 passes because its larger initial variation grants 19.99 MiB.
The rule does not compare the allocators fairly. All raw samples and the
retained-raster-cache control remain in `stability-*`; the prototype is removed
from the final source rather than registered as a green test.

The bounded replacement uses the real Home → Close review → open → select-
large-source path five times within one live application. The initial harness
left its recording one-shot and timed out; the existing complete repeat-read
switches correct that, without changing recorded request validation. A final
loaded-close check also needed to bind the final reopened comparison instead
of the first. Failed attempts remain separate. These are harness corrections,
not product-memory evidence or retries to seek a passing scalar.

A predeclared 4 MiB last-three settled range does not validate across the
allocators. The no-injected-fault mimalloc samples span 6.12 MiB on Mac and
6.79 on Ubuntu before that final binding check. They are retained rather than
rerun for a green number. The bounded completion runs only baseline normal
and the deliberate retained-owner controls, then ends the gate investigation:

| Native allocator / course | Absolute post-collection footprint MiB, five real close/reopen boundaries | Last-three range | Owner result |
|---|---|---:|---|
| Mac mimalloc, no injected fault | 125.81, 142.47, 137.81, 143.94, 139.84 | 6.12 | Final release checks unreached after stale owner-binding failure |
| Mac mimalloc, retained owner | 123.77, 139.11, 142.45, 146.91, 149.83 | 7.38 | Actual completed-owner assertion fails |
| Mac baseline, normal | 110.25, 115.31, 116.24, 118.14, 118.41 | 2.17 | Every tracked reopened owner releases |
| Mac baseline, retained owner | 110.02, 117.20, 121.92, 127.55, 130.44 | 8.52 | Actual completed-owner assertion fails |
| Ubuntu mimalloc, no injected fault | 272.91, 286.30, 283.79, 290.17, 290.57 | 6.79 | Final release checks unreached after stale owner-binding failure |
| Ubuntu mimalloc, retained owner | 273.12, 286.19, 294.05, 292.15, 302.08 | 9.93 | Actual completed-owner assertion fails |
| Ubuntu baseline, normal | 142.69, 148.73, 150.05, 150.73, 151.99 | 1.93 | Every tracked reopened owner releases; legacy numeric miss remains |
| Ubuntu baseline, retained owner | 145.15, 153.52, 157.72, 161.31, 164.26 | 6.54 | Actual completed-owner assertion fails |

A threshold raised to accept the observed 6.79 MiB mimalloc span would hide
the known 6.54 MiB baseline fault span. These observations do not establish
absence of another untracked leak; they demonstrate that this scalar is not
qualified to distinguish the cases. No such metric is adopted. The retained-
owner control is independently meaningful: it holds actual computed comparison
objects, and the normal public shutdown/destruction then trips their weak-owner
release assertion. It does not force a failure by checking a control flag.

### Implemented policy and verification boundary

Candidate `355669cc1f4419acb01c1e139945e84d49200d92` changes only test code:

- Four existing CTest resource rows carry `--resource-contracts` and use the
  distinct `LARGE_REVIEW_RESOURCE_CONTRACTS_OK` marker. Raw current/peak values,
  original limits and misses remain; per-predicate `Enforced=false` makes the
  physical-envelope retirement explicit. Plain original flags still enforce
  that legacy envelope and can still fail it.
- Native allocated-heap checks remain gated where covered. Mimalloc and
  replacement allocator instrumentation get no invalid independent native
  heap sampler. `NativeHeapCounterCoversAllocator=false`, `Enforced=false`
  and unset/null `WithinLimit` identify unavailable observations; zero is
  never treated as allocated-heap acceptance. The timing control retains all
  clock/stall assertions and reports that sub-observation unmeasured.
- Every observed cache-pressure diff/index is checked after actual application
  destruction, including entries between the original first/final probes.
  `--cache-entries --retain-middle-owner` holds one such middle result outside
  the cache. The course must fail this new assertion while the ordinary final
  diff/index owners still release, so the stronger gate is not vacuous.

No production default, core/application/Qt policy, runtime pin, cache limit,
package or version changes. The real repeated-reopen diagnostic remains an
opt-in attribution tool, not a new memory gate. [[BPR-1768]] records the
unqualified process/allocated-heap risk separately and does not claim a leak
repair or bounded RAM. Native signed-installed/delivered Cocoa Mac, installed
package/source/trust agreement and final RFC closeout remain pending under r5.
No release, signing, credentials, push or desktop operation occurred.

### Exact native useful-contract verification

The unchanged C++/CMake producer is `355669cc1`; main integrates its final
bounded tree as `64752aa7e`. Producer `4585f67aa` adds only the Python summary
and static manifest inventory corrections; its three C++/CMake blobs match
`355669cc1` exactly. The revised course does **not** collect on ordinary runs.
It changes the CI verdict policy explicitly, not allocator behavior.

| Release native station / allocator | Seven registered rows | Four resource owner/covered-heap contracts | Four additional mimalloc public workflows |
|---|---|---|---|
| Apple Silicon Mac / baseline | 7/7 | 4/4 | — |
| Apple Silicon Mac / mimalloc | 7/7 | 4/4 | 4/4 |
| Ubuntu 26.04 x64 WSL2 / baseline | 7/7 | 4/4 | — |
| Ubuntu 26.04 x64 WSL2 / mimalloc | 7/7 | 4/4 | 4/4 |

The seven rows are launch, responsiveness, four resource courses and timing
control. Additional workflows are composed benchmark, active work, prepared
open and held-provider cancellation. Each cache-entry course records 67 live comparison observations, cache bytes
records 21, and resources/loaded close each record one. Repeated observations
may name the same owner; no unique-owner count is claimed. All observed diff/index owners expire after real destruction, every
original cache/source/shutdown assertion holds, and every available baseline
allocated-heap predicate passes. Mimalloc allocated-heap observations remain
explicitly unmeasured. The middle-owner control fails on both hosts at the
actual owner-release assertion while the original final diff/index probes
pass. Invalid suffixes cannot bypass a response course.

Observed lifetime-peak growth remains telemetry, in MiB; values above the old
limit are still misses, even though the new contract row passes:

| Native allocator | Resources | Loaded close | Cache bytes | Cache entries |
|---|---:|---:|---:|---:|
| Mac baseline | 144.84 | 148.31 | 198.94 | 103.61 |
| Mac mimalloc | 161.61 | 164.06 | 191.11 | 106.31 |
| Ubuntu baseline | 177.17 | 177.82 | 232.47 | 120.48 |
| Ubuntu mimalloc | 275.86 | 280.21 | 352.30 | 194.88 |

These are original baseline growth readings, not rebased or post-collection
figures. Mac baseline cache bytes now also misses its 184 MiB physical
telemetry; that does not erase its historical passing cohort. A separate
plain legacy mimalloc resources invocation happens to pass on Mac but fails
on Ubuntu, exactly as its own recorded predicates require. It is not a retry
of a failed row nor evidence that the observed Mac contract-row misses were
repaired. Physical variability and allocation/retention remain material.

Same-producer response observations, baseline → mimalloc: Mac composition
startup 405.36→405.60 ms, large-file selection 385.79→391.12 ms; Ubuntu
295.29→285.46 and 333.97→337.08 ms. Both full response courses pass unchanged
limits. These single observations do not establish a speedup or rule out
other workflow regressions. The earlier options/idle results retain the
tradeoff: lazy commit can reduce residency on Linux, shorter purge delays do
not by themselves purge an idle process, and forced release costs/reuse/page
faults require real-workflow justification. No product option is adopted.

Exact benchmark SHA-256 values:

- Mac baseline `ba204c230f76005e2f320965969662ff4eddb0e732f12039cec40a03e8877b0c`;
  mimalloc `295e250b9b17dc76ee3d614e11b5d32d109b02fd783c8ad17003de157b8961f9`.
- Ubuntu baseline `8c3147c89887c486b088279aadc3b905d444a6a17d4fe844ebf2f3d4b8c5d215`;
  mimalloc `e483068e61431b08a3f5eb103da091c9cf9c784b4cecaceb2372ac0627c0ab3c`.

Runtime hashes remain Mac
`90c1974c304ab7b5ba283f603b9c3f9f38a64edd03e9c847b7a2fee54e6a4627`
and Ubuntu `72c4fd05b110d40fb4e49402d2bd39414cf2957017c6e0c144bff606946464ca`.
No allocator environment overrides or injected loader environment are present.
Raw identities/images/reports remain in `build/mimalloc-memory-r8/final-*`
and the task-owned native Linux root; collected Linux records are under
`build/mimalloc-memory-r8/linux-final/`.

The Python summary now groups by phase **and collection stage**, preserving
pre/post distinctions. It reports unsupported heap figures as null/unavailable
and retains raw native counters, policy, enforced misses, telemetry misses and
owner outcomes, including in brief output. Actual R8 baseline/mimalloc reports
and four R7 collected reports verify coverage, stage separation and historical
R7 enforcement. No raw report is rewritten. The static network manifest found
a missing Mac-only update-helper pending-inventory entry; the correction
explicitly says unmeasured on Windows. Building the local transport test
executable resolves its absent-command inventory check; no transport, GitHub
or credential test is run. The Mac static manifest then passes.

### Intermediate Windows checkpoint — retained composite failure

The first exact `355669cc1` Windows 11/MSVC Release cohort is **5/7**, not a
passing suite. Responsiveness, timing control, entries, loaded close and
resources pass. Launch fails its unchanged child response predicate
`theme_light=100.66 ms > 100`; cache bytes reaches the existing correctness
assertion `stale comparison published while superseding file B remained
selected`. It has 8 source observations, one supersession with outstanding
work, and 22 byte-pressure selections. Cleanup/owner assertions are not
reached for that row. This is not physical-memory telemetry or an accepted
pass. No BPR-0967 cause is inferred. Additional mimalloc workflows are 3/4:
composed benchmark, prepared open and provider cancellation pass; active work
retains an input/drain timing failure.

The first direct Windows owner-control invocation exits before any report
because its harness omitted the CTest Qt/build binary search paths. That
empty-log/absent-report attempt is unaccepted and preserved; the corrected
native environment must run the control. The Mac/Linux controls remain valid.
A matched Windows baseline is now active, using the same exact producer and
unchanged course. The coordinator requires one diagnostic exact-candidate
reproduction after that comparison and a bounded correction if the semantic
failure reproduces. It must remain a correctness gate. No retry-to-green,
threshold relaxation or broad design change is authorized by this checkpoint.

The static gate's only C++ findings were two unnecessary temporary shared
handle copies in the observer. Producer `1fafe3db9` / main `c67f07196` observes
them by const reference. Format 22 and clang-tidy 22 pass; exact-final focused
native owner/resource checks are continuing. The full matched cohort above
remains explicitly bound to `355669cc1`. Current commands/state are in
`build/mimalloc-memory-r8/mandate.json`; final quiescent handoff is not yet made.

### Corrected held-B barrier and final native evidence

The Windows `0052566b9` diagnostic reproduces the old composite failure but
falsifies a stale-publication explanation: target A is epoch 1, `drained=1`,
`stale=0`, eight source observations, selected B and its exact key both true.
Only the second `settled()` recheck is false. The original assertion's message
conflates a later quiescence change with stale source. Neither the source
observer nor its key/text checks is weakened to resolve it.

Producer `e75a78a723af100fb9a284fe9b24c5347ef1a884`, integrated on main as
`e1d222aab`, keeps the observers connected through the existing separately
measured `wait_preparation_tail` before running every original final predicate.
Source diagnostics confirm work the old epoch-job-only snapshot misses: Mac
has 14 comparisons preparing with zero epoch jobs; Windows has 11 preparing
and one preparation epoch job; Ubuntu has seven preparing with zero epoch
jobs. The full tail drains that deferred work. The observed MI tail costs are
628.09 ms on Mac and 1315.79 ms on Windows, reported separately from reviewer
readiness; no input or completion limit changes. No stale source is observed.
All final predicate bits, exact-source assertions, shutdown and owner checks
then pass. This is a stronger harness completion boundary, not a product race
repair, a disabled correctness check, or a performance improvement.

All four exact-final mimalloc resource paths pass on Mac, Windows and Ubuntu.
Each native final middle-owner control fails at the actual tracked-owner
assertion with 67 comparison observations while the original final diff/index
owners release. The first Windows loader-only attempt is superseded by a
separate corrected harness which applies the registered CTest environment,
including Qt/build binary search paths; no source or permission exemption is
needed. Plain legacy resources remains a failing numerical qualification on
all three final hosts in its own separately preserved invocation. Mac's
previous passing invocation remains recorded; none is selected as a repair.
Invalid suffix checks still reject attempts to bypass a response course.
Mac and Ubuntu also pass the final baseline byte-pressure course with the
stronger tail, preserving their available native allocated-heap gates.

The original matched Windows `355669cc1` cohort is baseline 7/7 and mimalloc
5/7; its actual failures are never rewritten. The corrected exact-final cache
course is a separate positive observation. A single predeclared same-source
Windows active-work pair completes with both allocators passing: baseline
close dispatch/drain 5.7239/44.2379 ms, mimalloc 4.0975/47.9343 ms, against the
unchanged 150 ms drain limit. There is no retry loop. The original 259.9814 ms
MI close failure remains evidence, as does launch's 100.66/100 ms miss. The
later pair does not establish their cause, a speedup or universal timing.
Resource-course wall times are not allocator throughput comparisons: native
heap sampling is available on the baseline and explicitly unavailable on MI.
The unsampled response/active-work courses supply the response observations.

Final format 22 and clang-tidy 22 pass. The summary consumer's actual-report
checks retain R7 collection-stage separation and historical enforcement, and
represent uncovered heap as unavailable rather than zero-byte acceptance.
Main's five test/script blobs match the native `e75a78a72` producer exactly.
Production, provider, package, pin, cache policy and version paths remain
unchanged from `0f799140c`; prior sanitizer/fuzzer and native product/provider
checks remain bound to their original exact producers and are reused.

### Manifest boundary, retained risk and handoff

The final Mac/Ubuntu static manifest checks pass; two targeted inventory
controls still require a pin everywhere and reject an unmeasured Windows pin.
The registered Windows manifest check remains red. Under its actual container,
`ctest --version`, CTest JSON enumeration and `cmake --version` all exit
3221225794 (`0xC0000142`, DLL initialization failure), with no output. This is
the previously measured CMake-tool initialization class, not evidence that a
product allocation fails. The identical direct read-only structural verifier
can enumerate the graph but rejects three existing unmeasured Windows pins:
`sqlite_agent_setup_storage`, `workspace_clone_folder_scan`, and
`application_agent_finding_codex_found_contract`. All three are already present
in base `73877b822`. No container exemption, inventory waiver or security-policy
change is made. No global native-security or release approval is inferred.

The remaining process/allocated-heap qualification risk stays open on
[[BPR-1768]], independent of the original RFC closeout under the explicit owner
mandate. Useful passing ownership/cache/source/lifecycle tests do not bound
arbitrary MI live allocations, retained pages, process growth or peak memory.
The large recorded memory cost and every historical limit/miss remain. No
product option, zero-delay default, forced-purge hook or child-global setting
is adopted. Signed-installed/delivered Cocoa Mac, source/package/trust agreement
and final BPR-1759/1762/1764 acceptance stay pending under r5; native Intel
execution remains waived, not claimed.

Final raw records and identities are in `build/mimalloc-memory-r8/`, its
`linux-final/` and `windows-final/` collections, `final-e75a78a72/`,
`supersession-tail-{mimalloc,baseline}/`, `active-close-pair-*`,
`manifest-diagnosis/`, and `current-mimalloc-image/`. Original failed
`final-*`, `supersession-trace-mimalloc/` and loader attempts remain separate.
Additional MI workflow reports are copied after their actual runs into
`final-mimalloc-public-workflows/`; earlier glob copies made before those runs
are not their authoritative reports. Graph restores can produce different
binary hashes; identities remain per run. Restored MI images are separately
preserved and checked through their public resources course when their hash
changes, rather than relabeled as the earlier matrix image.

Native roots are `/home/mmarcin/bpr-rfc0093-memory-r8/source` and
`C:/code/bpr-rfc0093-memory-r8/source`; the Mac source/build remains
`/Users/jaeger/.codex/worktrees/mimalloc-memory-r8/BetterPullReview`. All earlier
candidates and roots remain protected. No release, signing, credential lookup,
push or desktop capture occurs. No attended session/server/URL/cursor is active.

The exact restored/current MI image hashes, each preserved with its runtime and
checked through public resources when relinking changed its hash, are:

- Mac `b42fcb00b1ac6eaf860f8ac352c60b45c945e3a7c023b9881b6009289bf093de`.
- Ubuntu `77d6428a4d9d2e78e660f1bf8a44b5432553ab8e5cdec387811fb5e7c31b378a`.
- Windows `0165d8a0c6b4d42f924911f2bc352328de9d4c0a725bb72ff9c63e81577d802d`.

`final-verification-summary.json` validates all twelve source-bound final MI
resource paths, three actual retained-owner failures, three separate legacy
numeric failures, two covered baseline tail courses and the two Windows close
observations. Each run keeps its actual image identity. The full earlier
matched seven-row matrix remains bound to `355669cc1`; no mixed set is called
a new full seven-row passing cohort. The bounded useful-test amendment is
complete. Distribution exit 1 and final native Mac/package closeout remain
pending; exit 2 remains completed reporting with historical failures intact.
