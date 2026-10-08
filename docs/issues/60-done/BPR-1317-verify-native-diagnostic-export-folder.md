---
schema: "implement-rfc.card.v1"
id: "BPR-1317"
title: "Verify the native diagnostic export chooser location"
status: "Done"
rfcs: ["RFC-BPR-0064"]
depends_on: ["BPR-1316"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"ca51d5df7ed9564a44f971ae792956a093f60965","evidence":{"kind":"command","detail":"Exact frozen source/build passes all5 export courses14.08s plus core/architecture3/3 through coordinator14.50s. Dev+Qt-free native output, controlled fallback/no-discovery-write and full signed-out public routes pass; format/all touched-TU tidy/docs pass. Byte/cancel/overwrite/stale courses retain original contract."}},{"criterion":"C2","outcome":"pass","revision":"ca51d5df7ed9564a44f971ae792956a093f60965","evidence":{"kind":"observation","detail":"Owner Accept, decision 4ac53a5e-efe7-4b3a-851e-1569b0bbe503 (2026-10-08T04:35:01Z), handed back at cursor 5, session 5c434605baf017ea6f02e2e02ef6763f, candidate BPR-1317-native-folder-mac-v001 fingerprint 1b0f1529234f1234e1a7cad68e3d7d9d9dacd7a49afcab3a50bb720b54726813 (source ca51d5df7 + binary 8d7093a3 + launcher a57da1bd). Single criterion: on macOS the actual native Save chooser starts in OS Downloads (or admitted Documents/home fallback) through Help and Settings in both export modes. Claimed platform is macOS only; no Windows or Linux native qualification is claimed."}}]
---

# Verify the native diagnostic export chooser location

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Focused controlled-folder and composed export courses cover AC01–AC04 including redirected/localized locations and disk-root launch.
- C2: Actual native Save chooser shows the proposed location on each claimed platform; automated chooser state does not substitute for native observation.

## Owner native verdict (2026-10-08)

The owner accepted candidate `BPR-1317-native-folder-mac-v001` (fingerprint
`1b0f1529…6813`, source `ca51d5df7` with frozen binary SHA-256 `8d7093a3…3273`)
in session `5c434605baf017ea6f02e2e02ef6763f`: decision
`4ac53a5e-efe7-4b3a-851e-1569b0bbe503`, handed back at cursor 5 and consumed by
dispatch `parallel-integrator-r5-001`. The question asked only whether the
actual macOS native Save chooser starts in OS Downloads, or the admitted
Documents/home fallback, through Help and Settings in both export modes. C2
passes for macOS, the only claimed platform. No Windows or Linux native
chooser qualification is claimed. The export-location source is unchanged
from `ca51d5df7` at the current head.
