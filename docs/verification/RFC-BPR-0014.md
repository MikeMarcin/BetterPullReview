# RFC-BPR-0014 verification and planning ledger

RFC base: `d0a8a6dda005af12b3818234f4878a9f48a44146`.
Mandate `r1`, amended by the owner's explicit release-workflow reuse instruction
`r2` and prescription-modernization instruction `r3`, authorizes this run;
current child `/root/rfc0014_m10` owns only
`rfc-bpr-0014-release-readiness/m10-documentation-impact` on `main` under
coordinator-implementer rev2. The earlier records below retain their original
child and candidate provenance; M8/M9 remain blocked.
The [import record](../rfcs/assets/rfc-bpr-0014-planning-v1.LIVE_IMPORT.md)
establishes the baseline and provenance. The
[live closeout](../issues/10-backlog/BPR-0965-close-rfc-0014.md) retains
all nine canonical acceptance criteria and the predecessor gate.

## Inherited evidence and limits

BPR-0253 and BPR-0324–0328 are Done icon/identity prerequisites; their existing
asset, bundle and public-icon checks remain their own evidence.
BPR-0908's candidate `b7e92258bb6115388e1c837730bb5977cc6c4eb2` supplies
a universal tests-disabled macOS release tool, allow-listed deployment,
Developer ID signing, relocated launch, DMG/ZIP and checksums. Its recorded
run explicitly skipped notarization and did not publish.

RFC-0035 later supplied disposable signed/notarized Apple Silicon updater
qualification. The current RFC-0035 ledger retains the exact
`5db1d104a2436a2b0df811ac2e40cff0c58c2319` package source separately from
later menu/documentation changes. None of this proves the RFC-0014 delivered
matrix, complete SBOM/licensing, clean install/uninstall or Intel execution.
M2 must inspect reusable inputs before carrying any evidence forward.
The historical package README contains older foundation platform observations;
they are not current package certification.

## Acceptance and direct verification plan

Every row is pending full RFC acceptance. The closeout stores the verbatim
criteria; the row numbers below follow their existing order, not new requirements.

| RFC row | Direct falsifiable check and terminal observation | Owning cards | Current disposition |
|---|---|---|---|
| 1 Clean installation and operation | Install the exact candidate in a clean native user account; launch, authenticate through an authorized safe route, open a fixture PR, select a file and verify both displayed source identities; uninstall and inspect residuals | BPR-0956–0961 | M8 has three scoped macOS human accepts and Windows mechanics; full clean-account/native/vault/removal matrix is pending |
| 2 Package trust | Verify the delivered installer/app/package signature, macOS notarization/staple and checksum using native trust tools independently of package generation; reject altered/untrusted artifacts | BPR-0962 | Current macOS candidate is ad-hoc, Windows MSI unsigned, native Linux package absent; prior updater trust is not transferable |
| 3 Runtime attack surface | Inspect packaged dependency closure plus native sockets, spawned processes, Qt plugin/library resolution and network destinations under public operation; compare with explicit allow-lists | BPR-0956, BPR-0957, BPR-0962 | Exact Mac/Windows inventory and bounded runtime/IPC courses pass; active-operation and full native matrix remain pending |
| 4 Inventory and licensing | Generate SBOM/notices/source-offer manifest from the actual shipped closure, reconcile versions/provenance and hash every delivered artifact; fail on omissions or substitutions | BPR-0956, BPR-0962 | Exact revised macOS licensing-v002 owner criterion accepted at hand-back 470; complete delivered matrix and revised Windows/Linux obligations remain pending |
| 5 Release hardening policy | Name platform compiler/linker/runtime flags and release policy; run relevant static analysis, sanitizer/fuzz, secret/vulnerability scans with tool/database versions and adjudicated findings | BPR-0957, BPR-0962 | Policy and bounded M9 checks recorded; 21 Mac/19 Windows Grype findings, external hardening and native lifecycle coverage remain open |
| 6 Storage and removal | Exercise vault/file/cache permissions, retention, explicit purge, migration, crash recovery and uninstall in disposable roots; compare actual retained/deleted items with docs | BPR-0953–0955, BPR-0958–0961 | Retained-lease maintenance is composed and fixture-tested; real native credential and complete installed removal courses remain pending |
| 7 Diagnostic privacy | Seed source/comment/token canaries, preview/export diagnostics and inspect crash/log artifacts; defaults omit bodies/secrets and redaction survives hostile inputs | BPR-0953–0955, BPR-0957, BPR-0962 | Metadata preview/export and hostile courses pass at recorded candidates; resident source/comment/token crash course and native matrix remain pending |
| 8 Corporate packet | Reconcile network/storage/process/credential/AI/logging/removal claims with observed composed effects and final threat model | BPR-0952, BPR-0963 | M10 publishes a tracked source-backed packet and operator instructions; pending artifact/native/security qualification is explicit, not certified by this document |
| 9 Packaged architecture | Build/test core with Qt/provider unavailable; inspect delivered targets, required install rules and packaged symbols/dependencies; trace real host effects and exact selected comparison through composition | BPR-0952, BPR-0956, BPR-0957, BPR-0962 | Existing architecture checks must be bound to final package inputs |

A successful launch alone is insufficient for row 1: the terminal observation
is the exact selected-file comparison through the shipped alignment controller
and DiffLinkMap. A passing fixture cannot prove independent signing trust,
native credential continuity, clean installation or a human verdict.
Release policy also retains the wider [release gates](release-gates.md);
an RFC milestone does not authorize publishing a release.

## Platform and build matrix

| Target | Required package/build scope | Evidence owner and unavailable dependency |
|---|---|---|
| Windows 10/11 x64 | Supported MSVC toolchain; signed per-user installer; native PE/dependency/vault/runtime and high-DPI checks | BPR-0960 native operator/station plus BPR-0962 signing authority; not run here |
| macOS 13+ Apple Silicon | Universal arm64/x86_64 tests-disabled app and DMG/ZIP; hardened signature/notarization; clean native install/render/remove | BPR-0958 native operator plus BPR-0962 trust evidence; current host is arm64 but no clean package run occurred in M1 |
| macOS 13+ Intel | Same exact universal package, actual native Intel install/launch/auth/render/remove | BPR-0959 Intel operator/hardware; updater BPR-0950 deferral does not satisfy it |
| Linux x86_64 | Declared glibc 2.39 floor: Ubuntu 24.04 LTS DEB and RHEL 10 RPM qualification targets; signed packages, dependency/desktop/plugin/vault/render/uninstall checks and default per-user route | BPR-0961 Linux operators/stations plus BPR-0962 signing authority; no native qualification yet; RHEL 10's x86-64-v3 and actual C++ runtime requirements must be explicit |
| Core-only | No Qt/provider discovery or link dependency; deterministic build/tests and delivered target/dependency inspection; audit any install/export rules actually selected | BPR-0953, BPR-0956, BPR-0957 |

No additional architecture is implied. Signing identities, clean native
environments and any attended decisions are required external inputs when
their cards reach them. This planning milestone discovers no current attended
candidate and starts no review server.

## Initial architecture allocation and identity plan

M2 completes the source-level inventory before implementation. This ledger
fixes the ownership obligations so a later no-impact conclusion needs evidence.

| Region | Planned authority and work | Required evidence |
|---|---|---|
| Functional core | Compact retention/purge/diagnostic-selection policy, command legality, typed operation identity and stale decisions; no file paths as deletion authority and no effect APIs | BPR-0952/0953 exact reusable or new transitions and deterministic refusal tests |
| Application host | Validated bounded private-root/vault/catalog observations, maintenance/migration/export jobs, effect ports, cancellation and quiescent lifecycle | BPR-0952/0954 fake-port/manual-executor failures plus disposable-root evidence |
| Platform adapters | Native filesystem/vault/package/process mechanisms through narrow capabilities; secure signing inputs remain external | BPR-0954/0956/0962 permissions, containment, runtime and independent trust checks |
| Qt presentation | User gesture, confirmation, preview, focus/accessibility, incremental command/model projection; no provider or storage effects | BPR-0955 public rendered interaction and bounded update checks |
| Composition and packaging | Select first-party adapters and actual private roots, deploy only required declared dependencies/plugins, preserve isolated core target and updater boundary | BPR-0956/0957/0962 tests-disabled relocated public path and dependency inspection |
| GitHub/shared state | GitHub remains authoritative; packaging and local maintenance cannot mutate shared review history | Fixture/read-only paths by default; any remote mutation needs an expressly designated safe target |

The intended path is Qt semantic intent → core eligibility using host-supplied
bounded observations → typed host request → adapter effect → typed completion
on the serialized decision lane → core acceptance → bounded delta/projection.
Every async request carries the applicable request, profile/session, authority,
catalog or package revision and generation. A stale/duplicate completion cannot
choose a deletion target, publish a diagnostic or replace current UI state.
The host captures exact root/object identity and owns cancellation/recovery
until quiescence; a human confirmation cannot widen that captured authority.
These are planning constraints, not claims of newly implemented types.

Routine UI updates must scale with the active and changed sets. Jobs bound
input/output, residency and outstanding operations; previews use bounded
summaries, and status/progress changes do not reset the review model or scan
all source/history. M2 must identify existing limits and M7 must choose direct
counters/negative controls that would detect a violated claim.

For M1 alone, core, application, adapters, composition, package scripts and
build definitions have no behavior impact: only allocated cards, RFC metadata,
planning/provenance and this ledger change. No source/runtime qualification is
claimed by documentation tests.

## Documentation impact ledger

| Semantic obligation | Owning documents/surfaces | Disposition and owner |
|---|---|---|
| Core/host/Qt and typed ownership, identity, concurrency, projections | System architecture, maintenance contract, UX specification and architecture-conformance matrix | Updated in M10 against composed ownership and direct test names; no behavioral edit |
| Application build/install/package, supported baseline, dependencies and license/source offer | README, build-and-test, packaging README and release tooling/operator guide | M10 documents existing drivers, observed producers, separate source asset, eight-asset publication contract and pending trust/native inputs |
| Credentials, storage, retention/purge, migration and removal | Product contract, foundation security, security threat model and storage/removal guide | M10 distinguishes remembered grants, exact cleanup, journals, residuals, raw logs and metadata export |
| Network, subprocesses, AI, diagnostics and corporate posture | Security threat model and corporate packet | M10 packet reconciles source, actual scoped evidence, AF_UNIX exception, pending privacy and unresolved vulnerabilities |
| Qualified macOS updater and manual managed/Windows/Linux policy | Existing release-updater architecture/security and application-updates guide | No change: existing documents already describe exact authority, private draft recovery, transaction residuals and qualification gaps; new guides cross-link them |
| RFC acceptance, canonical order and continuation | RFC/index, adopted plan, live cards, this ledger and BPR-0965 | M10 updates progress only; index/acceptance/plan unchanged, RFC remains Proposed; M12 alone may close |
| Agent guidance | Root/packaging AGENTS.md, docs AGENTS.md and consumer context | Root/packaging add evidenced hazards; docs/consumer no impact because ownership/configuration and documentation rules did not change |

## Safe fixtures and external boundaries

Use deterministic local fixtures and disposable installation/profile roots.
Authentication fixtures must exercise the selected production capability path
with any test-only seam absent from the delivered package. When true native
authentication cannot be proven by a fixture, preserve the native/live gate;
do not rename the fixture as live evidence. No safe live mutation target is
designated for this milestone. No GitHub/GHES mutation, release publication,
credential enrollment, installed-app replacement or notarization submission
occurred. Later signing/release work uses the configured companion skill when
applicable and the scope then authorized by the coordinator/user.

## Milestone 1 verification and continuation

Shortest public workflow before changes: target-only plan discovery reported
`absent`; the RFC queue reported seven selected/Done cards, no active card and
no closeout. These are workflow observations, not application runtime checks.
The final public workflow must discover the adopted plan by RFC ID and return
the next eligible architecture card after BPR-0951 is Done.

The original r1 binding is
[checkpoint-m1.json](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m1.json).
It binds the whole BPR-0951 acceptance section and all three milestone exits.
Helpers report structural consistency only; the assigned implementer owns
the completeness judgment. Focused final gate outcomes and integrated revisions
are recorded below before handoff.

Planning candidate: `75d9901382f6b1ec04f570d854c93b81889d44b4`.
At this exact revision, the documentation gate reports `DOCUMENTATION_OK`,
scoped board validation reports valid with 957 total cards, target-only discovery
finds the adopted twelve-stage plan, the immutable source-copy comparison passes,
and the scoped product/build/test/package diff against the RFC base is empty.
Receipts are `build/rfc0014-run/m1-{documentation,board,discovery,import-fidelity,no-impact}-evidence.json`.

The 64 existing documentation-verifier tests passed in 2.131 seconds during
candidate preparation at claim revision `cec196fb` plus the planning working
tree. Their test/script inputs did not change; hostile-fixture `ERROR` lines
are expected rejection assertions and the suite exited zero. A negative
checkpoint bind for M2 without accepted M1 exited 2 with unmet prerequisites,
demonstrating that the next stage is not silently eligible. Initial staged
whitespace inspection caught generated trailing blank lines; they were removed
and the still-unpublished proposal receipt regenerated before the candidate
commit. The final staged whitespace check passed.

Self-review checked all twelve source stages and six suggested implementation
slices against live owners, all nine canonical rows, per-platform metadata,
dependency order, preserved updater amendments and absence of product edits.
The original source files and seven historical Done cards are unchanged.
No independent or human verdict is required for this planning milestone;
none is claimed. Remaining native/security/parent acceptance belongs to M2–M12.

BPR-0951 is Done at `314fd9a7782c03dbef96c924a22ecaa28d3ccb5c`;
BPR-0952 is Ready for a fresh milestone-2 owner. The
[durable handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m1.json)
preserves exact revisions, changed paths, command state and pending obligations.
No child agent, background command or review server is handed off.

## Owner amendment r2: existing macOS release workflow

The owner explicitly directed M1 to inventory and reuse the project
[publish-release skill](../../.codex/skills/publish-release/SKILL.md).
The complete skill and both underlying scripts were read at
`9778997d565fa3b5c89540bd52799cc0c12e2982`; their contents are unchanged by M1.
This is an implementation inventory, not a newly observed release or proof
that a host loaded a rendered skill. No package, signing/notary command,
remote preflight, tag, push or release command was run for the inventory.

