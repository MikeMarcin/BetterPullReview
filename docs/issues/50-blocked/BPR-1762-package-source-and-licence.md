---
schema: "implement-rfc.card.v1"
id: "BPR-1762"
title: "Reconcile mimalloc runtime and corresponding source in packages"
status: "Blocked"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758", "BPR-1760", "BPR-1761"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
blocked_by: "native-macos-candidate"
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
