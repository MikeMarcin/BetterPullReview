---
schema: "implement-rfc.card.v1"
id: "BPR-1318"
title: "Verify and close RFC-BPR-0064"
status: "Done"
rfcs: ["RFC-BPR-0064"]
depends_on: ["BPR-1317"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
closeout_for: ["RFC-BPR-0064"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"ca51d5df7ed9564a44f971ae792956a093f60965","evidence":{"kind":"observation","detail":"Help/Settings both privacy modes, disk-root cwd, signed-out: frozen 5/5 at ca51d5df7 and head replay 5/5 at 3836d3486 (build/dev, 15s); owner native macOS Accept decision 4ac53a5e (BPR-1317 C2)."}},{"criterion":"AC02","outcome":"pass","revision":"ca51d5df7ed9564a44f971ae792956a093f60965","evidence":{"kind":"command","detail":"Controlled redirected/localized, Documents then home fallback and complete-unavailability preview retention: frozen 5/5 and head replay 5/5 at 3836d3486."}},{"criterion":"AC03","outcome":"pass","revision":"3836d348666a4f797e5f4fe20e484b84e5603f8f","evidence":{"kind":"command","detail":"Exact frozen bytes to alternate destination, cancel, overwrite refusal and stale preview: replayed after 980c08873 snapshot-content change; application_release_maintenance_public_path 10.70s, 5/5 rows pass."}},{"criterion":"AC04","outcome":"pass","revision":"ca51d5df7ed9564a44f971ae792956a093f60965","evidence":{"kind":"observation","detail":"No discovery writes and boundaries 3/3 on exact source; actual native macOS Save chooser location accepted by owner (session 5c434605, decision 4ac53a5e). macOS is the only claimed platform; Windows/Linux native not claimed."}}]
---

# Verify and close RFC-BPR-0064

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

RFC base: `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** Through the composed Help and Settings export paths, both privacy modes open Save in the OS-reported Downloads directory with the existing filename, including when the application is launched from a disk root or another working directory and no PR/account session exists. | Frozen 5/5 export at `ca51d5df7`; head replay 5/5 at `3836d3486`; owner native Accept (BPR-1317 C2) | Pass on macOS: automated composed routes and the owner's native chooser observation through Help and Settings in both modes | `ca51d5df7`; rechecked at `3836d3486` |
| **AC02:** Controlled unavailable-folder cases select Documents, then home; localized/redirected eligible locations are honored, and complete unavailability preserves the preview without an arbitrary/root fallback. | Frozen 5/5 export; head replay 5/5 | Pass: controlled redirected/fallback/unavailability | `ca51d5df7`; rechecked at `3836d3486` |
| **AC03:** An explicitly chosen alternate destination receives the exact previewed bytes through the existing output path; cancellation, an existing file and stale preview cannot cause an unintended write or overwrite. | Frozen 5/5 export; head replay 5/5 after the 980c08873 diagnostic-content change | Pass: frozen bytes, cancel, overwrite and stale refusal | `ca51d5df7`; rechecked at `3836d3486` |
| **AC04:** Folder discovery and chooser opening perform no directory/file creation or permission changes, preserve the architecture boundaries, and the actual native Save chooser shows the proposed location on each claimed platform. Automated chooser-state checks alone do not establish native dialog behavior. | Frozen 5/5 export and 3/3 boundaries; owner native Accept (BPR-1317 C2) | Pass on macOS, the only claimed platform: no-write/boundary checks plus owner native observation; Windows/Linux native not claimed | `ca51d5df7`; rechecked at `3836d3486` |

## Closeout (2026-10-08)

Closed by dispatch `parallel-integrator-r5-001` against RFC base
`f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. The RFC0064 product change is
`ca51d5df7` (BPR-1316). The export-location sources (chooser, composition
controls, native output admission and the directory resolver) are unchanged
from it at the current head. Combined source `980c08873` changed only the
diagnostic snapshot content, so the export courses were replayed at
`3836d348666a4f797e5f4fe20e484b84e5603f8f` in `build/dev`:
`application_release_maintenance`, `application_release_maintenance_public_path`
(10.70 s) and `qt_release_maintenance_scale_{100,125,200}` pass 5/5 in 15 s.
[[BPR-1317]] records the owner's native macOS Accept (decision
`4ac53a5e-efe7-4b3a-851e-1569b0bbe503`, session
`5c434605baf017ea6f02e2e02ef6763f`). Its dependency RFC-BPR-0027 is
Auto-Verified.

Every criterion passes for macOS, the only claimed platform. Windows and Linux
native chooser behavior is not claimed; the Windows branch keeps its existing
owner/ancestor admission without native qualification. RFC-BPR-0064 and its
index row are set to Auto-Verified, and the
[ledger](../../verification/RFC-BPR-0062-0065.md) records the closeout.
