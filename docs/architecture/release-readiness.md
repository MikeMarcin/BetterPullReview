# Release-readiness allocation

This is RFC-BPR-0014 milestone `m2-architecture-allocation`, BPR-0952, under
owner mandate r3. The inspected source is
`a9a1e3cd1b897bf1242a3dc46ccd986a9dfe0323`; claim
`303ba3b5e2f457f5b0f9385ea999e46c058b84d5` changes only the card's lane.
The immutable RFC base remains `d0a8a6dda005af12b3818234f4878a9f48a44146`.
This allocation records existing code separately from successor design and
qualification. It changes no product behavior or RFC acceptance criterion.

The subsequent [M3/M4 maintenance contract](release-maintenance.md) implements
bounded G5/G6 policy, host/native effects, private local export and the existing
logger's Windows mechanism. It records the exact per-profile inventory and
durable recovery model, residuals and r3 whole-profile-purge prescription
correction. Windows logging now links the existing native Advapi32 capability;
no new logging library or core dependency is introduced. Public controls,
composition and target-native qualification remain M5–M9 obligations. The M2
inventory below retains its historical meaning, including then-missing APIs.

The [live plan](../rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json)
and [verification ledger](../verification/RFC-BPR-0014.md) own sequence and
evidence. The twelve stages still apply; the existing release workflow and
domain hosts are the starting points for their work.

M10 reconciles the completed M3–M9 source work in the
[corporate security packet](../security/corporate-security-overview.md),
[package guide](../development/release-packages.md) and
[storage/removal guide](../development/storage-and-diagnostics.md).
The M2 inventories and then-missing G1–G9 mechanisms below are dated allocation
evidence. They do not override the later implementation or transfer its scoped
acceptance: M8/M9 remain blocked. The current publication set has eight assets,
including a separate version-bound source ZIP and checksum; the old four-asset
inventory below describes the inspected M2 revision.

## Selected build, install and runtime graph

`CMakeLists.txt` configures the audited top-level project and central dependency
providers before declaring `src/core`, `src/application`, `src/adapters` and
`src/app`. `BUILD_TESTING=OFF` removes test sources and private fixture APIs;
`BPR_BUILD_QT=OFF` selects the independent core/application graph. These are
separate controls: a core-only graph can include SQLite and host tests while
`BetterPullReview::Core` itself has no linked library dependency.

| Owner and exact source | Selected responsibility / remaining work |
|---|---|
| `src/core/CMakeLists.txt` | Static `BetterPullReview::Core`, C++23 and its own headers; no Qt/provider link. Preserve this in M3/M6/M7; do not create a separate SDK. |
| `src/application/CMakeLists.txt` | Static `BetterPullReview::Application` links Core; standard-C++ hosts own effect ports and lifecycle. Windows additionally links Bcrypt. No widget/provider library is linked here. |
| `src/adapters/*/CMakeLists.txt` | GitHub: Application + private Qt Core/Network, Glaze and native vault libraries. Git/agent: Application + designated Qt Core process effects. SQLite: Application + controlled static SQLite. Syntax: Application + private static KSyntaxHighlighting/Qt Core. Logging: Application + private Quill. Update: Application + Qt Core and macOS LibArchive/Security/CoreFoundation. Qt presentation: Application + Qt Core/Gui/Widgets, plus native presentation integration. |
| `src/app/CMakeLists.txt` | Composition alone links concrete adapters and Threads. `app_better_pull_review` links Composition; production Mac output is `BetterPullReview.app` with `com.gamecoretech.betterpullreview`. Tests-enabled output has a distinct name/identity. |
| `src/adapters/agent/CMakeLists.txt`, `src/app/CMakeLists.txt` | Build and copy the agent supervisor beside the application on UNIX/Windows. Include its exact bytes/dependencies in every package inventory and process course. |
| `src/app/update_helper_main.cpp`, `src/adapters/update/CMakeLists.txt` | macOS alone builds/copies `bpr_update_helper`; existing release code signs and audits nested Mach-O files. Preserve the qualified updater protocol and exact helper/startup identity. |
| `src/adapters/mcp/CMakeLists.txt`, `src/app/src/github_application.cpp` | MCP helper links only AdapterMcp/Core and serves bounded frozen context through stdio. **Gap G1:** production resolves `tool_better_pull_review_mcp_context_helper`, while `bpr_add_executable` produces `better_pull_review_mcp_context_helper`; app assembly has no production copy/dependency for this helper. The tests-only absolute-path definition masks the discrepancy. M6 must reconcile the emitted filename, platform suffix, dependency and installation; M7 must remove the delivered helper and observe explicit failure. |
| `packaging/icons/`, `packaging/macos/Info.plist.in` | Existing Windows resource, macOS ICNS/plist and Linux desktop/hicolor assets. No application `install()`/CPack, Windows installer, or DEB/RPM definition was found in first-party CMake/package/tool inputs. **G2:** M6 supplies these paths and a default per-user route. |