| Existing source mechanism | What is already implemented and should be reused | Evidence limit / remaining RFC obligation |
|---|---|---|
| Skill preparation | Choose an explicit version, commit CMake version and release notes, verify private repository status; stage publication after package review | Procedure exists; no version or publication authority is granted by M1 |
| `tools/release.sh` and `macos-release` | Clean/version guards; normal dev configure/build/CTest gate; tests-disabled Release with `arm64;x86_64`; exact bundle identity/version checks | Reuse this build entry point. Two slices and preset selection do not prove native Intel or macOS 13 deployment compatibility; the preset does not explicitly set a deployment target |
| Qt deployment and relocation | `macdeployqt`; allow-list five frameworks and four plugins; install-local `qt.conf`; RPATH normalization; per-slice dependency inspection and universal check | Reuse and qualify this mechanism against the final closure, including the update helper. It is not a complete SBOM, license/source-offer or live runtime/socket/process audit |
| Signing | Sign nested Mach-O/framework code and the app with Developer ID, timestamp and hardened runtime; deep/strict signature verification plus bundle/team identity and no-entitlement checks | Reuse existing signing. Native independent trust verification is still required for each final artifact; credential availability was not queried |
| Notarization and trust | Default path submits the app and DMG with the configured Keychain profile, staples/validates both, Gatekeeper-assesses execution/open and signs the DMG | The skill treats a missing notary profile as setup work. Its existence and old BPR-0908 setup notes do not establish today's profile or a fresh notarization result |
| Smoke and artifacts | Relocated isolated-profile launch requires the private-profile and production-bundle markers; emits universal DMG, ZIP and their SHA-256 files under `build/releases/VERSION/` | The smoke accepts exit 0 or 1 with those markers; it does not authenticate or open the exact fixture PR/diff, establish clean install/uninstall or native Intel evidence |
| `tools/publish-release.sh --dry-run` | Check clean main/version/notes, four files and checksum validity; verify exact PRIVATE repository, absent local/remote tag and absent release | This path performs remote reads. It was inspected, not executed, and does not authorize publication or prove final package qualification |
| Explicit publication branch | Recheck PRIVATE; create annotated tag; atomically push main and tag; recheck PRIVATE; create the release with DMG/ZIP/two checksums and return release metadata | Use only after the user requests publishing that prepared version. M1 authorizes no tag/push/release. Partial mutation is inspected before retry; the skill separately requires verifying URL, tag, four assets and visibility |

The source inventory fingerprints are:

- Skill: `c5e654bade74c4821cdde0109f623cefa1ae618c44b5184a7a59b4b427810bb2`.
- Release script: `251b6be79236871b946f87b62517201adaf7b62b2bb41866e41c877ad8896202`.
- Publish script: `50e0e3e8f521d9c418b01d119ba62f556ea346a1b5c39c79719ea13bee87e62f`.
- Presets: `d341a40099f605a6dde518962def369e210eec950c886c5301e5b8a1ecb27fb9`.

BPR-0908 and RFC-0035 provide only their previously recorded candidate-bound
observations, described above. Skill text, code availability and static shell
syntax checks are distinct from executed package evidence. The normal release
workflow may be reused and extended for demonstrated gaps; it must not be
reimplemented merely because the generic authored plan says “implement”.
M2 inventories these exact inputs and their compatibility; M6 completes missing
cross-platform/install/inventory behavior; M9 obtains final trust/security proof.
Windows/Linux packaging, clean native operation/removal, SBOM/licensing,
diagnostics/retention, corporate security and canonical closeout remain open.
Skip-tests/skip-notarize/allow-dirty are not publication defaults; the skill
requires the user's explicit acceptance of their limitations for a publication.

The [checked amendment](../rfcs/plans/rfc-bpr-0014-release-readiness/amendments/r2/check.json)
changes only future M2/M6/M9 descriptions and preserves all work selectors,
acceptance exits and M1's definition. An initial colocated proposal was refused
because the helper compared it with the existing plan's generated view; placing
the amendment source in its own directory produced the valid receipt without
changing the live source or losing its original provenance. The original
preparation/adoption receipts and r1 checkpoint remain historical.

The [r2 binding](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m1-r2.json)
retains identical M1 definition and obligation fingerprints. The amendment
checker identifies only future M2/M6/M9 changes. Shell syntax checks for both
existing scripts, adopted-plan and scoped-board checks, documentation and
whitespace gates pass after this inventory. No release script was executed.

The r2 source inventory and future plan amendment are committed at
`f66c512c7f48c01ab63d34c051675ce903872c1f`. At that exact candidate,
documentation, both script syntax checks and the expanded empty product/release-
skill diff check pass. Receipts are
`build/rfc0014-run/m1-r2-{documentation,release-syntax,publish-syntax,no-impact}-evidence.json`.
The current handoff carries mandate r2 and preserves original r1 acceptance.

## Owner amendment r3: current implementation prescriptions

The owner explicitly authorized revision of obsolete or no-longer-sensible
RFC/plan prescriptions based on the current repository, while preserving intended
outcomes/security obligations and recording material changes. M1 applies two
concrete prescription corrections; all nine canonical acceptance bullets remain
verbatim and the Windows/macOS Intel/Apple Silicon/Linux matrix stays required.

| Earlier prescription | Source-backed correction | Preserved obligation |
|---|---|---|
| Ship only required Qt and first-party runtime components | The selected architecture already includes required non-Qt third-party components such as SQLite, KSyntaxHighlighting, Quill and LibArchive. Declare and account for the actual minimal runtime closure, including static dependencies | Minimal trusted dependency/plugin surface; complete provenance, SBOM, notices and source offers |
| Core package/target plus future install/export composition | The current app ships native application bundles/helpers and deliberately configures as the audited top-level project. Require an independently Qt/provider-free core build/test target and only the install/export rules the actual application packages need, without inventing a standalone core SDK or general project embedding | Deterministic core isolation; delivered target/dependency graph; all effects through the packaged host/composition path |

The live RFC now names the existing release workflow and allows direct reuse or
evidence-backed no-impact completion of a generic core/host/Qt stage. The
original twelve-stage grouping, all work selectors and milestone exits remain.
BPR-0956's implementation criterion is reconciled to the clarified packaging
prescription; its platform, isolation and artifact obligations are retained.
README and the owning architecture document remove their stale suggestion that
RFC-0014 necessarily supplies general install/export embedding. Existing updater
and helper scope remains declared and must be inventoried, not silently removed.
There is no acceptance waiver, native-platform deferral, new publication
authority or product-code change in this amendment.

The original RFC snapshot, authored issue plan and r1/r2 records remain intact.
The [r3 amendment receipt](../rfcs/plans/rfc-bpr-0014-release-readiness/amendments/r3/check.json)
reports only future M2/M6 definition changes. The successor may use r3 authority
for further source-backed prescription updates, but must record any material
scope/acceptance change and preserve unavailable native/human decisions.

During r3 orientation another owner staged and integrated RFC-BPR-0036 on main.
This child paused main mutations and preserved the other index. Integration
completed with clean main at `cf490c5a9873f058ba5730759304424082f6a5d8`, retaining
the r2 handoff ancestor `03458e69`. The shared architecture diff was reviewed
before applying the two scoped documentation lines; its Markdown changes are
preserved. The immutable RFC base stays `d0a8a6dd`. Earlier empty product-diff
receipts remain true only for their earlier candidate; final M1 scope is verified
against the reconciled integration head and does not certify the unrelated RFC.

R3 candidate: `3deec8893c3f722096f797c45ecfde18e1e71534`.
The exact-candidate documentation gate and scoped product/release-tool/skill
no-impact check pass; the nine-bullet acceptance comparison is unchanged.
Receipts are `build/rfc0014-run/m1-r3-{documentation,no-impact}-evidence.json`.
The [r3 checkpoint](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m1-r3.json)
and current handoff preserve the original baseline, all prior acceptance limits
and the next Ready BPR-0952 milestone. The unchanged 64 documentation-verifier
tests and r2 script-syntax evidence remain reusable. No new runtime acceptance
or publication is claimed.

## Milestone 2: architecture allocation

Assignment `m2-architecture-allocation`, child `/root/rfc0014_m2`, mandate r3,
run `rfc0014-20260920`, segment `m2-architecture-allocation`, started from clean
`a9a1e3cd1b897bf1242a3dc46ccd986a9dfe0323`. Guarded claim
`303ba3b5e2f457f5b0f9385ea999e46c058b84d5` moved BPR-0952 into In Progress.
The [bound checkpoint](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m2.json)
uses accepted M1 and binds all BPR-0952 acceptance plus both M2 exits.

The [source allocation](../architecture/release-readiness.md) inventories the
real build/install/runtime graph, dependency provenance, existing macOS tools,
retention/migration/credentials/logging/profile/updater ownership, exact identity
and lifecycle, nine canonical public checks and documentation owners. M3–M5
reuse epoch maintenance and store/vault owners while adding only the required
diagnostic preview/export/clear and exact profile-purge policy/capabilities/UI.
The deferred RFC-0027 umbrella is not activated. Existing macOS release scripts
and the complete project publish-release skill remain unchanged and authoritative.

G1–G9 record the concrete remaining gaps: the production MCP helper filename/
copy, application installers, complete static/transitive provenance, macOS
deployment floor and per-user course, diagnostic controls/Windows sink,
profile-wide purge, native baselines, effective hardening/security policy, and
the existing authenticated local profile-activation IPC versus the old broad
no-local-server wording. That last criterion is explicitly unresolved for M9;
M2 does not waive it or omit the listener from inspection.

The Linux target fills the previously unspecified baseline: glibc 2.39,
Ubuntu 24.04 LTS x86_64 DEB and RHEL 10 x86_64 RPM, with the latter's CPU floor
and actual C++23 runtime/toolchain requirements explicit. The allocation cites
upstream Qt/Red Hat facts, not a BPR support result. M6 must reconcile any
inability to meet it; M8 must perform native qualification. No canonical
acceptance bullet, work selector, milestone exit, updater exception or
publication authority changed, so no milestone-plan amendment is needed.
The shared architecture's obsolete caller-installed-only Qt sentence is
corrected against the actual pinned provider; this changes no build behavior.

### Focused source/public evidence

At claim `303ba3b5`, with unchanged product inputs:

- `cmake --build --preset dev --target app_better_pull_review` passed. The
  real dev executable SHA-256 is
  `68fb0910686c6959777c26f59d898b109bdbf3ea18c89420816c60636140a597`.
- `ctest --test-dir build/dev --output-on-failure -R
  '^(application_epoch_maintenance_public_path|architecture_boundaries|composed_application_hosts)$'
  -j 1` exercised all three checks. Maintenance passed in 49.86 seconds through
  four real processes (seed, prune, restart/cache delete, restart/verify deleted).
  Composed-host linkage passed in 2.17 seconds. The suite exit was 8 because
  the checkout architecture scan found a duplicate AdapterGitHub declaration
  under an unrelated ignored `.claude/worktrees/agent-a17cae88115dbbb4e` checkout.
- `cmake -DBPR_ROOT=... -P .../cmake/VerifyArchitectureBoundaries.cmake`
  passed on a complete `git archive` of exact claim `303ba3b5`. The isolated
  tracked tree demonstrates the source boundary without traversing the ignored
  worktree. The original failing checkout result remains recorded; no unrelated
  checkout or verifier was altered to hide it.
- `otool -L` inspected the real dev executable's Qt/system dynamic linkage.
  Static dependency accounting comes from source/link ownership, not an empty
  dynamic-library scan. This is not relocated/package/native trust evidence.
- The M1 hashes for publish-release skill, both scripts and presets still
  match. Their unchanged syntax evidence is reused; no release command ran.

The maintenance test uses private disposable roots, offscreen Qt, test-enabled
composition and blocked external network. Its test-only entry disappears from
tests-disabled builds. It proves reuse of that maintenance path, not final
package installation, signing, authentication, visual/native or new-control
acceptance. No M2 human or independent criterion exists; all later required
verdicts remain pending. Final documentation, board, checkpoint and no-impact
checks bind the committed allocation candidate before handoff.

Allocation candidate: `fa93f89a22a189042486ccfc757e6759b982c072`.
At that exact commit, documentation reports `DOCUMENTATION_OK`, scoped board
validation reports 957 cards/valid, and target-only discovery finds the same
twelve-stage plan. The product/build/test/release-tool/skill diff against M2
entry is empty. The architecture gate passes a fresh complete `git archive`
of this candidate, not only the earlier claim. Local receipts are
`build/rfc0014-run/m2-{documentation,board,discovery,no-impact,architecture}-evidence.json`.
The archive path and complete CMake command are captured in the architecture
receipt; the disposable copies are removed after verification.

All 64 documentation-verifier tests passed in 2.019 seconds during candidate
preparation; expected hostile-fixture `ERROR` output is covered by passing
assertions, and the suite exited zero. Script/test inputs are unchanged at the
candidate. Staged whitespace checks passed. A direct acceptance-section
comparison with the immutable RFC base confirms all nine canonical bullets
remain byte-for-byte unchanged. Self-review checked the allocation against
current source and the live criteria, including the unresolved IPC issue and
absence of native/signing/privacy verdicts. Only BPR-0952/M2 is accepted by
these records; M3–M12 and predecessor closeout remain open.

BPR-0952's guarded finish is
`57b3bce38495c544dba36b8aa279c5a7c33b068c`. BPR-0953 is promoted to Ready
for a fresh M3 owner; this child stops at the M2 boundary. The
[M2 handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m2.json)
records the exact candidate/commits, paths, checks, pending work and next action.
Both disposable source archives were removed after their checks; Git can
recreate them. No unrelated worktree, product input or review candidate changed.
No task-owned process/server is handed off. The final observation/check receipts
are local to `build/rfc0014-run/m2-observation.json` and `m2-final-check.json`
and bind the final bookkeeping HEAD without claiming new product acceptance.

## Milestone 3: deterministic functional core

Assignment `m3-deterministic-functional-core`, child `/root/rfc0014_m3`, mandate
r3, run `rfc0014-20260920`, started from clean main
`9ceb9b0af2eff3c015203a48ff7dd12cc1c55b18`. Guarded claim
`d6a39867839283b12e0cdb597e605018041814fd` and the
[M3 binding](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m3.json)
retain accepted M1/M2, all BPR-0953 criteria and both M3 exits.

The [maintenance contract](../architecture/release-maintenance.md) owns the
typed state/selection/lifecycle and exact host obligations. New core code admits
only fixed metadata fields, bounds preview to 1,024 records/256 KiB and purge
inventory to 4,096 exact entries, requires exact preview/inventory confirmation,
and rejects stale/duplicate/mismatched or over-limit work. State is trivially
copyable and at most 512 bytes; no content bytes or effect dependencies enter it.
Native capability checks, actual digest/byte equality, redaction, recovery,
producer draining and UI confirmation remain M4–M6 work. These limits are
conservative engineering ceilings, not latency or native qualification claims.

Core retention, appearance/theme migration semantics, profile identity and
updater policy are reused unchanged. The contract maps each to exact source
and direct test selectors; migration transactions and credential sanitization
remain their current host/adapter owners. No generic migration engine, second
logger, broad RFC-0027 activation or alternate release pipeline is introduced.

Early checks: `cmake --preset core-only` selected no Qt/provider; its dependency
audit names Threads and test Python, with pinned SQLite acquired for host tests.
The explicit epoch/appearance/profile/updater targets built and all four core
courses passed (1.08 seconds). The first new test build caught four ignored
`nodiscard` results in test code; each became an asserted transition outcome.
The new public core course then passed. Self-review added exact cancellation
target binding and preserved recovery after retiring irreversible deletion;
their direct counterexamples pass. An initial tidy invocation used positional
paths; the helper required `--paths`, and the corrected invocation passed.
The code-simplifier pass kept one shared command-admission function, typed
fixed-size values, early refusal helpers and no dynamic effect list. Exact
cancel-target and irreversible-retirement counterexamples were retained as
direct tests. A later status correction clears the recovery reason after valid
host readmission; its asserted result is part of the final candidate.

