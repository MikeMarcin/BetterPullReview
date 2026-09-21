# RFC-BPR-0014 verification and planning ledger

RFC base: `d0a8a6dda005af12b3818234f4878a9f48a44146`.
Mandate `r1`, amended by the owner's explicit release-workflow reuse instruction
`r2` and prescription-modernization instruction `r3`, authorizes this run;
current child `/root/rfc0014_m3` owns only
`rfc-bpr-0014-release-readiness/m3-deterministic-functional-core` on `main`.
The M1/M2 records below retain their original child and candidate provenance.
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
| 1 Clean installation and operation | Install the exact candidate in a clean native user account; launch, authenticate through an authorized safe route, open a fixture PR, select a file and verify both displayed source identities; uninstall and inspect residuals | BPR-0956–0961 | Not run for the delivered matrix |
| 2 Package trust | Verify the delivered installer/app/package signature, macOS notarization/staple and checksum using native trust tools independently of package generation; reject altered/untrusted artifacts | BPR-0962 | Existing Mac evidence is bounded to its recorded candidates |
| 3 Runtime attack surface | Inspect packaged dependency closure plus native sockets, spawned processes, Qt plugin/library resolution and network destinations under public operation; compare with explicit allow-lists | BPR-0956, BPR-0957, BPR-0962 | Not run on final packages |
| 4 Inventory and licensing | Generate SBOM/notices/source-offer manifest from the actual shipped closure, reconcile versions/provenance and hash every delivered artifact; fail on omissions or substitutions | BPR-0956, BPR-0962 | Complete transitive inventory remains pending |
| 5 Release hardening policy | Name platform compiler/linker/runtime flags and release policy; run relevant static analysis, sanitizer/fuzz, secret/vulnerability scans with tool/database versions and adjudicated findings | BPR-0957, BPR-0962 | Policy and exact-artifact evidence pending |
| 6 Storage and removal | Exercise vault/file/cache permissions, retention, explicit purge, migration, crash recovery and uninstall in disposable roots; compare actual retained/deleted items with docs | BPR-0953–0955, BPR-0958–0961 | Earlier component evidence does not prove packaged lifecycle |
| 7 Diagnostic privacy | Seed source/comment/token canaries, preview/export diagnostics and inspect crash/log artifacts; defaults omit bodies/secrets and redaction survives hostile inputs | BPR-0953–0955, BPR-0957, BPR-0962 | Required controls and package course pending |
| 8 Corporate packet | Reconcile network/storage/process/credential/AI/logging/removal claims with observed composed effects and final threat model | BPR-0952, BPR-0963 | Existing docs are inventory inputs; final packet pending |
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
| Core/host/Qt and typed ownership, identity, concurrency, projections | System architecture and architecture-conformance matrix | Plan recorded here; M2 allocates exact changes, M10 reconciles |
| Application build/install/package, supported baseline, dependencies and license/source offer | README, build-and-test, packaging README and release tooling/operator guide | M6 implementation; M10 final source-backed instructions |
| Credentials, storage, retention/purge, migration and removal | Product contract, foundation security, security threat model and operator docs | M3–M5 implement/qualify; M10 reconciles |
| Network, subprocesses, AI, diagnostics and corporate posture | Security threat model, corporate packet, diagnostic/operator docs | M9 evidence; M10 final packet |
| Qualified macOS updater and manual managed/Windows/Linux policy | Existing release-updater architecture/security and application-updates guide | Preserve owner amendment; revalidate only affected package boundary |
| RFC acceptance, canonical order and continuation | RFC/index, adopted plan, live cards, this ledger and BPR-0965 | M1 creates truthful records; M12 alone may close |
| Agent guidance | Applicable AGENTS.md and consumer context | No M1 rule changed; M10 updates only newly evidenced hazards or ownership changes |

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
