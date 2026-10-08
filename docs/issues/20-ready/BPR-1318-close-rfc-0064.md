---
schema: "implement-rfc.card.v1"
id: "BPR-1318"
title: "Verify and close RFC-BPR-0064"
status: "Ready"
rfcs: ["RFC-BPR-0064"]
depends_on: ["BPR-1317"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
closeout_for: ["RFC-BPR-0064"]
---

# Verify and close RFC-BPR-0064

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

RFC base: `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** Through the composed Help and Settings export paths, both privacy modes open Save in the OS-reported Downloads directory with the existing filename, including when the application is launched from a disk root or another working directory and no PR/account session exists. | Exact frozen 5/5 export and 3/3 boundaries; ledger native session | Automated pass; actual native chooser pending | ca51d5df7ed9564a44f971ae792956a093f60965 |
| **AC02:** Controlled unavailable-folder cases select Documents, then home; localized/redirected eligible locations are honored, and complete unavailability preserves the preview without an arbitrary/root fallback. | Exact frozen 5/5 export and 3/3 boundaries; ledger native session | Pass: controlled redirected/fallback/unavailability | ca51d5df7ed9564a44f971ae792956a093f60965 |
| **AC03:** An explicitly chosen alternate destination receives the exact previewed bytes through the existing output path; cancellation, an existing file and stale preview cannot cause an unintended write or overwrite. | Exact frozen 5/5 export and 3/3 boundaries; ledger native session | Pass: frozen bytes, cancel, overwrite and stale refusal | ca51d5df7ed9564a44f971ae792956a093f60965 |
| **AC04:** Folder discovery and chooser opening perform no directory/file creation or permission changes, preserve the architecture boundaries, and the actual native Save chooser shows the proposed location on each claimed platform. Automated chooser-state checks alone do not establish native dialog behavior. | Exact frozen 5/5 export and 3/3 boundaries; ledger native session | Automated no-write/boundary pass; actual native chooser pending | ca51d5df7ed9564a44f971ae792956a093f60965 |