Source candidate: `7445b09efd0f831ae0838f1eee3c430cdc248ebc`.

| Check at the candidate | Result / evidence limit |
|---|---|
| `cmake --build --preset core-only --target test_release_maintenance_core test_better_pull_review_epoch_core_tests test_better_pull_review_appearance_core_tests test_better_pull_review_theme_exchange_core_tests test_better_pull_review_core_profile_window_tests test_release_update_core -j 4` | Pass; Qt/provider-free core build, 0.729 seconds |
| `ctest --test-dir build/core-only --output-on-failure -R '^(core_release_maintenance\|core_epoch_policy\|core_appearance_policy\|core_theme_exchange\|core_profile_window\|core_release_update\|core_dependency_boundary\|core_boundary_contract)$' -j 4` | 8/8 pass, 1.34 seconds; covers new policy and reused semantics |
| Complete candidate archive followed by `cmake -DBPR_ROOT=/Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0014-run/m3-candidate.a8KTWz -P /Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0014-run/m3-candidate.a8KTWz/cmake/VerifyArchitectureBoundaries.cmake` | Pass, 5.513 seconds; all tracked source included, unrelated ignored worktree excluded without altering it |
| `architecture_boundary_contract` in the earlier nine-check core-only pass | Pass, 90.28 seconds; checker, fixture and relevant CMake inputs unchanged at candidate, so reused |
| `/usr/bin/clang++ -std=c++23 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I src/core/include src/core/src/release_maintenance.cpp tests/core/release_maintenance_tests.cpp -o build/rfc0014-run/m3-policy-asan`, then run with `ASAN_OPTIONS=abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1` | Both pass; `release-maintenance-tests:pass`, no sanitizer findings; only the new pure core/test translation units, not package-wide sanitizer acceptance |
| `python3 _coding-standard/scripts/format_check.py --root . src/core/include/bpr/core/release_maintenance.hpp src/core/src/release_maintenance.cpp tests/core/release_maintenance_tests.cpp` | Pass, three touched files, clang-format 23.1.1 |
| `python3 _coding-standard/scripts/tidy.py --root . --build-dir build/core-only --warnings-as-errors --paths src/core/src/release_maintenance.cpp tests/core/release_maintenance_tests.cpp` | Pass, two translation units, clang-tidy 23.1.1; one non-user-code warning suppressed by the existing tooling |
| Documentation gate, scoped board validation, target-only plan discovery, `git diff --check` | Pass; `DOCUMENTATION_OK`, 957 cards/valid, unchanged twelve-stage plan |
| Scoped diff against M3 entry; verbatim RFC acceptance-section comparison with immutable base | Existing semantic owners, application/adapters/Qt/composition, release tools/skill/presets unchanged; all nine canonical RFC bullets unchanged |

The native compiler is Apple clang 21.0.0 on arm64 macOS; Python is 3.14.7.
`otool -L build/core-only/tests/core/release_maintenance_core` lists only
`libc++` and `libSystem`. The new course tests safe default capabilities, all
32 diagnostic/category bits, record/byte/entry bounds, exact snapshot and
inventory fields, stale completions on every identity axis, irreversible
failure, invalidation, shutdown and counter exhaustion. Local receipts are
`build/rfc0014-run/m3-{build,core-tests,architecture,sanitizer-build,sanitizers,format,tidy,documentation,board,discovery}-evidence.json`.

M3 accepts BPR-0953's three criteria and both milestone exits only. The public
surface exercised here is the deterministic library API; real UI/host/file
effects are intentionally M4–M6 obligations. Actual classification/redaction,
byte/digest fidelity, private-root/vault cleanup, migration recovery, canary/crash
inspection and every native/package/trust criterion remain pending. There is
no attended candidate, independent verdict or release/publication command.
The existing MASK preflight discrepancy and `host_load: not_verified` remain
recorded and are not acceptance evidence.

Guarded finish `b5a325423e1f11813fcc7cf9a6afd2b899ee350a` accepts BPR-0953.
BPR-0954 is Ready for the coordinator's fresh M4 owner. The
[durable M3 handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m3.json)
preserves candidate/commit identities, paths, verification, command ownership
and remaining work. The task-owned source archive, sanitizer executable and
its debug-symbol directory were removed after verification; Git/compiler can
recreate them. Runtime receipts and normal core-only build output remain
ignored. No child, worktree, server or native app is handed off. Final local
observation/check receipts bind the final bookkeeping HEAD; source acceptance
stays bound to `7445b09e`.

## Milestone 4: imperative application host

Assignment `m4-imperative-application-host`, sole non-delegating child
`/root/rfc0014_m4`, mandate r3, coordinator-implementer recipe revision 2, run
`rfc0014-20260920`, started from clean main
`a9146cac58e584b3c7f225eabe8b31d39eed6bf7`. Guarded claim
`a578f8ec07fb1dd1eb98192e61ab8473152b023b` and the
[M4 binding](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m4.json)
retain accepted M1–M3, all BPR-0954 criteria and both exits. No agents were
delegated and no other main writer was observed.

The [implemented M4 contract](../architecture/release-maintenance.md) records
the one-job host, exact native preparation/output capabilities, bounded metadata
classifier, lifecycle barrier, credential-owner cleanup and durable inventory
recovery. Both POSIX and Windows have native mechanisms; Windows uses the
existing ACL/handle admission extracted into a shared native header, genuine
runtime-lease validation and the existing Quill logger. The core is unchanged.
No Qt widget performs effects and no maintenance port can mutate GitHub or a
developer checkout. Raw diagnostic prose is never an export payload.

The r3 prescription correction is explicit in the contract: whole-profile purge
journals the closed manifest-identified cache inventory together with the
catalog being removed. Ordinary epoch retention/cache deletion retains its
existing transactions and recovery. Unknown objects and unfinished original-
owner cleanup still refuse. Exact private authority, bounded work, credential-
before-metadata ordering and restart safety are preserved. Native uninstall,
shared registry/all-profiles cleanup and root/lock residuals are not renamed as
complete uninstall. No criterion, plan selector, platform or release permission
changed. All nine canonical RFC acceptance bullets remain immutable.

The shortest existing public composition was exercised early through
`application_epoch_maintenance_public_path`: seed, prune, restart/cache delete,
restart/verify in isolated roots with external network blocked; pass in 47.91
seconds. New direct courses exercise the public host with manual lanes and the
real native file capability. The logger/profile courses use actual Quill and
development composition. They are not tests-disabled package or human evidence.

Self-review and simplification retained one shared completion/failure path and
native cleanup helpers, separated policy from mechanisms, and removed duplicated
Windows admission primitives. Direct counterexamples caught and corrected a
native refusal whose reason was not a legal core operational terminal. The host
now maps that effect refusal to `selection_changed`, consumes the terminal and
drains the job. The immutable journal now has a separate digest-bound credential
receipt: torn receipts retry exact idempotent keys, while foreign receipts,
corrupt inventories and replaced objects refuse. This does not weaken the core
commit boundary or invent a second retention policy.

Initial preparation checks caught local capture/suffix compile errors, an
incorrect error-enum alias during extraction, formatting/bracing and exception-
type tidy findings; each was corrected before final verification. The first
MinGW check also needed the project's existing missing-field-initializer warning
exception. No compile/test failure is being presented as a passing platform
qualification. Final exact-candidate results and acceptance follow below.

Source candidate: `19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8`.

| Exact-candidate check | Result / evidence limit |
|---|---|
| Core-only build of new host/native tests and existing store/vault courses | Pass, 0.505 seconds; no Qt/provider needed by the new application API |
| `ctest --test-dir build/core-only --output-on-failure -R '^(application_release_maintenance\|sqlite_release_maintenance_files\|sqlite_appearance_settings_storage\|sqlite_workbench_settings_storage\|sqlite_device_binding_storage\|sqlite_profile_runtime_lease\|sqlite_epoch_catalog_storage_contract\|sqlite_epoch_catalog_behavior_contract\|sqlite_review_ledger_storage\|sqlite_review_mutation_recovery_storage\|sqlite_queue_enrollment_storage\|sqlite_window_review_restoration_storage\|application_github_credential_vault)$' -j4` | 13/13 pass, 43.33 seconds; actual macOS private roots and SQLite plus manual/fake vault boundaries |
| Dev app/logger/profile-target build; `ctest --test-dir build/dev --output-on-failure -R '^(quill_logging\|quill_logging_backpressure\|application_logging\|application_profile_management_public_path\|application_profile_multi_window_public_path\|application_epoch_maintenance_public_path)$' -j2` | 6/6 pass, 81.22 seconds; retention/restart course 50.80 seconds; tests-enabled real composition, not a delivered package |
| `x86_64-w64-mingw32-g++ -std=c++23 -Wall -Wextra -Werror -Wno-missing-field-initializers -Wconversion -Wsign-conversion -Wshadow=local -fmax-errors=5 -fsyntax-only` with project/SQLite/Quill includes and eight changed host/native/logging/catalog/test TUs | Pass, 8.734 seconds, GCC 16.2.0; Windows source/type evidence only, not MSVC linking or native execution |
| `/usr/bin/clang++ -std=c++23 -g -fsanitize=address,undefined -fno-omit-frame-pointer` for new host/parser/credential/native and direct-test TUs, linked to existing static dependencies; both binaries with `ASAN_OPTIONS=abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1` | Compile and run pass; both course pass markers, no findings. Existing static dependencies are not sanitizer-instrumented; this is not package-wide release-policy acceptance |
| Complete candidate `git archive`; `cmake -DBPR_ROOT=/Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0014-run/m4-candidate.jloLnD -P /Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0014-run/m4-candidate.jloLnD/cmake/VerifyArchitectureBoundaries.cmake` | Pass, 6.435 seconds; avoids the unrelated ignored nested worktree already diagnosed in M2 without changing it or the checker |
| Touched-file format check; focused tidy in core-only and dev compilation databases | Pass, 20 files and nine TUs; clang-format/clang-tidy 23.1.1, existing tooling excludes non-user-code warnings |
| `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py`; scoped board validation, plan discovery and whitespace | `DOCUMENTATION_OK`; 64/64 pass (2.156 seconds; hostile-fixture `ERROR` output is expected); 957 cards valid; same twelve-stage plan; clean whitespace |
| Scoped no-impact diff from M4 entry; byte comparison of canonical RFC acceptance against immutable base | Core, Qt/app, GitHub/Git/MCP and release scripts/skill/presets unchanged; all nine canonical bullets identical |

The exact command arrays/results are in
`build/rfc0014-run/m4-{core-build,core-tests,dev-build,public-tests,windows-source,sanitizer-native-build,sanitizer-native,sanitizer-host-build,sanitizer-host,architecture,format,tidy,adapter-tidy,documentation,documentation-tests,board,discovery,reuse-no-impact,acceptance-fidelity}-evidence.json`.
The durable handoff records reproducible commands and candidate identity; build
receipts are local ignored verification output, not shipped artifacts.

All three BPR-0954 criteria and both M4 exits are accepted for the implemented
host/adapters and direct disposable-root course. Windows source mechanisms and
test registration are complete rather than waived; actual Windows/MSVC, Linux,
OS-vault and native installer/removal qualification stays explicitly pending
with M8. Qt controls/rendered acceptance and real lifecycle composition belong
to M5/M6. Package trust, native crash artifacts, complete dependency/licensing,
hardening/security policy, G9's local activation IPC issue and canonical
predecessors remain open. No human/independent verdict or release command is
claimed. BPR-0885/BPR-0891 remain unclaimed despite overlapping source repairs.
The complete publish-release skill was read; existing release paths are
unchanged and were not run. MASK drift/incomplete and `host_load: not_verified`
remain recorded discrepancies, not acceptance evidence.

Guarded finish `2475b4ba5af545236ac0f68f1738bc76f09685a3` accepts BPR-0954.
BPR-0955 is Ready for a fresh M5 implementer; this child stops at M4. The
[durable M4 handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m4.json)
contains exact assignment/candidate/commit identities, paths, commands, pending
obligations and lifecycle instructions. The task-owned full source archive/tar
and both sanitizer executables/debug directories were removed after verification;
they are reproducible. Normal build output and ignored receipts remain.
All task commands and native test processes completed; no server/app is handed
off. Separately advancing worktrees, including new RFC-0037 and RFC-0018–0026
checkouts, were observed and left untouched; main remained the expected task
revision. Final local observation/check receipts bind the bookkeeping HEAD,
while product acceptance remains bound to `19a3dfd7`.

## Milestone 5: Qt presentation adapter

Assignment `m5-qt-presentation-adapter`, sole non-delegating child
`/root/rfc0014_m5`, mandate r3 and coordinator-implementer recipe revision 2,
run `rfc0014-20260920`, started from clean main
`d2539a90b387eedd119748d019a1490633227158`. Guarded claim
`46a32a100c2cd348fb07014dcc1357b8e8a51047` and the
[M5 binding](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m5.json)
retain accepted M1–M4, all BPR-0955 criteria and both unchanged exits. No agents
were delegated. The shortest existing Settings composition passed early
(`qt_settings_window`, 1.02 seconds).