The application uses installed Git and explicitly configured optional `gh`/agent
executables; they are not silently acquired as runtime package dependencies.
The native browser opener delegates a user gesture to the system browser.
Developer Python/CMake/Ninja/aqt/CPM, test fixtures, attended supervisors and
asset-generation tools are build/verification inputs, not ordinary runtime
payloads. The separately selected designated-live capsule is not the default
release graph.

### Third-party closure and provenance

| Input | Exact owner / version at the inspected source | Distribution accounting |
|---|---|---|
| Qt | `cmake/BprQtProvider.cmake`, `BprDependencies.cmake`: pinned default 6.11.2, or explicit caller-installed package; syntax requires at least 6.9 | Dynamic runtime, platform/TLS plugins and Qt's own bundled transitive components; inspect actual files, license/source-offer obligations and search paths. |
| SQLite | `cmake/BprSQLiteProvider.cmake`, `third_party/sqlite/`: 3.53.4, controlled C/header staging, static link | Include static code/provenance even though no SQLite dylib appears in a runtime scan. Extension loading is compiled out. |
| KSyntaxHighlighting | `cmake/BprSyntaxHighlightingProvider.cmake`, `third_party/ksyntaxhighlighting/`: 6.28.1, static library/definitions; ECM 6.28.0 is its build input | Include the library, embedded definitions and their notices/terms. Static linking does not remove source-offer/relinking analysis. ECM is not a runtime library. |
| Quill and bundled fmt | `cmake/BprQuillProvider.cmake`, `third_party/quill/`: 13.0.0, controlled headers | Code is compiled into the logging adapter; retained MIT/fmt notices are inputs to the delivered notices, not proof they were packaged. |
| Glaze | `cmake/BprGlazeProvider.cmake`, `third_party/glaze/`: 8.3.0, controlled headers | Used by the production GitHub adapter as well as fixtures; include compiled code and its retained notice. |
| LibArchive | `cmake/BprReleaseArchiveProvider.cmake`: 3.8.9, macOS static staging dependency; system zlib enabled | Reuse generated `_bpr/LibArchive-NOTICES.txt`, already copied into the Mac bundle. Reconcile all linked source notices and native dependencies. |
| OS/toolchain libraries | Security/CoreFoundation/AppKit and other linked Apple frameworks/system libraries; Windows Advapi32/Kernel32/Bcrypt; Linux libsecret >= 0.20 and libxcb (linked directly since [[BPR-1024]], and already required by Qt's xcb plugin), with their dependency closure; C/C++/thread runtimes | Distinguish supplied platform prerequisites from bundled files. Inspect symbols, versions, loader origins and transitive TLS/runtime dependencies on each native target. |

**G3:** `_bpr/product-dependency-audit.cmake` is a selected configuration record,
not an SBOM: it includes build tools and does not enumerate the full static or
transitive closure. M6 must join provider provenance, compile/link inputs,
embedded resources, helpers and final deployed files; M9 independently checks
that result against each delivered artifact. No license-compliance verdict is
established by this inventory.

### Reuse the macOS release path

The complete [publish-release skill](../../.codex/skills/publish-release/SKILL.md),
`tools/release.sh`, `tools/publish-release.sh` and `macos-release` preset are
unchanged; their M1 fingerprints were rechecked. The script already owns
universal tests-disabled building, restricted deployment, RPATH normalization,
nested Developer ID/hardened signing, notarization/stapling/Gatekeeper, relocated
smoke and DMG/ZIP/checksums. M6 extends demonstrated gaps in this path; M9
collects independent exact-artifact trust evidence.

The deployed allow-list is QtCore/DBus/Gui/Network/Widgets plus Cocoa, Mac style,
Apple network-information and SecureTransport plugins. The source policy must
be checked against the full package, including every helper and every slice.
**G4:** the preset does not set `CMAKE_OSX_DEPLOYMENT_TARGET`; the existing cache
also has an empty value. M6 pins and checks the macOS 13 floor for all delivered
Mach-O files. A universal header is not native Intel acceptance. The DMG's
`/Applications` shortcut also needs per-user installation instructions/course.
The current relocated smoke accepts exit 0 or 1 with two startup markers;
it does not prove authentication or an exact fixture comparison.

Publication remains a separate, uninvoked capability. The publisher's private
repository guards and four-asset workflow remain authoritative. No version,
tag, push, release upload, signing enrollment or Apple submission is authorized
by this allocation. Its `--dry-run` also contacts remote services and was not run.

## Existing authority and typed paths

| Feature / one authority | Existing public and typed path | Identity, effects and lifecycle |
|---|---|---|
| Retained history: epoch catalog, with core semantic eligibility | `epoch_comparison_control.cpp` captures selected IDs → `prune_review_epochs_intent` / `delete_repository_cache_intent` → `epoch_application_host::dispatch` → `core::transition` → `perform_epoch_maintenance_request` → catalog/cache adapters → `epoch_maintenance_completed_event`, `failed_event` or `cancelled_event` → maintenance/catalog/comparison deltas and `epoch_projection` | `epoch_maintenance_context` in `core/epoch_model.hpp` carries Request, ApplicationSession, full Authority (profile revision, host/account/repository/PR/access mode), Source revision/generations, Generation, Catalog incarnation/revision, Cache, <=100 exact Epochs and TargetDigest. Host owns `epoch_catalog_port.hpp`, `git_cache_port.hpp`, serialized repository actors and durable operation records. Cancellation before the no-return point may cancel; after durable deletion/quarantine, recovery supplies truth. |
| Credentials: exact native vault binding plus host-owned device-binding metadata | Account controls enter the existing GitHub/device hosts; `github_credential_vault_port` and refresh-only port produce typed credential completions; accepted host projections update account state | `github_credential_vault.hpp` binds profile/revision, host authority and account; tokens remain in designated vault/transport capability. `device_binding_store.hpp` retains <=64 metadata records, not tokens. Manual sessions are session-only; remembered device grants survive orderly quit, and Sign out deletes exact token slots and disables their binding. M4 must reuse that lifecycle before purging its metadata, never enumerate/delete a whole vault namespace. |
| Profile/storage selection: validated registry and native runtime lease | `profile_storage_authority::select` → `resolve_profile_storage_root` → startup admission/runtime lease → profile services; profile/window intents produce management commands and accepted projections | Profile ID, registry revision/root identity and `bpr_window_context` generations are distinct from a GitHub profile/account. `profile_storage_root.cpp`, `profile_runtime_lease.cpp`, `state_directory.hpp` and `catalog_database.cpp` own native root admission. A path/label/mtime alone grants no deletion authority. Default and isolated profile roots must not become competing owners. |
| Schema migration: each durable store | Host-owned store open/load/save → adapter transaction and typed result → owning host's accepted projection | `appearance_settings_database.cpp`, `workbench_settings_database.cpp`, `queue_enrollment_database.cpp`, `review_ledger_database.cpp`, `catalog_database.cpp`, device bindings and window restoration already have versioned migrations/refusals. The SQLite adapter owns application/schema identity and atomicity; core validates semantic values. No universal migration engine is needed. M4 qualifies old/future/foreign/corrupt stores and preserved bytes. |
| Local diagnostics: profile-owned logging capability | `diagnostic_event` → `diagnostic_log_port::record` → `logging/quill_log.cpp`; producer domains retain their own acceptance decisions | Fixed operation/outcome, WindowSession/Request and bounded sanitized summary. `diagnostic_summary` caps prose at 2,048 bytes; segment ceiling is 5 MiB, aggregate 50 MiB, file count 64 and inactive age 30 days, producer queue 64 KiB, admitted producer identities <=16. Composition closes logging after producers quiesce. BPR-1216 rotates separate session files with coordinated retention and metadata-only maintenance export. Native Windows/Linux retention qualification is pending. |
| macOS update: one desktop-wide service | General/menu intent → `release_update_intent` → deterministic transition/request → `release_update_application_host` ports → typed completion → `release_update_projection`; install enters the desktop shutdown barrier and exact helper/startup transaction | `release_update_context` binds Session, Request, Generation, Access lease, Operation and optional exact Candidate. Native adapter owns staging/trust/installation; journal/inode identity gates cleanup. The updater is not profile-purge authority. Preserve Windows/Linux/managed manual distribution, BPR-0950 Intel deferral and separate BPR-0911/0912 live-service gates. |

The existing maintenance public course was exercised in M2 through the real
development executable: seed → prune → restart/delete → restart/verify deleted.
This supports reuse of that path. It does not qualify a tests-disabled package,
signing, vault persistence, new diagnostic controls or native human interaction.

The exact source entry points behind that table are:

- `src/core/include/bpr/core/epoch_model.hpp`,
  `src/core/src/epoch_model.cpp`,
  `src/application/src/epoch_application_host.cpp`,
  `src/application/include/bpr/application/epoch_catalog_port.hpp`,
  `src/application/include/bpr/application/git_cache_port.hpp` and
  `src/adapters/qt/src/epoch_comparison_control.cpp` for retained history.
- `src/application/include/bpr/application/github_credential_vault.hpp`,
  `src/application/src/github_credential_vault.cpp` and
  `src/application/include/bpr/application/device_binding_store.hpp` for
  credential authority and binding metadata.
- `src/application/src/profile_storage_authority.cpp`,
  `src/adapters/sqlite/src/profile_storage_root.cpp`,
  `src/adapters/sqlite/src/profile_runtime_lease.cpp` and
  `src/adapters/sqlite/src/state_directory.hpp` for root admission/lifetime.
- `src/adapters/sqlite/src/catalog_database.cpp`,
  `appearance_settings_database.cpp`, `workbench_settings_database.cpp`,
  `queue_enrollment_database.cpp`, `review_ledger_database.cpp` and
  `device_binding_store.cpp` in that same directory for durable stores.
- `src/application/include/bpr/application/diagnostic_log_port.hpp`,
  `src/adapters/logging/src/quill_log.cpp` and
  `src/adapters/qt/src/settings_window.cpp` for logging and the missing
  Settings controls.
- `src/application/include/bpr/application/release_update_application_host.hpp`,
  `src/application/src/release_update_application_host.cpp` and the existing
  [updater security allocation](../security/release-updater.md) for updater
  request/port ownership and exact helper transactions.

## Bounded additions assigned to M3–M5

**G5: diagnostic preview/export and local clearing are missing.** Settings'
Storage & privacy page is a placeholder. Keep the one logging adapter and
credential-owner sanitization; do not start a second logger or copy raw logs
into a default support bundle. The RFC-0027 design is a compatibility reference,
but its deferred BPR-0603 remains unclaimed: RFC-0014 authorizes only the
overlapping controls/privacy/retention required for release acceptance. It does
not activate the optional recent-context ring, broad Verbose coverage or the
whole later RFC.

M3 defines compact deterministic operation/selection policy. The proposed
semantic vocabulary is preview diagnostics, export the accepted preview,
clear diagnostics, confirm exact purge, cancel, and typed
prepared/completed/failed/cancelled events. These are design concepts, not
existing APIs. State holds one pending operation, request ID, BPR profile and
session, operation generation, accepted selection revision/digest and terminal
reason. Bounds, allowed categories, stale/duplicate refusal and safe defaults
are core decisions; clock/file/vault/Quill types remain outside core. Do not
replicate epoch pruning, updater decisions or store-schema transactions here.

M4 owns narrow prepare-snapshot, export-snapshot and maintenance capabilities.
Adapters capture a bounded immutable metadata-only export snapshot; unknown or
unclassifiable legacy prose is omitted with counts or refused, not copied.
No tokens, source/diff/comment bodies, notes, drafts, agent text, raw database,
MCP snapshot or crash-memory file enters a default export. Preview and export
must name the same snapshot revision/digest, record limit and byte limit;
destination selection supplies only a narrowly admitted local output capability.
Cancellation, profile replacement or changed selection invalidates publication.
No UI action uploads a bundle. Native crash reports are external OS artifacts:
M9 must inspect their actual behavior or preserve an explicit failed/pending
criterion; exclusion from export alone does not establish canary redaction.

**G6: profile-wide purge and uninstall residuals are not implemented by epoch
cache deletion.** M3 names allowed artifact categories; M4 owns exact admitted
root/object identities and an explicit inventory. Keep registry/shared updater
state and nested sibling profiles outside a current-profile purge. Stop new
ingress, invalidate generations, drain all windows/effect producers, close
stores/logger, then perform narrow native cleanup under the retained lease.
Default-root deletion must never recursively consume `profiles/`. Delete only
known credential bindings through their owner before removing binding metadata;
failure remains visible and recoverable. Catalog tombstones, recovery journals,
WAL/SHM sidecars and unknown/changed files require explicit dispositions.
Developer worktrees, GitHub state and already exported user files are outside
that authority. Quitting or uninstalling the executable does not itself prove
data or credential removal.

The owned-data inventory must cover `state/v1/` stores (appearance, workbench,
review queue/ledger, window restoration, device bindings, mutation/submission
recovery), retained Git/cache/catalog data, agent workspaces, `mcp-snapshot/`
and recognized session logs plus legacy `logs/diagnostics.log`. The coordination lock remains. `profile-registry.sqlite3`, desktop release-update
settings and update transaction data have different owners; they need their
own all-profiles/uninstall course. No store is safely deletable merely because
its filename ends in `.sqlite3`.

M5 mounts these controls in the existing profile Settings surface and makes
diagnostics reachable while signed out. Qt owns confirmation, preview, native
save gesture, keyboard/focus/accessibility and incremental rendering. It sends
typed identity-bearing intents; it never reads a log, traverses a directory,
chooses a vault key or performs deletion. Cancel is the default/Escape for
destructive confirmation. One status change updates one bounded projection;
it does not reset the review model or scan the entire PR/history. New limits
must be explicit/tested in M3/M4, not inherited from the larger deferred RFC's
proposal as if already measured.

## Package and security work allocation

**Current G7 scope, owner amendment 2026-10-11.** The next Linux release selects
Ubuntu 26.04 LTS x86_64 DEB/per-user archive and glibc 2.43 ceiling, with actual
GLIBCXX requirements recorded. WSL2 may produce ordinary binaries. Clean Ubuntu
desktop/authentication/Secret Service/installation/purge evidence remains open.
Ubuntu 24.04 and RHEL are outside the selected set; optional RHEL RPM mechanics
retain their own older constraints without becoming required release work.
[BPR-1386](../issues/60-done/BPR-1386-prepare-ubuntu-2604-release-route.md)
owns bounded preparation. This owner amendment is separate from observations;
the historical M6 rationale and unchanged M8/M9 plan below remain preserved.

**Historical G7: target baselines and native qualification.** For M6 implementation, the
initial Linux ABI floor is glibc 2.39, with Ubuntu 24.04 LTS x86_64 as the DEB
baseline and RHEL 10 x86_64 as the RPM qualification environment. This fills the
RFC's previously unspecified baseline; it is not an observed support claim.
Qt's pinned-series [platform documentation](https://doc.qt.io/qt-6.10/supported-platforms.html)
states its official Linux binaries use Ubuntu 24.04/glibc 2.39. RHEL 10 supplies
glibc 2.39 and defaults to x86-64-v3; that RPM environment's CPU requirement must
be explicit. [Red Hat toolchain facts](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/10/html/considerations_in_adopting_rhel_10/compilers-and-development-tools)
do not prove BPR builds or runs there. M6 records its actual C++23 toolchain and
GLIBC/GLIBCXX/CPU requirements; M8 tests both environments. Do not infer RHEL 9,
older glibc, arbitrary distributions or newer-OS compatibility from a successful
development fixture. If required inputs cannot satisfy this floor,
record a plan/mandate reconciliation instead of silently raising it.

**G8: hardening and runtime evidence.** `cmake/Hardening.cmake` enables MSVC
`/sdl /guard:cf` with a complete (`/INCREMENTAL:NO`) link in every
configuration, because an incremental relink can leave a vtable target out of
the Guard CF table ([[BPR-1027]], `cfg_complete_link_contract`),
non-MSVC stack protection/frame pointers and non-Debug
`_FORTIFY_SOURCE=3`, plus Linux RELRO/NOW. M9 inspects emitted flags and actual
PE/Mach-O/ELF attributes, including helpers and static third-party code; source
options are not evidence of effective protection. M7/M9 bind applicable
coding-format/tidy, sanitizer, fuzz, secret and vulnerability scans to tools,
databases, inputs, findings and a written release policy. Existing
`scripts/verify_binary_dependencies.py`, `verify_runtime_surface.py`, the
architecture checks and their hostile fixtures are reusable; no complete SBOM
or release-vulnerability policy was found in the package path.

Runtime inspection must account for designated native HTTPS/vault adapters,
installed Git, configured `gh` and its helper residual risks, agent supervisor,
stdio MCP helper, system browser, macOS updater helper/trust commands and
profile activation. **G9:** `sqlite/profile_activation.cpp` implements a
bounded owner-authenticated AF_UNIX listener (a local named pipe on Windows
since [[BPR-0979]]) under the profile lease, while old
security prose and the RFC's row 3 use broad no-listener/no-local-server wording.
M9 must explicitly reconcile that existing IPC contract and verify its owner,
peer, frame, lifetime and no-network-service limits before accepting row 3.
M2 neither erases that surface from the inventory nor waives the criterion.

### Per-criterion public checks and owners

All nine rows remain pending final RFC acceptance. Commands below describe
successor checks, not M2 execution.

| RFC row | Concrete public check and required terminal | Environment / owner |
|---|---|---|
| 1 | Install exact package as an ordinary user; launch its tests-disabled executable; authenticate through an authorized route; open the safe fixture PR, select a file and compare both displayed revision/line identities through the shared alignment controller and DiffLinkMap; uninstall and enumerate documented residuals | Clean Windows 10 and 11 x64/MSVC, macOS 13+ native arm64 and Intel, baseline DEB/RPM desktops. BPR-0956/0957 prepare; BPR-0958–0961 own native evidence and needed attended decisions. |
| 2 | Verify exact delivered bytes independently: Windows `signtool verify /pa /all`; Mac `codesign --verify --deep --strict`, `stapler validate`, `spctl` execute/open; native RPM/DEB signature verification; reject a modified copy | Native trust tools/trust roots and signing authority, artifact hashes, operator identity. BPR-0962. Existing package-generation success is insufficient. |
| 3 | Run `verify_binary_dependencies.py`/`verify_runtime_surface.py` plus native process/socket/loader inspection during normal public operations; inject an untrusted plugin/library origin and prove refusal; enumerate the G9 IPC surface | Every delivered native platform, including packaged helpers and selected TLS/vault backends. BPR-0956/0957/0962. |
| 4 | Reconcile machine-readable SBOM and file hashes against the expanded package, static link inputs, helper/resource inventory and upstream notices/source offers; omission/substitution must fail | Exact committed release source, pinned inputs and every delivered package. BPR-0956 generates; BPR-0962 verifies independently. |
| 5 | Build with declared hardening; inspect emitted binary properties; run policy-selected static analysis, sanitizers, fuzzers, secret and vulnerability scans; retain failed findings and explicit dispositions | Each supported toolchain/native ABI, available sanitizer/fuzzer instrumentation and versioned vulnerability data. BPR-0957/0962. Missing tools/evidence do not become passes. |
| 6 | Through real maintenance/account controls, seed disposable profile/vault/cache data, migrate/restart, cancel or interrupt cleanup, purge/uninstall, then inspect exact retained/deleted items and permissions including sidecars, siblings and remembered grants | Disposable native user/profile roots and vault integration namespaces; live authentication only when separately authorized. BPR-0953–0955 implement; BPR-0958–0961 qualify. |
| 7 | Seed synthetic credential/source/comment/draft canaries; cause public-path failures, preview/export, corrupt a record and change selection after preview; inspect every resulting log/export/crash artifact and verify failure does not release raw content | Tests-disabled app plus safe fixture course and native crash-report policy. BPR-0953–0955/0957/0962. No actual secret or private repository payload in retained evidence. |
| 8 | Reconcile corporate packet's network/data/process/credential/AI/logging/removal table against the same package's observed effects, private-root inventory and G1–G9 dispositions | Cross-platform evidence, final threat model and operator procedures. BPR-0963; publish here means tracked documentation, not external distribution. |
| 9 | Build/test Core with Qt/providers unavailable; inspect selected targets, link maps/symbols and tests-disabled compile/byte graph; drive the real selected-file comparison and reject a missing required capability | Core-only graph and each package's actual host/composition, including helper inventory. BPR-0953/0956/0957/0962. No invented SDK or fixture-only acceptance. |

M10 reconciles `README.md`, `packaging/README.md`, build/operator guides,
product/system architecture, architecture-conformance and release-gates,
foundation/threat security, data retention/removal and the corporate packet.
The historical package README's token-cleanup and old platform observations
must be labeled against their original candidates; they cannot override current
remembered-device behavior. The updater architecture/security/recovery documents
remain authoritative for that separate service. M11 simplifies only changed
implementation; M12 alone reconciles the final RFC-base diff, full matrix and
canonical predecessor closeout.

## No-impact and evidence limits

M2 changes allocation and workflow records only. Shared GitHub state, review
mutation admission, exact diff/alignment policy, schema behavior, visual assets,
credential semantics and updater installation behavior are unchanged. Existing
history maintenance is a reuse candidate supported by the direct public check;
migrations and credentials still need their assigned focused/package evidence.
No generic planning stage justifies duplicating their owners.

The current product has no attended M2 criterion. There is no review server,
protected candidate, new platform qualification or independent verdict from
this allocation. Full clean-target, trust, privacy and predecessor obligations
remain on the live cards; the Linux baseline is a declared engineering target,
not a passing native result.

## M5 G5/G6 presentation disposition

BPR-0955 mounts the typed maintenance panel in the actual profile Settings shell.
The [M5 contract](release-maintenance.md#m5-settings-presentation-and-bounded-composition)
records native-host preview/export and controlled clear/purge presentation,
exact-target dialog invalidation, incremental updates and shared-window Settings
ownership. Existing epoch retention controls and their underlying policy remain
unchanged. Qt receives no effect capability. Diagnostic access remains signed-out.

Live clear/purge admission is explicitly unavailable until BPR-0956 supplies the
complete retained-lease lifecycle barrier, including actual window/producer
retirement, original-owner recovery, store/logger closure and the independent
vault worker. This is pending M6 composition, not a no-impact disposition or a
waiver of G5/G6. M5 rendered development tests do not satisfy the tests-disabled
package, native installer/vault, human or independent trust matrix above.

## M6 composition and package disposition

BPR-0956 resolves G1's production helper path by using the emitted target filename
and application-directory location, adding the real build/copy/install edge for
MCP, and retaining the supervisor and macOS updater helper. G2/G3/G4 now have
tests-disabled application-only Windows/Linux install definitions, native runtime
allowlists, per-user routes, source archives, complete notices and an exact-byte
SPDX/checksum generator. macOS extends `tools/release.sh`; `--prepare-only` takes
its existing deployment/smoke route with offline ad-hoc sealing, without Apple
credentials/submissions. It is not a second release pipeline. Both slices are
checked against the macOS 13 floor. The existing private publication script
requires the resulting SBOM assets but publication remains unauthorized.

The [package guide](../development/release-packages.md) records exact baselines,
runtime layout, strict plugin discovery, Linux X11/XWayland and system C++/TLS
dependencies, and first-party/mixed syntax-data distribution approval still
pending. At M6, G7's Ubuntu 24.04/RHEL 10 and Windows definitions were not qualified
by this macOS implementation, and WSL Ubuntu 26.04 was refused as a
package-build baseline. The current owner amendment above replaces that Linux
selection without accepting qualification. Signing, clean install/upgrade/uninstall, credential,
native UI and independent exact-artifact security gates remain M8/M9.

G5/G6 now use the [retained-lease desktop barrier](release-maintenance.md#m6-retained-lease-maintenance-composition)
before enabling destruction. Real profile owner retirement, original recovery,
store/logger closure, an independent native vault and startup receipt recovery
are composed; the real multi-window public course covers their observable
boundary. G8/G9 and all other assigned later security obligations are unchanged.
