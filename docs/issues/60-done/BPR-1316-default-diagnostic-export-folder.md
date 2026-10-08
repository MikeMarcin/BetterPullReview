---
schema: "implement-rfc.card.v1"
id: "BPR-1316"
title: "Resolve and present a user-owned diagnostic export default"
status: "Done"
rfcs: ["RFC-BPR-0064"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"ca51d5df7ed9564a44f971ae792956a093f60965","evidence":{"kind":"observation","detail":"Production QStandardPaths Downloads/Documents/home resolves on composition effect lane using shared readonly output-parent admission. Controlled localized/redirected, missing/empty, unwritable/unsafe and total-unavailable observations pass; no directory/file/permission mutation. Qt retry retains preview."}},{"criterion":"C2","outcome":"pass","revision":"ca51d5df7ed9564a44f971ae792956a093f60965","evidence":{"kind":"command","detail":"Dev public composition passes 9.03s: signed-out Help/Settings both modes with disk-root cwd, fresh initial path despite chooser history, exact suggested name, alternate frozen bytes, cancel/overwrite/stale refusal; Qt100/125/200 and SQLite adapter pass. Core-only output pass0.43s. Format, all6 touched-TU warnings-as-errors tidy, docs gate+67 tests pass. Actual native chooser remains BPR1317C2 pending."}}]
---

# Resolve and present a user-owned diagnostic export default

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Composition resolves Downloads, Documents then home using standard-folder discovery and existing output policy; total unavailability keeps preview with typed retry.
- C2: Both privacy modes and Help/Settings open the chooser with the selected folder and suggested filename; alternate selection/cancel/overwrite/stale refusal preserve exact preview semantics and create no folder/file during discovery.