Source candidate `a34fc07ffe49e21d03edce174bd28a0d5fb37a8a` implements the
[M5 contract](../architecture/release-maintenance.md#m5-settings-presentation-and-bounded-composition).
Final product candidate `13a7d0522137eeb4ab147ff9d563de62aa978035` changes
only session generation from Qt's `system()` accessor to `securelySeeded()`.
The former tripped the existing architecture check's lexical shell-process
ban. The checker was not loosened; secure random identity remains effect-lane
work. Corrected composition build, public path, Cocoa execution, tidy, format
and complete final-source architecture scan pass. Unchanged checks are reused
from `a34fc07f`, not relabelled as newly run at the final revision.

Real Settings admits diagnostics independently of sign-in, previews frozen
metadata and saves exactly those bytes to an admitted new local file. Non-ASCII
filename and existing-destination negative controls exercise native effects.
The panel follows the profile Settings shell across invoking windows. Controlled
host/storage/lifecycle courses exercise clear/purge presentation, exact targets,
default/Escape cancellation, stale dialogs, stopped runtime and partial cleanup.
Live `Clear`/`Purge` remain false until M6 supplies actual window/producer
retirement, recovery, store/logger closure and retained vault/lease lifetimes.
This is the authored boundary, not production purge acceptance from widget tests.

| Check | Exact revision and result |
|---|---|
| Dev app, panel, public path, Settings, host and multi-window target build | `a34fc07f`: pass, 17.488s; final app/public-path rebuild at `13a7d052`: pass, 2.659s |
| `ctest --test-dir build/dev --output-on-failure -R '^(qt_release_maintenance_scale_(100\|125\|200)\|application_release_maintenance_public_path\|application_release_maintenance\|qt_settings_window\|application_profile_multi_window_public_path)$'` | `a34fc07f`: 7/7 pass, 82.834s, network-isolated; includes existing selected-review multi-window regression |
| `ctest --test-dir build/dev --output-on-failure -R '^application_release_maintenance_public_path$'` | `13a7d052`: 1/1 pass, 1.52s; actual private-root preview/export, no model reset, Settings owner transfer and shutdown |
| Core-only host build and `application_release_maintenance` | `a34fc07f`: pass, 2.132s and 0.292s; Qt/provider-free presentation API, inputs unchanged in final candidate |
| Cocoa public path through `qualification_guard.py` and network-denying runner | Final `13a7d052`: pass; native macOS widgets/effects, deterministic Qt filename field, not attended native-picker/VoiceOver evidence. Four selected live MASK/context files unchanged; cooperative guard is not a filesystem sandbox. |
| Complete final `git archive`; `cmake -DBPR_ROOT=/private/tmp/bpr0014-m5-architecture-final.et7Lad -P /private/tmp/bpr0014-m5-architecture-final.et7Lad/cmake/VerifyArchitectureBoundaries.cmake` | `13a7d052`: pass, 5.669s; excludes unrelated ignored nested worktree without changing it or the checker |
| Changed-file format and production tidy | 11 files/three production TUs pass at `a34fc07f`; changed composition TU rechecked at `13a7d052`; clang-format/clang-tidy 23.1.1 |
| New-test tidy | Full checks pass with a test-source line filter. Unfiltered tidy finds inherited `home_view_state.hpp:35` signed-bitwise warnings, also reproduced by untouched `settings_window_tests.cpp`; header unchanged, not silently fixed or claimed clean. |
| Source reuse/fidelity | Core, native file/logging/provider/MCP owners, epoch host/actor/controls, diff/alignment and release scripts/skill/presets unchanged from entry; all nine canonical bullets byte-identical to immutable base |
| Documentation and verifier suite | Candidate gate `DOCUMENTATION_OK`; 64/64 tests pass, 2.3s, expected hostile-fixture error text; final bookkeeping gate recorded in the handoff |

Command arrays are retained in `build/rfc0014-run/m5-*-evidence.json`; final
correction checks use `m5-final-*-evidence.json`. The failed first archive scan
remains `m5-architecture-evidence.json`. Three-scale preview/confirmation/recovery
PNGs are under `build/dev/tests/adapters_qt/`; actual Settings is under
`build/dev/tests/end_to_end/`; the final Cocoa view is copied to
`build/rfc0014-run/m5-public-cocoa.png`. These ignored artifacts are reproducible
test evidence, not human verdicts. Inspection confirmed literal text, visible
Cancel default and bounded scrolling.

Self-review used code-simplifier on the changed surface: removed an unused
factory/button cache, retained one projection/dispatch boundary, preserved
focus by not toggling equivalent commands, and corrected shared-Settings page
ownership. Preparation failures included a capture-name compile error, a
mistyped test target, keyboard animation timing, generic refusal wording,
Cocoa's test-driver filename selection and the lexical architecture hit. Each
was diagnosed and its relevant course rerun. The Cocoa driver now checks the
real filename field before acceptance; its early misnamed export stayed inside
the disposable root and was removed by the runner. No live credential/content,
release command, publication, independent or human verdict was used.

### Observed remote qualification routes

Read-only SSH `aito-main-win` reached `JaegAW`. These are discovery facts, not
M5 candidate execution, clean native acceptance or permission for another writer.
Neither checkout was changed, built, fetched or launched; both pre-existing
`.mask.lock.json` modifications were preserved.

- Windows: `C:/code/git/bpr-windows`, station `stn-a5155427` / `bpr-win`,
  `windows-native`, `windows-lane`, HEAD
  `b9cf4edb316dceaf8c7ea75f4c7321ce353651e8`; Windows NT 10.0.26200.0.
  Managed Remote task `bpr-win` was visible as `notLoaded`, thread
  `01a0ad7c-172d-7bd1-877f-b92372b3f09b`, host
  `remote-control:env_e_6a925c2980dc832da3493cea2cb4d944`.
  Python/CMake resolve in SSH; Ninja/`cl` did not. The MASK skill link exists
  but its helper path did not resolve; writer-lease status is unverified, not
  free. The next owner must reconcile deployment/lease before writing.
- Linux: Ubuntu WSL2 on JaegAW, `/home/mmarcin/bpr-linux` on ext4,
  station `stn-3f7fcc92` / `bpr-linux`, `linux-lane`, HEAD
  `1aabc2f3bbd619a8e26938b8c2e909e4a7058f3e`; kernel
  `6.18.33.2-microsoft-standard-WSL2 x86_64`. CMake/Ninja/clang++ resolve;
  `DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0` exist (not desktop acceptance).
  Installed helper reports `station-writer: free`. Recorded Codex thread
  `01a0ad7c-5112-7230-ace9-1cf4715ea864` alone does not prove a visible managed task.

Carry these routes to M8. Neither station proves the clean Windows 10/11,
Ubuntu/RHEL package matrix; Intel Mac remains external. MASK drift/incomplete
and `host_load: not_verified` remain recorded. No skill repair or remote launch
was performed. The final handoff retains candidate identities, commands and
pending later obligations; this child stops at M5.

Guarded finish `42c7ac49b41fdc6b11bf072fe8a055c86ac2612e` accepts BPR-0955.
BPR-0956 is Ready for a fresh M6 owner. The
[durable M5 handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m5.json)
contains exact assignment, candidate/commit identities, commands, rendered
hashes, station observations, current ownership and the next action. All task
commands and native test processes finished. Exact task-owned archive trees,
tar files and qualification directory were removed after preserving the Cocoa
image; Git and the tests can recreate them. Ignored receipts, screenshots and
normal build output remain. No server, worktree, child or protected candidate
is handed off. Final local observation/check receipts bind the bookkeeping HEAD;
product acceptance remains bound to `13a7d052` and the explicitly reused
unchanged evidence at `a34fc07f`.

## Milestone 6 composition and public path

Assignment `m6-composition-public-path`, child `/root/rfc0014_m6`, mandate r3,
coordinator-implementer revision 2, began from clean
`b908246e72bb2a9de75ff9c913af6ac321354311`. Claim `b8515c7c` precedes edits.
Implementation `65dd7dd4add669606f67f39da05e6b420effec77` composes the retained-
lease desktop retirement/recovery barrier and actual package paths. Correction
`da12f5568f8ab207d8a75451cda9b3de1a49df63` separates original Qt producer
verification assertions from relocated/sealed file hashes. The
[M6 binding](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m6.json)
preserves every assigned criterion and exit. M6 is now accepted after the user
unlocked the Mac and the exact packaged native UI replay passed. The temporary
blocked checkpoint `c4d64313` and resume `02325a49` changed bookkeeping only;
package/source evidence was reused without rebuilding. BPR-0957 is Ready for M7.

### Implemented scope and direct checks

The [retirement contract](../architecture/release-maintenance.md#m6-retained-lease-maintenance-composition)
and [operator guide](../development/release-packages.md) describe the actual
owners. Core/host/Qt/provider policy remains unchanged: the desktop refuses
ingress, retires every affected window through the existing registry lane,
drains original producers/recovery owners, destroys the runtime/stores/logger,
then acknowledges the maintenance barrier while a separate vault worker and
the original root lease remain held. Startup recovers before opening producers.
Real Git inventory testing found missing isolation files and an incorrect
decimal epoch-ref assumption; the fixed inventory accepts only exact known
files/empty shells and the producer's fixed 16-hex ref grammar.

Windows/Linux application installation and native closure rules are new;
macOS extends the existing release script and preset. Production MCP now uses
its emitted filename, platform suffix and actual app-directory build/install
edge. Qt discovery is restricted before QApplication. Source/notices account
for static dependencies and mixed-license embedded syntax/theme data, not just
Qt. First-party and aggregate syntax-data distribution terms remain owner/legal
gates; `NOASSERTION` is explicit, not a licensing waiver. Source-only publication
changes require the new SBOM assets without running publication.

| Direct check | Candidate and result |
|---|---|
| `cmake --build --preset dev --target test_release_maintenance_core test_release_maintenance_public_path test_profile_startup_public_path test_profile_multi_window_public_path -j4`; focused CTest selector for maintenance core/host/files/Qt scales 100/125/200, startup, multi-window and package contract | Source now committed at `65dd7dd4`: build pass, 10/10 pass in 98.05s; latest source files rebuilt after range formatting |
| `ctest --test-dir build/dev --output-on-failure -R '^application_release_maintenance_public_path$'` | `65dd7dd4`: pass 16.385s, actual PR/file selection plus two-window retirement, expired original owner, held lease, cancel/no restart, stale inventory refusal, clear, populated purge and corrupt-receipt startup refusal |
| `cmake --preset core-only`; build `test_release_maintenance_core test_release_maintenance_host`; CTest core/host maintenance | `65dd7dd4`: pass 2/2, 0.147s; no new core SDK or Qt/provider requirement |
| Complete committed `git archive`, then `cmake -DBPR_ROOT=/private/tmp/bpr0014-m6-architecture.lHVEbA -P /private/tmp/bpr0014-m6-architecture.lHVEbA/cmake/VerifyArchitectureBoundaries.cmake` | `65dd7dd4`: pass; preserves unrelated ignored nested worktree and checker |
| `python3 tests/scripts/package_runtime_tests.py` | `da12f556`: 11/11 pass; pin/allowlist/baseline/refusal, Linux provider-before-stage/SONAME binding and corrected SPDX assertions |
| Focused formatting and strict tidy | Nine touched C++ files format pass; new maintenance/vault TUs strict tidy pass. Installed runtime strict tidy with distribution macro and single-architecture dev compile database passes; universal compile database itself is unsupported by clang-tidy's one-job requirement |
| Documentation gate and verifier regression | `65dd7dd4`: pass; verifier regression 64/64. Final bookkeeping reruns are in the handoff |
| `git diff --exit-code b908246e HEAD -- src/core src/application src/adapters/qt src/adapters/git src/adapters/github src/adapters/agent src/adapters/mcp src/adapters/update .codex/skills/publish-release/SKILL.md` | Pass: selected comparison/alignment/DiffLinkMap, retention and existing provider/updater owners are unchanged |

Initial integration selected an unbuilt core test executable; building the
correct target repaired the harness error and all ten checks passed. The first
temporary external fixture used macOS's `/private/tmp` alias, which Qt spells
`/tmp`; existing canonical-root admission correctly refused it. Moving only the
disposable fixture to a canonical user directory fixed that course, without
weakening product containment. Broad `PUBLIC_PATH_OK`/native-vault marker absence
was an invalid fixture check: these are existing unguarded production self-test
status strings. Actual tests-only translation units and access symbols/markers
are absent. No claim that every production self-test string is removed is made.

### Exact clean local artifact

`tools/release.sh --version 0.1.3 --prepare-only --skip-tests --output-dir
build/releases/m6-da12f556` passes in 18.55s from clean `da12f556`.
It reuses the actual universal tests-disabled deployment, verifies both Mach-O
slices against macOS 13 and the restricted dependency paths, includes notices
and complete pinned source, seals offline ad-hoc, and passes relocated launch.
It does not perform Developer ID signing, Keychain discovery/change, Apple
submission, tag/push/upload or publication. No such authorization is inferred.

| Artifact under `build/releases/m6-da12f556/` | SHA-256 |
|---|---|
| `BetterPullReview.app/Contents/MacOS/BetterPullReview` | `d31b371b1aad6caef369bdf60212b0a156b913ee7834d1f88b7424c8d8221852` |
| `BetterPullReview-0.1.3-macos-universal-local.zip` | `c0fe65a9978b9786f40953349806492afe7c3cce24e4de659901d92586c1306c` |
| `BetterPullReview-0.1.3-macos-universal-local.spdx.json` | `4fb15f0fd7ec256355fa39b6b75b096812512e4527a1e14a74eb6642360aa7fe` |
| `BetterPullReview.app/Contents/Resources/ThirdParty/corresponding-source.zip` | `3b0c46045b6f4c070ab59f44124b932ce2c85dd850a480ffd0794513082b6c2e` |

All 43 installed file hashes match; SPDX has 71 packages and 144 relationships,
clean source identity and no stale producer verification code. The complete
43-file installed inventory is identical to the clean `65dd7dd4` package; only
the external SPDX source/assertion metadata changed. ZIP/SBOM checksums and the
existing binary dependency scan pass. Both MCP helper slices link only system
libc++/libSystem, not Qt. Tests-disabled graph and selected fixture byte/symbol
checks pass; a forbidden platform-plugin-path CLI request fails before startup
with `INSTALLED_RUNTIME_REFUSED`.

The existing `verify_runtime_surface.py` forces the offscreen Qt plugin. That
plugin is intentionally not shipped, so its attempted package execution fails
at Qt initialization and is not a passing runtime audit. M7/M9 must select the
native platform for that course without expanding the shipped plugin closure.

### Public UI and remaining acceptance

An earlier working package at `build/releases/m6-working-v3/` reached the real
native window through the existing external local-only `gh` fixture, not an
in-product fixture hook. Native UI actions selected GitHub CLI, its read-only
consent, PR 17 and Start review at `review.cpp`. AX and screenshot showed six
exact earlier values 11–16, later values 21–26, one replacement, one Diff
correspondence map and one Shared comparison position scrollbar; normal Quit
completed with exit 0. App SHA was
`2983c9382ffb8c495cd367b194dc4e751f57bd0b5548142feee1da013b4ea613`.
This was a dirty working candidate and is not substituted for final acceptance.

The first final exact-byte replay was launched under the installed qualification
guard in `/Users/jaeger/bpr-m6-final.NqToKm/fixture`; CUA reported the Mac locked
and automatic unlock unavailable. No UI pass is claimed. The bounded launcher
terminated/reaped the process after 310 seconds; selected live project/MASK
files stayed hash-identical. This interruption remained pending until the user
unlocked the Mac. There is no required human verdict, attended-review server or
hidden approval request at M6.

On resumption, source `da12f556` and executable SHA `d31b371b…` were unchanged,
and no old fixture process remained. The same guarded external fixture launched
the final `build/releases/m6-da12f556/BetterPullReview.app` as PID 99461. CUA
selected GitHub CLI, accepted only the closed fixture's read-only consent, opened
PR 17 and chose Start review at `review.cpp`. Native AX and screenshot showed
`Diff ready — 1 semantic changes`, exact earlier values 11–16/later 21–26, one
selected replacement in the Diff correspondence map, and one Shared comparison
position scrollbar. The product path selected the same single alignment and
DiffLinkMap ownership verified by the source/public test; no fixture-specific
application argument or alternative UI was used. Normal Quit exited 0 and the
guard passed with both live configuration hashes unchanged. This final artifact
evidence completes AC3 and M6 exit 2, independently of the earlier dirty package.

The post-Quit CUA snapshot unexpectedly reopened the same app bare as PID 99796.
It was closed immediately without further UI interaction, and process checks
confirmed both PIDs gone. The guarded course's exit-0 result was already complete;
the extra launch is not used as fixture evidence. Future courses should verify
Quit through the launcher/process state without a post-exit app snapshot.

Read-only station discovery confirmed JaegAW, VS 18 Community and Smart App
Control state 0. The Windows installed skill link lacks a resolvable
`station_lease.py`; WiX/native toolchain activation and CRT notice are not
qualified. WSL's free guarded station is Ubuntu 26.04, not Ubuntu 24.04 or RHEL
10. Both remote checkouts have unrelated `.mask.lock.json` changes. No remote
dispatch, mutation, checkout, build or repair occurred. Their declared native
package matrix remains M8; all exact trust/vault/crash/security and distribution
approval obligations remain M9. MASK drift/incomplete and host-load
`not_verified` remain configuration observations, not silent repairs.

Guarded finish `2589f07f2897f3f19b6e408edfdd2be487892b0d` accepts BPR-0956.
The [durable M6 handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m6.json)
records the resume command, all candidate identities, ownership and pending
acceptance. No background command, child, review server or protected candidate
is handed off. Normal ignored package/build evidence remains; all exact
task-owned fixture/probe and archive directories, including the completed final
fixture, were moved to the user's Trash after their evidence was recorded and
remain recoverable. Recreate a canonical disposable external fixture for later
courses; the recorded launch command is historical evidence, not a live process.
No unrelated worktree changed
through this child's actions. Full RFC closeout and its predecessor gate remain
unchanged.

## Milestone 7 focused tests and hostile fixtures

BPR-0957 implements only M7 under mandate r3 and coordinator-implementer rev2.
Claim `988f2d34` precedes implementation `3da341d1`, native fixture eligibility
correction `b5fa5246` and readability-only audit refinement
`0b2323845a2d326da3f4b545c2a4c0f50b69d697`. The
[bound checkpoint](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m7.json)
and [handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m7.json)
bind all three card criteria and both M7 exits. No product, core/host/Qt policy,
adapter effect, package producer, release skill or updater behavior changed.

`verify_release_package.py` reuses the actual deployment manifest, dependency
pins/scanner, production symbol anchors and device-flow secret/fixture policy.
It reconciles exact installed file sets/hashes, source/version identity,
required helpers/plugins, static dependency provenance, tests-off compile graph,
native bytes/symbols and contained macOS load commands. Missing observations
fail closed. It does not infer signing trust or approve licensing.
`verify_runtime_surface.py --native-platform` uses the shipped Cocoa/Windows/XCB
plugin instead of requiring the unshipped offscreen plugin. Development's
offscreen default stays unchanged. Native loader inspection is process-scoped,
requires a packaged QtCore positive control and rejects outside origins. The
Windows TCP query now observes connected sockets as well as listeners.

The guarded package course copies the unchanged M6 app into a path with spaces
outside the checkout, launches its actual entry with a child-only disposable
home, and confirms production profile creation. It observes 15 loaded libraries,
no IP socket or child process, no canary output, and trusted loader origins even
with hostile Qt plugin environment variables. It then removes every required
helper, removes/escapes the plugin root, supplies five forbidden loader CLI
arguments, changes bundle identity and appends four fixture/source/comment/secret
markers to an actual helper. A real external RPATH is added to a copy with
`install_name_tool`. Rehashed counterfeit inventories force the semantic guards
to catch the byte/RPATH alterations themselves. Every intended refusal occurs.
Finally the actual shipped MCP helper reads exact snapshot bytes over stdio,
refuses traversal to a canary and rejects stale binding without source leakage.
The restored copy and untouched original must pass the same exact audit.

The real Settings course also seeds a private diagnostic log with source,
comment and secret bodies plus a malformed legacy line. Keyboard navigation
reaches the production preview/export and neither bodies nor markers appear.
The unchanged selected comparison, bounded projection, two-window retirement,
held lease, cancellation/no restart, stale inventory and populated purge/recovery
checks remain part of that public course. This is diagnostic privacy evidence,
not a crash-dump or actual-vault claim.

### Exact candidates and focused results

The verifier/test candidate is `0b2323845a2d326da3f4b545c2a4c0f50b69d697`.
The package is still the clean tests-disabled universal `da12f556` artifact
recorded in M6, not a relabeled M7 build. `git diff --exit-code da12f556 -- src
cmake CMakeLists.txt CMakePresets.json packaging third_party tools/release.sh
.codex/skills/publish-release/SKILL.md` passes. Its app SHA remains
`d31b371b1aad6caef369bdf60212b0a156b913ee7834d1f88b7424c8d8221852` and its
SPDX SHA remains `4fb15f0fd7ec256355fa39b6b75b096812512e4527a1e14a74eb6642360aa7fe`;
all four M6 artifact identities above remain valid. The auditor checks 43 files,
13 native binaries, 34 production anchors and 227 first-party compile units.

| Direct check | Exact evidence and result |
|---|---|
| `cmake --build --preset dev --target test_release_maintenance_public_path` | Pass; the six-line diagnostic hostile fixture addition uses the existing public course |
| Focused CTest: core/host maintenance, maintenance files, appearance/workbench stores, Qt scales 100/125/200, maintenance public path and three Python contract suites | Final `0b232384`: 12/12 in 20.801s; public path 17.03s. `build/m7-final-integration-evidence.json` records the full selector |
| Guarded `tests/end_to_end/release_package_course.py` against the exact M6 app/build/SPDX/source revision | Final `0b232384`: pass in 17.291s; all nine check groups pass, original unchanged, child processes reaped; `build/m7-final-package-evidence.json` contains the complete absolute command |
| `verify_composed_hosts.py --profile production` and `verify_rfc0016_test_inventory.py --release-binary` against the app | Pass, including all 34 production anchors and no test access/fixture/secret markers; unchanged established audit policies |
| Package/runtime verifier regression | Package 11 tests, new audit 6 tests, runtime 21 tests; mutation checks reject bad identity, inventory drift/escape, missing provenance/capability, fixture graph/bytes/symbols, unsafe library origin and empty observations |
| Focused format, Python compilation and whitespace checks | Modified C++ file passes clang-format 23.1.1; changed Python modules compile; `git diff --check` passes |
| Documentation verifier regression | 64/64 pass; hostile-fixture ERROR lines are expected refusals, suite exit 0. Documentation gate rerun for final bookkeeping |

M6's unchanged Qt/provider-free core build and tracked-source architecture scan
are reused rather than repeated. The ignored nested worktree limitation remains;
no unrelated checkout or verifier rule was removed. The simplification pass
replaced nested package-path conditionals with explicit platform/origin choices;
focused tests and the full hostile package course still pass. No abstraction,
deployment dependency or product behavior was changed.

### Actual native inspector contracts, not package qualification

The standalone `runtime_surface_native_probe.py` opens only task-owned loopback
sockets and an owned Python child. Each native run must observe its own child,
listener and established TCP connection, exclude these sockets when querying
the child's PID, read native loaded libraries and reap the child. These checks
exercise the actual native APIs that unit mocks alone could not validate.

| Station and immutable proof inputs | Observed result |
|---|---|
| macOS arm64, Python 3.14.7; probe `3da341d1` (unchanged in final candidate) | Native process/socket/library proof passes; 20 libraries; guard's live project/MASK hashes unchanged |
| JaegAW Windows 11 `10.0.26200` AMD64, Python 3.14.7; station `bpr-win` / `stn-a5155427`; checkout `b9cf4edb` unchanged | Native proof passes, 34 loaded DLLs. Exact `b5fa5246` proof archive: package tests 10 pass/1 POSIX-path skip; audit 6/6; runtime 20 pass/1 Darwin-only skip |
| JaegAW WSL Linux `6.18.33.2-microsoft-standard-WSL2` x86_64, Python 3.14.4; station `bpr-linux` / `stn-3f7fcc92`; checkout `1aabc2f3` unchanged | Native proof passes, 13 libraries. Same archive: package 11/11; audit 6/6; runtime 20 pass/1 Darwin-only skip |

The native proof used archived committed files, not dirty remote checkouts.
Proof archive SHA-256 is `765b3a13d28a3b7e3a82f8b4f165212479120aec0aa93bcb0740160e3cf1aadb`.
The final refinement only makes audit path selection explicit; the native
inspector/probe and platform eligibility inputs are unchanged. Local final
audit mutation tests and artifact course cover that refinement. Each remote
run used the installed qualification guard within the exclusive station-writer
lease and disposable roots, hashing live project config and existing dirty
MASK locks before/after. Both live files remained identical; no station source,
branch, dependency or board changed. Windows' unresolved repository skill link
was bypassed using its existing absolute MASK-installed helper, not repaired.
WSL Ubuntu 26.04 is still not an accepted Ubuntu 24.04/RHEL 10 package baseline.

Relevant corrections remain visible: the first MCP snapshot fixture placed its
file outside `snapshot/files`, so the product correctly refused it; only the
fixture layout changed. One attempted build target did not exist and was
replaced by the actual CMake target. The initial Windows run attempted a Linux
`ldd` POSIX-path fixture; its eligibility is now explicit, while Linux still
runs it. An initial MSYS-converted WSL copy failed before proof execution;
subsequent commands use encoded native PowerShell routing. No failed attempt is
counted as a passing native/package observation.

M7 has no required human/independent verdict, attended session, server or
protected candidate. It leaves M8 native install/auth/render/remove acceptance,
M9 trust/hardening/crash/licensing and M10 corporate packet obligations intact.
No signing, notarization, GitHub mutation or publication occurred. The durable
handoff records cleanup, exact commands and the next M8 action. MASK drift and
`host_load: not_verified` remain unchanged configuration observations.

Guarded finish `3a0c398eb8f1977bfb5e3035409c29ab78d1e89f` accepts BPR-0957.
BPR-0958–0963 are now Ready because their BPR-0957 dependency is satisfied;
no successor work is claimed. Both remote leases are free. The empty Mac
scratch root and the exact Windows/WSL source-probe directories have been
removed after completion; the latter contain only reproducible committed-source
copies and temporary outputs. Original candidates and ignored local evidence
archives remain. All owned commands/children are finished and no review server
is handed off. Live project/MASK hashes were rechecked after remote cleanup and
remain unchanged.

## Milestone 8 native candidates and pending acceptance

M8 is **not complete**. The fresh sole `/root/rfc0014_m8_finalize` implementer owns only
`m8-ux-ui-native-verification` under mandate r3 and coordinator-implementer rev2.
The finalization entry was clean `23ed69ca258482bcd6189d9135f8016fd3b41557`;
the preceding post-review entry was `ee2e38cfab311622ff455d878279cd72cc6a46a9`;
the initial M8 entry was `789ccc311264271086e69ddec8d9cbd07a7ec839`. RFC base remains
`d0a8a6dda005af12b3818234f4878a9f48a44146`. The
[bound checkpoint](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m8.json)
and [handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m8.json)
retain all four complete card obligations and both exits. No card or native
matrix exit is accepted in full. All three narrow human questions are accepted
for their exact candidates, satisfying the human-decision binding exit.
Independent M9/M10 work requires renewed coordinator scope, not an M8 waiver.

### Initial M8 candidate and corrections

The initial M8 product source was `6ad716350c7f2e02a1b00370ea49cd5b77ce4c15`, version
0.1.3. Native execution found and corrected five concrete problems:

- `3a2404df`: after rejecting non-ASCII/unsafe filename characters, convert each
  validated Windows `wchar_t` explicitly; MSVC `/WX` no longer rejects implicit
  narrowing in the maintenance file adapter.
- `277a2c57`: make the actual entry-point runtime header portable; restore the
  macOS desktop Quit menu when maintenance retires the last workbench while
  Settings remains alive. The public test now uses that real Quit action.
- `6c46ce2f`, `9d03edb5`, `ff5242c4`: use CPack's existing WiX patch mechanism
  to remember the per-user custom install root in HKCU and restore it before
  costing. Native uninstall removes that exact root. The final patch uses the
  real executable component and leaves CPack's existing ARP metadata intact.
- `6ad71635`: compile the MSVC test-only CRT support only with `BUILD_TESTING`.
  The installed-package audit found it in shipped helpers; it was not waived.

This preserves the core/host/Qt ownership model. Only adapter filename encoding,
Qt desktop lifetime, entry composition and package/test graph mechanics change.
The refinement pass stays within touched code. No new release implementation,
fixture shortcut, deployment service or runtime dependency was introduced.

| Exact artifact | SHA-256 |
|---|---|
| macOS executable | `5ffa68c9b90e829cd18e73511b66c94f17099be965a781392bd916d46a0afd54` |
| `BetterPullReview-0.1.3-macos-universal-local.zip` | `a6fbad8a60f8f8c27156f2964ba44af7311f77e2ef6b8d64ac20fb4045dd6cb0` |
| macOS SPDX sidecar | `0e7cdb572489b7abc5a839a517a16974b24b4c209c55a2c41dab4c14c312a8b1` |
| Windows executable | `61f4d792cc6ea8171e833e40190e1e76e5960cb7e3da6af20da5a23f1097cc73` |
| `BetterPullReview-0.1.3-windows-x64.msi` | `ee2e304b9ab84c36105b7f365c6685caf1c5222313af927693d8a620dc0ac698` |
| Windows SPDX sidecar | `af694bf4df30d22fb173c0b811a01228084d3abfd610bf974fcdf58219098be5` |

The macOS candidate is in `build/releases/m8-6ad71635`, produced through the
existing publish-release skill's offline `tools/release.sh --prepare-only`
route, tests disabled, minimum macOS 13, universal, ad-hoc sealed only. All 43
payload file hashes are identical to the native-rendered `277a2c57` package;
later changes affect Windows only. Comparing complete sorted SPDX `files`
arrays passes, and the installed copies pass the final source/SBOM audit.
Only source provenance, SPDX and archive wrapper change. This exact byte binding
justifies reuse of native rendered observations; it is not reuse by version name.

The Windows candidate is retained at
`C:/Users/mikem/bpr0014-m8-df273695/candidate-6ad71635`. The external source clone
is detached at the exact final commit. `packaging/package.py --generator WIX`
uses native MSVC 19.51.36257.0/14.51.36231, Qt 6.10.3, CMake 4.4, task-local
WiX/UI 4.0.6 and the installed MSVC redistribution notice. It is unsigned;
notice inclusion is not license/source-offer approval. M9 retains that decision.

### Native observations and limits

| Card / declared environment | Direct M8 observation | Required pending work |
|---|---|---|
| BPR-0958 / macOS 13+ arm64 | macOS 26.6.2 (25G83), Retina 3456×2234; initial exact package in isolated HOME, closed CLI auth fixture, selected-file comparison, keyboard/map, diagnostics, retirement/clear, normal Quit and recoverable bundle removal; both original questions and exact revised recovery question accepted | Clean disposable OS/account with default Keychain, authentication, credential continuity/deletion, full purge/residuals and complete native qualification of the revised package |
| BPR-0959 / native macOS Intel | No eligible native Intel station supplied | All three criteria; arm64 universal slices or Rosetta are not native Intel evidence |
| BPR-0960 / Windows 10/11 x64 | Actual Windows 11 Home 10.0.26200 AMD64; native build, MSI custom-root install/uninstall, inventory/graph/dependencies/producer-map checks | Non-elevated clean desktop, Windows 10, native auth/rendered fixture/vault/full purge/residuals and full stripped-PE audit |
| BPR-0961 / Ubuntu 24.04 and RHEL 10 x64 | Available WSL is Ubuntu 26.04, kernel 6.18.33.2; no declared baseline found | Both declared clean native DEB/RPM baselines and all three criteria; no substitute baseline accepted |

On macOS the existing closed read-only CLI fixture provided only synthetic
`octo-reviewer`, `octo-org/review-repository` PR 17 and local Git objects. No
live GitHub authentication, mutation or private repository content was used.
Native AX/screenshot observations showed exact earlier values 11–16 and later
21–26 in `review.cpp`, one replacement correspondence map and one shared
comparison position. Return on the map, then Tab, reached later source focus.
Settings preview showed 57 metadata records/2677 bytes without free text,
paths, identity, source, comments or credential bodies. The confirmation's
default Cancel, retirement with Settings surviving, and truthful two-entry
diagnostic clear were observed. Command-Q then exited normally; the launcher
reaped the process and the qualification guard preserved live configuration.

The first native maintenance course exposed the lost Quit menu and timed out;
a secondary queued-activation process was not counted as a full course. The
fixed candidate's rendered course and regression both pass. One 360-second
launcher request was refused before launch; the valid bound is 300 seconds.
These failures remain recorded, not relabeled as passing observations.

The isolated HOME has no default Keychain (`security default-keychain -d user`
refuses it), so the actual vault capability is absent and full-profile purge
correctly remains unavailable. No default Keychain was fabricated and no real
credentials were touched. Bundle removal preserved every profile/cache file
hash; the previously cleared diagnostic file remained absent. This proves only
the observed uninstall-retention portion, not complete AC3.

Windows MSI product `{67A42F3D-0F59-437D-BAE4-34DCEAB3898F}` installed with exit
0, native `WindowsInstaller.Installer.ProductState` 5 and the correct custom
location. Uninstall returned 0, state -1 and the owned install root was absent.
The first candidate left files because the custom directory was not remembered;
the WiX correction fixes the actual cause. Native `msiexec` paths must use
backslashes; a forward-slash attempt failed with 1619 before installation.
Exact product queries replaced an inadequate registry enumeration. The SSH
session is elevated, so none of this establishes ordinary-user privilege needs
or an attended clean desktop. No Windows graphical app/profile was opened.

The full installed audit correctly rejected a test CRT source in the graph,
then, after the fix, refused `llvm-nm --demangle produced an empty symbol table`
on stripped PE images. That security inspection is still pending; the audit
was not weakened. A bounded native mechanics course independently checks all
23 payload hashes and required files, all 222 test-free first-party compile
units, 16 PE dependency inventories, byte equality with producer app/helpers,
and actual app composition through its producer linker map and `undname`.
Those passing checks are explicitly narrower than a full installed audit.

### Verification, protection and continuation

Final `6ad71635` focused CTest passes 5/5 in 22.713s: public maintenance,
native empty-desktop menu, SQLite maintenance files, package-runtime contract
and release-package-audit contract. Package Python tests pass 12/12; audit
regressions pass 6/6. Touched C++ formatting and whitespace checks pass.
The final macOS installed audit checks 43 files, 13 binaries, 227 compile units
and all 34 composition anchors; both independent fresh review copies pass.
The final Windows package command and narrower mechanics course pass, with
full-audit refusal retained. Exact commands and ignored receipts are in the
durable handoff; unchanged M7 hostile/runtime inspector evidence is not
repeated or promoted to platform acceptance.

Documentation verifier regression passes 64/64; the final documentation gate
reports `DOCUMENTATION_OK`. Board validation reports 957 cards with no errors.
The first gate ran before the new handoff file existed and correctly refused
its missing link; the completed handoff resolves it. Initial move commands used
the semantic name `blocked`; the helper requires the configured `50-blocked`
folder, and refused those attempts without writes. Corrected moves preserve
the platform metadata and name the concrete missing environments/accounts.

Both remote stations were acquired through their exclusive station-writer
lease and the qualification guard. JaegAW's live checkout remains `b9cf4edb`,
WSL's `1aabc2f3`; their existing dirty `.mask.lock.json` files remain unchanged.
Windows reused the existing pinned Qt cache read-only and used its absolute
installed MASK helper because the repository skill link is unresolved.
Task-local WiX/.NET setup created one untrusted localhost development
certificate as a first-run side effect. Its exact thumbprint and creation time
were checked, and only that task-created certificate was removed. Subsequent
commands disable that .NET first-run behavior. No user vault item was removed.

The [review manifest](reviews/rfc0014-m8-native-v001.json) supplies two separate
fresh isolated homes, absolute launch commands and one question per criterion.
`BPR-0958-native-diff-v001` asks source/map/focus usability;
`BPR-0958-maintenance-clarity-v001` asks local maintenance scope/confirmation
clarity. Session `cbd48b91000b00f1e72e1ad62c7c384c` protects both exact candidates
without overwriting previous unrelated decisions or protection. At the initial
handoff the human outcomes were pending; the post-review record below reconciles
their subsequent accepts. The coordinator receives the full token-bearing loopback URL
privately, event cursor and server ownership; no access token is committed.
Only the review server is intentionally handed off. Product/native commands
are finished and station leases are free. The next engineering round must
interpret actual decisions and reconcile remaining clean-platform evidence;
it must not close M8 or the RFC from two narrower human answers.

Guarded finish `63108c12b646205df3d3096772669dd9cffbd070` parks BPR-0958 for
human review and stores pending AC1/AC2/AC3 records with their partial evidence.
BPR-0959/0960/0961 are Blocked with their concrete environment/account reasons.
Both remote station leases are free and final live project/MASK hashes match.
Completed Mac courses are recoverable in Trash; obsolete reproducible Windows
candidate/residual outputs were removed. Exact review and final Windows
PE-investigation resources remain explicitly owned in the handoff. Only the
loopback review server is left running for the coordinator.

### Post-review round at hand-back 454

This round keeps mandate r3 and recipe revision 2. Session
`cbd48b91000b00f1e72e1ad62c7c384c` handed back at event 454 on 2026-09-21.
Both human verdicts bind source `6ad716350c7f2e02a1b00370ea49cd5b77ce4c15`
and executable `5ffa68c9b90e829cd18e73511b66c94f17099be965a781392bd916d46a0afd54`:

| Question | Accepted decision | Exact fingerprint |
|---|---|---|
| `BPR-0958-maintenance-clarity-v001` | `75ea6265-3302-49cd-8741-c23e1c583341`, event 452, no note | `b352efe45d939c97aff4425a6919afd5f1bd20dfac2ffb7d10a89e1fe72f9274` |
| `BPR-0958-native-diff-v001` | `fcd51e30-b163-4136-9e50-923d1a13de85`, event 453 | `daf5be5cb08311e758960c67e6ecdb07071b95b4aff925b5324acbb734049e77` |

The second decision's note is preserved verbatim: “Overview page looked
completely broken when I went to the review before picking github cli, that
might have just been a fixture test issue.” Its two screenshot attachment
SHA-256 values are `ae05a5d8a55bb209ff6651cea4e889f5dfd37555c6657dbc378d147584d679e6`
and `3251586066b5ba7850486fb9f13c5bb26662ab2f7cde1b671b0345959ee596c6`.
Both original protected installations were rehashed without modification.
These accepts satisfy only their original AC2 questions, not complete AC2,
AC1/AC3, a different artifact or another platform.

The observation is a **real product defect triggered by fixture sequencing**.
Before CLI selection, the synthetic link goes through native anonymous HTTP,
not the closed CLI fixture. The original log records a repository-read 404 at
16:26:46, before the later successful CLI session. A fresh copy of the exact
original executable reproduced the empty panes with Overview selected. The
window mistook always-enabled drawer commands for review content, hiding the
Home refusal after a first failed load. `babcb3dc` changes only that Qt
destination decision: first failure/cancellation returns Home; an accepted
review, active comparison or in-progress load stays in place. No core policy,
credential authority, transport, purge or trust check changes.

The focused regression composes the actual default core drawer commands. It
fails without the fix (`cancelled progress cannot keep a loading overview
alive`) and passes with it. The final focused CTest selection
`qt_composed_loading_scale_100|application_release_maintenance_public_path`
passes 2/2 in 18.53s. The broader eight-test UI selection is **not green**:
the composed GitHub suites fail their existing CLI-card-state assertion and
the workbench-window suites fail the epoch-row width/position assertion at all
three scales. Rebuilding the two scale-100 tests against unmodified tracked
source/test files at `79696fd9` reproduces those same failures. They are
pre-existing limitations, not a waived full-suite pass. Exact logs and receipts
are listed in the handoff. Touched C++ formatting and whitespace pass.

The tests-disabled `babcb3dc` native run observed the same anonymous 404 return
to Home with visible refusal and Copy details. The Mac locked during subsequent
CLI consent; the 300-second launcher reached its shutdown timeout and reaped
the app. That partial run is not a normal-exit or complete native-course pass.
The Open bar's accepted-destination projection deliberately clears submitted
text; `80481e80` corrects the new comment/documentation wording rather than
changing that established behavior. The final rebuild's embedded source
identity changes its bytes, so the native observation is not mislabeled as
execution of the final artifact.

| Revised artifact, source `80481e80d60f739db4c7eaea5e2eb8716ace2b4d` | SHA-256 |
|---|---|
| macOS executable | `ba8facaec459aa10464fadd76396dae372349dfd89c2c4a53ddc4eda281e7618` |
| `BetterPullReview-0.1.3-macos-universal-local.zip` | `95245bd2d6122bb197dbbf290451032e85350f5508283832c63d708c2a55fb19` |
| SPDX sidecar | `7d543637ab167827b0f69efb89a701daa894d9872407c01cf87a8e90ee341d40` |

Existing `tools/release.sh --version 0.1.3 --prepare-only --skip-tests` produces
`build/releases/m8-recovery-80481e80` in 41.267s, offline/ad-hoc only. The fresh
installed review copy passes the exact final source/SBOM audit: 43 files,
13 binaries and 227 test-free first-party compile units. No signing,
notarization, Gatekeeper, license or publication approval is inferred.
Initial fixture preparation refused a missing guard target and then an existing
fixture directory; the corrected parent-root guard prepares a fresh directory
successfully and preserves both live configuration hashes.

The [new single-question manifest](reviews/rfc0014-m8-native-v002.json) prepares
`BPR-0958-initial-load-recovery-v002`, fingerprint
`ee8486b7d89634bf1a8c707190daabbe7d48a0fc6a59f8c8fb24f40bb515b824`, in the
same session at cursor 458. It asks only whether first-load failure returns to
a clear, usable Home recovery. It explicitly describes the anonymous read,
does not ask for authentication, and supplies an absolute guarded launch
command. The original decisions/installations remain unchanged and protected.
The existing server, PID 18863/port 50579, is intentionally retained for this
follow-up; the coordinator owns the watch after this writer returns. The final
artifact's native/human recovery judgment remains pending.

No remote command, station writer or new environment was used this round.
The retained Windows MSI remains at `6ad71635` and does not contain the Qt
correction; rebuild it before any new whole-candidate qualification. Intel,
ordinary-user Windows 10/11, native Ubuntu 24.04/RHEL 10 and macOS clean-account
Keychain/auth/full-purge evidence remain unavailable. M8 and both exits remain
pending; M9 was not started. All local product/build/test commands are finished.
The two completed diagnostic profiles are recoverable in
`/Users/jaeger/.Trash/bpr0014-m8-postreview-courses.Jp3GAv`; only the new protected
review profile remains in `/Users/jaeger/bpr0014-m8-postreview.Xk8OSi`.

Guarded finish `bd088eb1406b3073bc69f4cb3e97fa5c5407e4aa` returns BPR-0958
to Parked with all three complete criteria pending and scoped verdicts retained.
The documentation regressions pass 64/64 in 1.898s, the documentation gate is
`DOCUMENTATION_OK`, and board validation reports 957 cards with no errors.
Final executable and live project/MASK rehashes match their recorded identities.
The final bookkeeping HEAD is bound by the ignored checkpoint observation;
no app/helper, build/test command or remote writer is left to the coordinator.

### Finalization at hand-back 460

Fresh sole leaf `/root/rfc0014_m8_finalize` reconciled clean main
`23ed69ca258482bcd6189d9135f8016fd3b41557` under mandate r3 and
coordinator-implementer rev2. It changes acceptance bookkeeping only. The
immutable RFC base, authored grouping, requirements and product candidate remain
unchanged.

Session `cbd48b91000b00f1e72e1ad62c7c384c` records human **accept** decision
`f9c2ddba-40bd-4d4d-b8f8-295a7056db79` at event 459, saved
`2026-09-21T17:11:51.232600+00:00`, with no note. Hand-back 460 occurred at
`2026-09-21T17:11:53.400497+00:00`. The verdict applies only to
`BPR-0958-initial-load-recovery-v002`, fingerprint
`ee8486b7d89634bf1a8c707190daabbe7d48a0fc6a59f8c8fb24f40bb515b824`, source
`80481e80d60f739db4c7eaea5e2eb8716ace2b4d`, executable SHA-256
`ba8facaec459aa10464fadd76396dae372349dfd89c2c4a53ddc4eda281e7618`.
The installed executable rehash matches. Its single question asks whether
first-load failure clearly returns to usable Home; the two original accepts
retain their original source, fingerprints and limits. No verdict is copied
to another artifact or promoted to a complete native course.

The human-decision binding exit passes: all three prepared questions now have
durable, exact-candidate human decisions. BPR-0958 AC1–AC3 and the complete
native-matrix exit remain pending. BPR-0958 moves from Parked to Blocked with
`clean-macos-account-vault`; its missing inputs are a disposable clean Apple
Silicon OS/account, working native default Keychain, authorized authentication,
full profile purge and documented credential/file/cache residuals. The complete
revised package still needs its native course. Native Intel, ordinary-user
Windows 10/11 plus full stripped-PE audit, and Ubuntu 24.04/RHEL 10 remain
required exactly as above. No new evidence for those requirements was supplied.

The finalizer found the review replay still running as guard 37534, fixture
37535 and app 37536 under the manifest's 300-second bound. All three then
exited before finalization; no exit-status receipt was available to this leaf,
so this is command quiescence, not a claimed normal-exit test pass. Original
passing focused checks and package audit remain applicable because no product
or package input changed; existing broader-suite failures remain explicit.

Commit `a1a9a90a8dbc412ab25efb7b07e670212feb333d` durably captured the exact
decision and guarded blocker move before the companion stopped review server
PID 18863. Subsequent status retains cursor 460, no pending questions and no
host; the server and all three replay PIDs are absent. All prepared candidate
protections, including unrelated historical BPR-0755, remain in place. This
bookkeeping touches no protected candidate paths or resources, and no data was
removed. No task command, application, server or remote writer remains.

The documentation gate passes, documentation regressions pass 64/64 in 2.139s,
and scoped board validation reports 957 cards with no errors. Whitespace and
the no-product-change comparison against round entry pass. Final installed
executable, both original accepted executables, revised ZIP/SPDX and live
configuration hashes match the recorded identities. The checkpoint checker
returns `fresh-context` for independent M9 with quiescent ownership and
accepted M7; it checks consistency, not engineering acceptance. The ignored
`build/m8-finalize-observation.json` binds the final bookkeeping revision.

The authored M8 boundary explicitly permits independent M9/M10 after safe
release. M9 depends only on accepted `m7-focused-tests-fixtures`, and the live
board reports BPR-0962 Ready with no unmet dependencies. The coordinator may
dispatch a fresh M9 implementer while preserving all M8 blockers. This leaf
does not claim or start M9. M12 and RFC closeout continue to require accepted
M8, M9, M11 and canonical predecessors; publication remains unauthorized.

## Milestone 9 security and boundary verification

M9 was independently eligible through accepted M7, entered at
`41cd3688cfb0bab82ea8d2551dd848d2b0d9275e`, and claimed BPR-0962 in
`8d75ff697d8aae83a54a6396fa7a71a519ec56ba`. The sole non-delegating owner is
`/root/rfc0014_m9`, mandate r3, coordinator-implementer rev2. The
[checkpoint](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m9.json)
preserves both authored exits. The [release security policy](../security/release-policy.md)
defines direct gates and [sanitized scan record](rfc0014-m9-security-evidence.json)
retains every reported vulnerability and exact secret-finding disposition.

Outcome: independent engineering evidence is complete for the available scoped
candidates; **BPR-0962 and both M9 exits remain unaccepted**, blocked by failed
dependency policy and missing independent trust/native qualification and remaining
artifact-specific licensing. The exact macOS v002 owner criterion is accepted
at hand-back 470; its scoped record follows the original-round evidence below.
No Linux/Intel, ordinary-user Windows, resident-review crash or final signing
evidence is inferred. A signing run is not authorized by this milestone.

### Exact candidates and implementation

The unchanged existing release paths built producer
`9409ff618c0e8c567fe266195ec8228852b18485` with testing disabled. Later commits
only refine verifier/test/doc behavior, not the packaged product/build inputs.

| Artifact | Exact identity and scope |
|---|---|
| macOS universal local ZIP | `build/releases/m9-9409ff61/BetterPullReview-0.1.3-macos-universal-local.zip`; SHA-256 `be7ce58db455fb22980933e41267385517e6a5b3c46e186f22dd71f98f73ae3d`; executable `8bb2ca1b11f9f859d2632a01912ae66c97375192170c86714bb04bcf34718cf1`; SPDX `7ee71d3987b75ad081cc8753a64a9b57c415d0412dbafef17282751c3d37377e` |
| Windows x64 MSI | JaegAW `C:/Users/mikem/bpr0014-m8-df273695/candidate-m9-9409ff61/BetterPullReview-0.1.3-windows-x64.msi`; SHA-256 `07942671462b337c4cf23e5a0b50b625ed000e5fe276372243106c7557a6fff6`; extracted executable `51efa21653e22eb27165b8a9646d85c8ca95d13a76205316e94680f50f9818bb`; SPDX `719a97a690122fc3c17e981d333086a5c16a5d3b3e44de5eeb6e57104f78033a` |

Verifier `f72013b0d245c3ef5e18b65eba7f31f250838b24` validates all first-party
public/static PE map names, image/timestamp/base/entry/symbol addresses and
composition anchors. It explicitly handles MSVC's wrapped linker-only CFG
index records without exempting their names from fixture-seam scans. Helpers
now receive `/MAP` centrally alongside explicit ASLR/DEP/CFG link flags.
The installed Windows/Linux SPDX copy must exactly match the external copy;
the inventory cannot recursively hash itself. Qt's seven signed Windows DLLs
match the exact local producer and reconstruct their upstream pre-sign SHA1
after only the signature fields are removed in memory; native Authenticode
validates The QT Company Oy. Altered code, wrong publisher/status, mismatched
maps and malformed hardening evidence have negative controls. Whole-artifact
trust remains separate from these producer identities.

Under mandate r3 the live RFC's obsolete “no local server” prescription and
its mechanically required closeout mirror now prohibit local network servers,
with only the existing bounded owner-authenticated AF_UNIX profile activation
exception. No HTTP/TCP/UDP service, arbitrary loader, browser/Node runtime or
new network destination is added. Tests inspect named Unix endpoints, ownership,
0700/0600 modes, malformed/version/profile refusals and production handoff;
same-user observations do not replace foreign-UID/race adapter tests. Core
policy and Qt presentation ownership are unchanged. The only product-source
change is a behavior-preserving optional-return/move correction in profile
activation, following strict tidy; code-simplifier also guided reuse of stream
hashing and explicit compiler-argument checks.

### Verification and failures retained

| Direct command/course | Result and boundary |
|---|---|
| `tools/release.sh --version 0.1.3 --prepare-only --skip-tests --output-dir build/releases/m9-9409ff61` | Pass, 26.346s. Offline ad-hoc seal only; strict seal verification passes, `codesign -dv` says adhoc/no TeamIdentifier, `stapler validate` exits 65 with no ticket |
| `python3 scripts/verify_release_security.py --build build/macos-release --root build/releases/m9-9409ff61/BetterPullReview.app --sbom build/releases/m9-9409ff61/BetterPullReview-0.1.3-macos-universal-local.spdx.json --source-revision 9409ff618c0e8c567fe266195ec8228852b18485` | Pass mechanical checks: 43 files, 13 binaries, 227 first-party plus one SQLite compile unit, 63 Qt transitive package records, seven source archives. Legal/trust verdict explicitly pending |
| Native MSVC/WiX existing `packaging/package.py`, administrative MSI extraction, final security audit under station lease and qualification guard | Pass mechanical checks: 23 files, 16 binaries, 222 first-party plus one SQLite compile unit, 58 Qt package records, six source archives; all PE ASLR/DEP/CFG declarations present and no writable/executable section. Authenticode MSI result NotSigned. Administrative extraction is not ordinary-user installation proof |
| Complete native first-party maps | App 159,337 names, supervisor 858, MCP helper 2,349; byte-identical producers, all required composition anchors and no forbidden fixture seams. Map SHA-256 values and exact commands retained in the handoff |
| Final focused CTest set | 9/9 pass, 26.984s: core boundary, both audit contracts, core/host maintenance, SQLite activation/maintenance and real maintenance/profile-activation public paths. Architecture contract additionally passes, 85.54s |
| Architecture scan | Checkout fails on unrelated ignored `.claude/worktrees/agent-a17cae88115dbbb4e`. Exact `9409ff61` tracked archive passes unchanged scanner, 5.543s. Initial combined suite is honestly 10/11; no unrelated worktree was changed |
| Focused clang-tidy 23.1.1, six policy/host/native TUs | Pass warnings-as-errors, 18.426s. Initial optional-value conversion and const-move warnings in activation corrected; non-user-header warnings remain suppressed by existing tool policy |
| Clang 23.1.1 libFuzzer + ASan/UBSan, no recovery, seed 140962 | Two harnesses, 100,000 iterations each. Core determinism/authority/stale/selection invariants and diagnostic framing/bounds/digest/no-detail-canary invariants pass. Max input 256/16384, five-second per-input timeout; not whole-program/native-vault fuzz coverage |
| Tests-disabled macOS hostile package course | Nine groups pass, 18.305s; two native launches each observe 15 libraries; real MCP read and stale/traversal refusal, five loader switches, hostile env/symlinks/missing helpers/wrong identity/injected bytes and RPATH all exercised |
| Tests-disabled macOS IPC/crash course | Final verifier passes, 9.165s: five hostile frames refused; real second launch opens the second window in the original owner; private named endpoint and no TCP/UDP. Actual OS report SHA-256 `f09cad771a40aae99c608c0532fbe77f8a22becf61f270f2597bfa7f8001bfdf`, 59,365 bytes, contains none of three synthetic environment canaries. No resident-review/token crash or native matrix approval |
| Native runtime inspector self-tests | macOS arm64: 20 libraries; Windows AMD64: 34; WSL x86_64: 13. Owned child/listener/connection observed and unrelated process excluded. Unix endpoint/mode negative control passes on macOS/WSL. Windows profile activation is unimplemented, not an empty-pipe certification |
| Gitleaks 8.30.1 exact producer source + RFC delta | Raw exits 1: 16 source matches and two delta matches (70 commits). Exact review finds fixture digest prefixes, static canaries, review subject IDs and ordinary parser/default-Escape prose; no actual secret. No global ignore/baseline or revocation performed |
| Grype 0.119.0, database v6.1.9 built 2026-09-21T06:39:24Z | **Fail, exit 2:** 21 macOS and 19 Windows matches. High PCRE2 10.47 and Windows zlib 1.3.2 require remediation or independent applicability; macOS WrapZLIB and broad Bitnami SQLite matches require precise source/system adjudication. No suppression, lower-severity disposition or complete coverage assertion |

Initial invalid command arguments/Windows transport quoting and tar metadata
failures are retained locally with their guard receipts. Actual native failures
revealed and corrected the installed-SPDX self-inventory mismatch, MSVC guard
index records and Qt pre-sign/signature distinction; none became an allow-list
waiver. Current inspector tests pass 9/9 natively on Windows; other prior host
contracts remain bounded to their named revisions. Raw crash contents are not
copied into the repository; only synthetic-report identities are retained.

### Remaining acceptance and continuation

Critical/High and lower-severity dependency matches need a reviewed disposition,
fresh final SBOM coverage, and either corrected dependency builds or named,
time-bounded independent applicability evidence. Prebuilt/external dependency
compile hardening and PE CFG call-site coverage are not proved by first-party
flags. Final signed Windows/macOS/declared Linux artifacts still need independent
native trust and checksum verification. Legal/owner review must resolve
first-party terms, mixed syntax-definition licensing, upstream/toolchain terms
and practical corresponding source/build/relink delivery for artifacts outside
the exact macOS v002 approval recorded below.

Clean native Intel, ordinary-user Windows 10/11, Ubuntu 24.04/RHEL 10 and the
complete macOS account/vault/root/removal course remain pending with M8. WSL
Ubuntu 26.04 is inspector-only evidence; no substitute baseline is declared.
Actual resident source/comment/token crash canaries, public-operation network
destinations and native credential/privacy lifecycle remain pending. Historical
M8 human accepts retain their exact old protected installations and do not
transfer to M9 bytes.

The M9 legal-review packet is a single existing AC1 owner/legal obligation,
not a new automated-work approval checkpoint. Acceptance of that question
cannot waive vulnerabilities, authorize publication, or close this card.
The [durable handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m9.json)
records session/candidate identity, pending obligations, exact commands, changed
paths, local/remote writer state and cleanup. BPR-0963/M10 remains independently
Ready through accepted M7; this leaf does not claim or start it. M12/RFC closeout
still requires all unmet predecessors and M8/M9/M11.

The companion prepared `rfc0014-m9-licensing-v001` at session
`cbd48b91000b00f1e72e1ad62c7c384c`, cursor 463, fingerprint
`2ec3475e1eed532ab1455783ae8553b823f6f7ffb005ee8b057ce1be5119c577`.
It is ready/protected with no decision at preparation. Loopback server PID
49181, port 59550, is explicitly handed to the coordinator; the authenticated
URL is not committed. The coordinator owns the durable wait and next-round
interpretation. All other engineering/remote commands are stopped and reaped.
The 2.6GB disposable scan/proof tree and transfer/fuzz artifacts were removed;
the three synthetic OS reports were moved to Trash and remain recoverable.
Native cleanup guards retain unchanged live project/role declarations.
Documentation passes, 64 documentation regressions pass, 957-card scoped board
validation is clean, and the bound checkpoint selects fresh independent M10
without evaluating or granting M9 acceptance.

### M9 post-review source distribution correction

Fresh sole implementer `/root/rfc0014_m9_postreview` resumed at clean
`6ee88ac8afdc30543c0c6ea42e2a596b7556c1c4` under unchanged mandate r3 and
coordinator-implementer rev2. Session `cbd48b91000b00f1e72e1ad62c7c384c`
handed back at sequence 465. Human rejection
`626562d7-65a5-4eb3-842b-30b2573a09e5` applies only to licensing v001,
fingerprint `2ec3475e1eed532ab1455783ae8553b823f6f7ffb005ee8b057ce1be5119c577`.
It requested a separate version-bound source release asset, installed exact
references, equivalent recipient access/retention and preserved replacement
instructions. It is a packaging disposition, not legal advice, security
acceptance, release authorization or an amendment to M8/M9 acceptance.

After explicit resume/claim commits `ce3634ca`/`819cfcc6`, implementation
`1c429749aff4c09c6dc88da930d3344cd2d33c36` updates the existing
`tools/release.sh`, `packaging/package.py` and shared runtime steps. Each platform
emits `BetterPullReview-VERSION-PLATFORM-corresponding-source.zip` beside its
binary outputs. Installed source provenance binds platform/version, producer
commit, exact versioned GitHub URL and SHA-256. Both notices and source offers
carry that reference; the SPDX binds their exact bytes. Package/source audits
reject missing, renamed, substituted, stale-version/commit/URL and embedded
archives, including incorrect archive members or upstream hashes. The existing
private publication script verifies the references inside the app ZIP and
requires source plus its checksum in its eight-asset set before remote work.

The policy requires equivalent source access at no additional charge for every
binary recipient, original-distribution-channel fallback after private-access
changes, and source copies alongside corporate/offline redistribution. Assets
and tags remain immutable; retention is indefinite until the accountable owner
documents that every applicable obligation has ended and matching binaries are
no longer distributed. Ongoing access/retention and practical licensing/build/
relink completeness still require owner/legal disposition. No asset is uploaded,
and the recorded release URL is prospective. The publish-release skill was
updated to match the existing pipeline's eight-asset contract and validated.

The new exact macOS candidate lives at `build/releases/m9-source-1c429749/`:

| Artifact | SHA-256 |
|---|---|
| App ZIP, 40,411,580 bytes | `06c1c17e427bc1aa9a2566ccb295165faee7aa2e8b5716728b9218eaeb527b25` |
| Separate source ZIP, 68,541,007 bytes | `37b0106584fb897a798c0cd048f46fd2051599835f399b8808774af76cfc02af` |
| SPDX | `d47ebc072f819496fdebefe16542f00ee8b940a03f29a7535cede2123a094a74` |
| Ad-hoc sealed application executable | `d3b06eaa1a3fbe8eaa8b6dda502dd4a639a757b0e3fc7ebf57b140f158b90a86` |
| Source offers | `cbcaea3d62bfa278a91ee19a94a3e330b41cbf2454fd3ba1a4985fbdccdd31b8` |
| Third-party notices | `c2a7944463e1f25f082ec926eb184b9669e944b07b52a358e701e6adf3aff8db` |

The app and app ZIP contain no corresponding-source archive. App disk footprint
is 92,536 KiB, formerly 159,472 KiB. Direct comparison confirms all seven upstream
source payloads and all prior notice bytes remain intact. Dependency package
records equal the prior macOS SPDX exactly; this does not turn its unresolved
21 Grype findings or the Windows 19 findings into a pass. The historical scan
tool/database/finding records remain unchanged. Source/archive hashes changed
because of the deterministic archive metadata and new package disposition.

| Verification against `1c429749` | Result and limit |
|---|---|
| Existing `tools/release.sh --version 0.1.3 --prepare-only --skip-tests --output-dir build/releases/m9-source-1c429749` | Pass, 17.222s; universal tests-disabled app, restricted deployment and real relocated launch; offline ad-hoc seal only |
| Four focused CTest contracts: package runtime, source assets, package/security audits | 4/4 pass, 1.789s; 34 tests including all platform notice layouts, MSI/DEB/RPM driver/checksum/provenance fixtures, actual publication shell with disposable git/gh commands, and source refusal controls |
| Exact `verify_release_security.py` | Pass, 2.193s; 42 installed files, 13 binaries, 227 first-party units, 63 Qt records and seven separate source inputs; independent trust/legal/native acceptance remains pending |
| `packaging/verify_release_sources.py` on the actual app ZIP/SPDX/source ZIP | Pass, 0.125s; no URL fetched or remote work performed |
| Guarded tests-disabled `release_package_course.py` | Nine groups pass, 15.675s; exact relocated audit, two native Cocoa launches, loader/refusal controls and real MCP read/stale/path boundaries; live declarations unchanged, children reaped |
| Exact checksum file and `codesign --verify --deep --strict` | Pass; ad-hoc seal does not prove Developer ID/notarization/Gatekeeper |
| Sidecar, prior-source and notice comparison | Pass, 0.212s; no embedded archive and unchanged dependency records; raw receipt `build/m9-source-observations.json` |

A manual simplification/self-review centralized source names/provenance/reference
checks instead of maintaining competing producer/auditor rules. Production C++
is unchanged. Prior native/vault/crash, external-dependency hardening and
independent security/trust findings retain their original identities and scope.
No revised native Windows/Linux package was built in this correction; their
shared pipeline contracts pass with fixtures, and requalification stays pending.

Only [licensing v002](reviews/rfc0014-m9-licensing-v002/manifest.json) presents
the revised exact macOS inventory for the already-required owner/legal decision.
The rejected v001 and accepted M8 packets/installations remain unchanged and
protected; no verdict transfers to new bytes. The current
[handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m9.json) records
the new fingerprint/cursor and reused loopback server. All engineering commands
are complete; the empty task-owned qualification scratch directory was removed.
Preparation is ready/protected at cursor 468 in the same session, fingerprint
`da5616bda5bc5727e9eb8b8cce756357300a9d5102592ca34698ec48aa8dce80`;
no decision was present at preparation. PID 49181/port 59550 was safely reused,
and its complete authenticated URL is handed to the coordinator without being
committed. The coordinator arms `wait --after 468 --kind handed-back` before
yielding; the next engineering round interprets any verdict.
BPR-0962 remains Blocked on `release-security-inputs`, both authored M9 exits
remain pending, and M10 was not started. No tag, push, upload, remote release
mutation, Developer ID signature or notarization was performed.

### M9 licensing-v002 decision finalization

Fresh sole implementer `/root/rfc0014_m9_finalize` entered at clean
`63c786684ebb1c66b314341f6ee4b66a1d569225`, unchanged mandate r3 and
coordinator-implementer rev2. The immutable RFC base remains `d0a8a6dd`.
Session `cbd48b91000b00f1e72e1ad62c7c384c` recorded human accept
`42661017-3d13-43ba-8366-7ac3bd32c0db` at event 469
(`2026-09-21T21:46:10.819293+00:00`) and handed back at event 470.
The decision binds only `rfc0014-m9-licensing-v002`, fingerprint
`da5616bda5bc5727e9eb8b8cce756357300a9d5102592ca34698ec48aa8dce80`,
revision `producer-1c429749-mac-d47ebc07-source-37b01065`, producer
`1c429749aff4c09c6dc88da930d3344cd2d33c36`.

The single existing owner/legal criterion for that revised macOS inventory is
accepted: separate version-bound corresponding source, exact installed
reference/digest and notices, equivalent no-extra-charge recipient access,
original-channel fallback and corporate/offline copies, indefinite retention
until all applicable obligations end, and preserved build/relink/replacement
instructions. The saved note is empty; this record adds no legal opinion or
supporting evidence that the reviewer did not supply. The original manifest's
preparation-time pending wording remains immutable historical context.

This approval does not dispose of vulnerabilities, certify security, establish
independent signature/notarization trust, qualify the native matrix, or authorize
publication. The URL is still prospective; actual recipient access and ongoing
retention must be fulfilled during distribution. Revised native Windows/Linux
packages and their exact source/license binding remain pending. BPR-0962 and
both authored M9 exits remain unaccepted, with Grype's 21 macOS / 19 Windows
failures and all native/vault/crash/hardening gaps preserved.

The read-only public source-asset preflight again returns
`RELEASE_SOURCE_ASSET_OK` for the actual app ZIP, SPDX and source ZIP. SHA-256
checks match every reviewed artifact, including the installed source offer and
provenance. Product/package inputs are unchanged, so the existing 34-test focused
contracts and tests-disabled public course remain their exact-candidate evidence.
No build, release command, remote mutation, or new acceptance test is implied by
this decision-only round.

The decision was durably captured in the
[M9 handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m9.json) before
the companion stopped inherited loopback server PID 49181. The process and its
port-59550 listener are absent; session status has no host or event after 470.
Candidate packets, installations and all six active historical protections are preserved; stopping
the host does not release them. Final command state and verification are recorded
in that handoff. Documentation passes, all 64 documentation regressions pass,
957-card scoped board validation is clean, and whitespace and the four-path
scope check pass. The checkpoint returns `fresh-context` for independent M10,
with engineering acceptance explicitly `not-evaluated` and both M9 exits pending.
The plan and board both identify independent
`m10-documentation-impact` / BPR-0963 as next, with accepted M7 its sole
prerequisite. This owner does not claim or start M10. M12/RFC closeout still
requires the complete M8/M9/M11 and canonical predecessor acceptance.

## Milestone 10: agent and product documentation

Fresh sole implementer `/root/rfc0014_m10` entered at clean
`59226c62058cfa08387a91c872d7208b30275917`, mandate r3,
coordinator-implementer rev2. Claim `4ba42ccea92d3eb04e5a2b4402aa618d81cd79ea`
owns only BPR-0963 / `m10-documentation-impact`. The
[checkpoint](../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m10.json)
binds all three card criteria and both authored exits; accepted M7 is its sole
prerequisite. M8/M9 are not treated as accepted to make this stage eligible.

The tracked [corporate packet](../security/corporate-security-overview.md)
describes implemented effects and their qualification limits. The package guide
now includes actual producer toolchains, recipient checksum/signature checks,
separate source delivery and release stop conditions. The new storage guide
distinguishes raw logs from frozen metadata export, exact purge from uninstall,
remembered grants from session-only tokens, and profile state from excluded
registry/updater/draft-recovery data. It puts the stop-profile and irreversible
cleanup warnings before the corresponding actions.

### Semantic document disposition and source checks

Implementation source is unchanged from entry `59226c62`; exact packaged
evidence remains bound to its earlier producer/artifact, not this documentation
revision. Each row below records source inspection, not a new native verdict.

| Documents | Reconciled facts and direct source/evidence |
|---|---|
| README; package guide; build-and-test; packaging README | `CMakeLists.txt`, `CMakePresets.json`, `packaging/Install.cmake`, native drivers, existing release/publish scripts and installed `publish-release` skill: application installation, no standalone SDK, declared baselines, selected toolchains, Qt/private helpers and eight separate binary/SPDX/source/checksum assets. M9 producer/compiler and PE audit records supply observed Windows/Mac facts. |
| Product contract; storage guide; corporate packet; security threat model | `profile_storage_root.cpp`, `profile_runtime_lease.cpp`, `release_maintenance_files.cpp`, `release_profile_cleanup.cpp`: private roots/leases, exact file/sidecar inventory, known vault-key cleanup before metadata, recovery journal, unknown-object refusal and shared/updater/workspace residuals. No broad vault enumeration, generic repair or secure erasure is promised. |
| Storage guide; build-and-test; corporate packet | `release_diagnostic_snapshot.cpp` and `quill_log.cpp`: 16 MiB raw-log/input cap, 65,536 input records, 8,192-byte lines, 1,024-record/256 KiB output, classified metadata only, frozen preview/new-file export and explicit cleanup; complete OS crash privacy remains pending. |
| System architecture; maintenance contract; release allocation; architecture conformance | Core `release_maintenance.hpp/.cpp`, application maintenance host, `release_maintenance_controls.cpp`, `profile_desktop.cpp`: authority/request/generation/selection identity; typed intents, prepared/commit/terminal events and deltas; one job and serialized completion; retained root lease and independent vault worker through retirement; startup recovery before stores. M2/M5 stale stage-specific descriptions are explicitly historical. |
| UX specification; storage guide | `release_maintenance_panel.cpp`: seven projected actions, exact preview, default/Escape Cancel, two distinct confirmations, stale-dialog invalidation, stopped-profile behavior and preserved preview selection/scroll. The composed public-path test was rerun; no complete native accessibility/usability verdict is inferred. |
| Corporate packet; threat model; foundation security; release gates | `github_host_profile.cpp`, `native_https_transport.cpp`, `gh_cli_git_smart_transport.cpp`, `github_release_update_contract.hpp`, `profile_activation.cpp` and M9 policy: native/GHES/device/CLI/browser/agent/updater authority, declared same-user AF_UNIX listener, bounded frames and owner/peer checks. Idle/environment-canary courses are not active-network, foreign-user or resident-secret crash proof. |
| README; AI agent boundary; corporate packet | `agent_process_launcher.cpp`, `windows_agent_process.cpp`, `github_application.cpp`, `installed_git_process.cpp`, Done BPR-0622 and RFC-0013 ledger: native Windows dispatch and durable workspace recovery are implemented, not pending. Existing credential/context/agent-tool boundaries are preserved; component/native recovery evidence is not RFC-0014 package qualification. |
| Root and packaging AGENTS.md | Reuse existing release paths; bind separate source/access/retention; distinguish audits from approval, purge from uninstall and the sole declared local IPC exception from forbidden network services. No role, consumer configuration or architecture authority is widened. |
| Live RFC; this ledger; BPR-0963; M10 checkpoint/handoff | Track documentation acceptance and next independent work without changing the nine RFC criteria, native/security blockers, immutable authored plans or canonical closeout order. |

No-impact dispositions are source-backed, not omitted obligations:

- The existing updater architecture/security and application-updates guide
  already name the fixed API/asset authority, separate account selection,
  signed helper/startup receipt, transaction recovery, private recovered drafts,
  manual managed/Windows/Linux route and outstanding Intel/live-service inputs.
  M10 changes none of their source paths; the new guides link that contract.
- The M9 release-security policy already specifies emitted hardening, exact
  source bindings, vulnerability disposition and pending native/independent
  evidence. Its requirements and `rfc0014-m9-security-evidence.json` are unchanged.
  M10 summarizes the same failed 21/19-match scans and does not reclassify them.
- Existing retention/epoch and per-store migration owners are unchanged. M10
  adds operator consequences, not another retention policy or generic migration
  switch. Core/application/Qt/provider runtime, CMake, scripts, package metadata,
  release skill and tests receive no executable change.
- `docs/AGENTS.md`, `_coding-standard/STANDARD.md`, CONTRIBUTING and the
  selected consumer `project.json`/`project.md` need no change: the documentation
  gates, source/tracker roots, layered authority and workflow remain the same.
  The RFC index remains Proposed and the adopted twelve-stage plan is unchanged.

### Verification, self-review and continuation

Before documentation edits, the shortest real public release composition check,
`packaging/verify_release_sources.py`, returned `RELEASE_SOURCE_ASSET_OK` for
the protected revised macOS app ZIP, SPDX and separate source ZIP from
`1c429749aff4c09c6dc88da930d3344cd2d33c36`. This verifies installed references
against those exact files without network, signing or publication. The original
M9 34-test contracts and nine-group tests-disabled package course remain reused
for unchanged code/artifact inputs, with their original limitations.

After the documentation edits, the repository documentation gate passed,
all 64 documentation-verifier regressions passed (2.191s), and
`application_release_maintenance_public_path` passed (16.83s). The latter runs
the actual composed maintenance route with fixtures; it does not supply real
vault or cross-platform removal acceptance. Documentation fixture `ERROR` lines
are expected rejection assertions; the suite exited zero. CTest discovery also
confirmed all seven maintenance check names used in the conformance matrix.
Final exact-revision command receipts and the integration/board state are
recorded in the [M10 handoff](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m10.json).

The light coauthoring/technical-prose pass checked four reader tasks: install
and verify trust, export safe diagnostics, remove a profile without losing its
recovery authority, and assess corporate release readiness. It corrected stale
session-only credential, Windows agent/PE, workspace recovery, source-delivery,
core-SDK and absolute no-listener claims. It did not simplify or change product
implementation; that remains M11's bounded assignment.

All M10 criteria concern documentation and source reconciliation; no new human,
native or independent acceptance is required to close BPR-0963. None is claimed.
The existing session `cbd48b91000b00f1e72e1ad62c7c384c` remains handed back at
470, with six protected historical candidates and no host. Accepted v002's
licensing criterion remains specific to the revised Mac inventory; source
access/retention, revised native Windows/Linux artifacts, unresolved
vulnerabilities, trust, vault/crash/privacy/hardening and the complete M8 matrix
remain open. No protection or candidate artifact is changed.

The next independent milestone is M11 / BPR-0964 after BPR-0963 is Done. This
owner stops at the M10 boundary. M12 remains blocked by M8/M9 and predecessor
acceptance, and no publication, remote mutation or release signing is authorized.

Documentation candidate `f6af44a5083a3b05a2a963eeb24b5f199406ab65` passes
the final documentation gate, 64 verifier regressions and all seven focused
maintenance tests (18.77s; composed public path 16.60s). The actual M9 source
asset preflight passes in 0.159s, four principal artifact hashes match, the
957-card scoped board validates, and the explicit source/test/script/package/
release-skill/consumer-context diff against entry is empty. Receipts are
`build/m10-final-{documentation,doc-regressions,composition,source-asset,board,no-product-change}-evidence.json`.
The unchanged review-candidate projection hashes to
`2e472c1178e8282b90331337b2c9c175023d3486282551015346b4b2012b6c5c`.

Guarded finish `2b0ab678bb4d956e6ad29e326cfb8a4e520d0ed2` marks BPR-0963 Done
with all three criteria accepted against `f6af44a5`. BPR-0964 is promoted to Ready,
not claimed. The checkpoint returns `fresh-context` for M11 with engineering
acceptance `not-evaluated`; the helper checks structural binding, not the
documentation's truth. All task commands are complete and no review server,
writer or scratch installation is handed off. The final handoff preserves exact
commands, hashes, unchanged protections and every downstream pending obligation.
