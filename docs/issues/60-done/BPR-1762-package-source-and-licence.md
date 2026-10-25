---
schema: "implement-rfc.card.v1"
id: "BPR-1762"
title: "Reconcile mimalloc runtime and corresponding source in packages"
status: "Done"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758", "BPR-1760", "BPR-1761"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC07","outcome":"pass","revision":"0f42035a1758a33c729317d66d3aaaac0e771fb1","evidence":{"kind":"observation","detail":"Exact signed/notarized Mac ZIP/DMG/private relocated install and full 53-file/19-image/519-unit audit; pinned runtime/notices/SPDX/separate-source agreement and omission/extra controls. Retained actual Windows MSI/Ubuntu DEB and all three CLI archive closure, source, relocation/removal and negative controls keep original producers. Human Cocoa acceptance is exact r9 decision only; no release/legal/native uninstall or signed-pointer claim.","ledger":"docs/verification/RFC-BPR-0093.md#post-review-reconciliation--round-r10","producers":{"mac_app":"0f42035a1758a33c729317d66d3aaaac0e771fb1","windows":"c5693fd05e9a673381cae5aa76c355db33e39182","linux":"0c40cca40e5d93205d83fd65f76af029ac937347","cli":"df6e8be48ff17271e2258f0040e4ca87795f44bd"}}}]
---

# Reconcile mimalloc runtime and corresponding source in packages

## Scope and acceptance

Integrate the actual selected mimalloc runtime file graph with BPR's macOS,
Windows and Ubuntu package closure and standalone CLI install (AC07). Update
the exact dependency inventory, installed MIT notice, SPDX and separate
versioned corresponding-source asset. Preserve native signing/hardening,
load-path, source digest and release refusal checks; prove removal and reject
missing or extra runtime files. This is package qualification, not release
publication or a security-owner vulnerability disposition.

## Current Mac reconciliation — authorized round r9

The missing local Mac trust/closure course now passes on signed producer
`0f42035a1758a33c729317d66d3aaaac0e771fb1`: both app and DMG are notarized,
stapled and Gatekeeper-accepted; private relocated installation matches the
ZIP/DMG; full 53-file/19-image audit, source binding and allocator omission/extra
controls pass. The [r9 ledger](../../verification/RFC-BPR-0093.md#signed-local-macos-acceptance--authorized-round-r9)
records exact hashes, Apple receipts, public paths and limitations. Prior native
Windows/Ubuntu and standalone CLI package/source evidence keeps its original
producer identity. No release publication or legal approval is inferred.

AC07 is accepted after exact review intake and final evidence reconciliation.
Human decision `958b2988-5f73-4ebf-beea-cdffa5cbe603` accepts the delivered Cocoa
criterion for this exact r9 candidate; session `3a0425eb3756874560105a4cff2b0b64`
is handed back at consumed cursor 6. The
[post-review ledger](../../verification/RFC-BPR-0093.md#post-review-reconciliation--round-r10)
records that narrow verdict and keeps native pointer proof on [[BPR-1759]].
The signed Mac package/source audit combines with unchanged native Windows,
Ubuntu and three-platform CLI closure/removal/negative-control evidence at
its original exact producers. This card's package/source scope is complete.

Broad suites were skipped for local Mac validation; ordinary native install/
upgrade/uninstall, vault/crash/vulnerability, legal recipient access/retention
and distribution approval remain separate. No version change, tag, push or
publication occurred. Earlier pending statements below retain historical scope.

## Historical acceptance — mandate r5, 2026-10-24

The owner's “Amend Intel criterion” decision removes native Intel execution
from macOS AC03 and its milestone/card. Universal arm64/x86_64 static proof and
the existing native arm64 observations remain bound in the
[ledger](../../verification/RFC-BPR-0093.md#intel-acceptance-amendment--mandate-r5);
no native Intel execution is claimed. This card remains **Blocked** on the
actual Developer-ID signed-installed Mac course and delivered native Cocoa
acceptance on [[BPR-1759]], with final AC07/AC09 reconciliation still pending.
Its final dependency and other package/source/trust obligations are unchanged.

The r4 records below retain their historical scope and evidence; r5 supersedes
their Intel-pending statements only.

## Scheduling amendment — mandate r4, 2026-10-24

The owner's instruction to keep working without an Intel Mac authorizes the
independent package and measurement work. Accepted [[BPR-1758]], [[BPR-1760]]
and [[BPR-1761]] remain prerequisites; full Mac acceptance no longer blocks
starting this card. The live distribution milestone already depended only
on Windows/Linux. The checked plan amendment makes the pending
`pinned-macos-integration` dependency explicit at final closeout. Every AC07
and AC08 requirement, authored outcome, work selector and exit remains intact;
this card cannot be Done while its own required evidence is incomplete.

Source basis: the independent native Windows/Linux installed courses in the
[ledger](../../verification/RFC-BPR-0093.md), RFC AC03 versus AC06–AC08, and
the separate package/instrumentation/developer-baseline paths in
`packaging/`, `cmake/BprMimallocProvider.cmake` and `CMakePresets.json`.
Native Intel/signed-installed Mac AC03 remains on [[BPR-1759]] and final
[[BPR-1764]] still depends on it. No acceptance or release approval is waived.
The check receipt is under `build/mimalloc-distribution/amendment-check.json`.

## Independent result and remaining block — mandate r4

The [final ledger](../../verification/RFC-BPR-0093.md#distribution-result--mandate-r4)
binds all agent-capable package work: actual unsigned DEB/MSI private extraction
and full runtime/source audits against their retained exact producers; new
standalone CLI archive preparation on native Mac, Windows and Ubuntu at
`df6e8be48`; MIT notices, SPDX, separate version-bound source; complete file and
producer closure; public archive expansion/relocation/removal and missing/extra
allocator controls. The CLI reuses the installed-runtime path guard; actual
Linux observation excludes the supplied foreign Qt plugin path. Windows maps
retain their strict guards and use separately byte-pinned SDK metadata evidence.
No source substitution, fixture seam, blanket map allowance or ambient loader
workaround was introduced. All protected earlier candidates are unchanged.

AC07 is partial. The existing universal Mac bundle and current universal CLI
have only an ad-hoc seal and arm64 execution. The actual native/signed installed
Mac candidate remains unavailable, with Intel, Developer ID/notarization/
Gatekeeper and delivered Cocoa observations retained on [[BPR-1759]]. Offline
closure cannot replace that requirement. This card is Blocked on
`native-macos-candidate`; no pending acceptance or final closeout dependency
is waived. Native install/upgrade/uninstall, distribution legal approval and
release trust remain separately qualified; private removal is not uninstall.

No owner review item is ready from this objective milestone, and no review
server or URL exists. All own commands are finished. Next: supply the actual
authorized Mac candidate/course, verify the affected exact source/runtime and
signing boundaries, then reconcile this card and AC09 on [[BPR-1764]]. The raw
records and isolated roots named in the ledger are retained. No publication,
push, signing credentials, keychain access or owner desktop was used.
