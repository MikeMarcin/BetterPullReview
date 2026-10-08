---
schema: "implement-rfc.card.v1"
id: "BPR-1315"
title: "Verify and close RFC-BPR-0063"
status: "Done"
rfcs: ["RFC-BPR-0063"]
depends_on: ["BPR-1314"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
closeout_for: ["RFC-BPR-0063"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"5136398e39f94b85969785a5ec707a2ab6866316","evidence":{"kind":"command","detail":"Initial long/short/empty allocation: qt_pull_request_overview_page, qt_composed_markdown x3, application_overview_summary x3 pass in clean export of 5136398e3 (13/13 with boundaries); product 3e7376512; owner native review v001-v004, Accept e4b2ef3c."}},{"criterion":"AC02","outcome":"pass","revision":"5136398e39f94b85969785a5ec707a2ab6866316","evidence":{"kind":"command","detail":"Pointer/keyboard resize, reset by menu and double-click, accessible name/value, narrow heights at 100/125/200 in overview page and public course rows; owner native v001 course exercised grip/keyboard/reset without objection. VoiceOver announcements not observed."}},{"criterion":"AC03","outcome":"pass","revision":"5136398e39f94b85969785a5ec707a2ab6866316","evidence":{"kind":"command","detail":"qt_markdown_body x3 and qt_composed_markdown x3: Select All copy returns original Markdown, source selection bytes exact, no HTML MIME; public course independent description/Files scrolling without outer scroll."}},{"criterion":"AC04","outcome":"pass","revision":"5136398e39f94b85969785a5ec707a2ab6866316","evidence":{"kind":"command","detail":"application_overview_summary x3: refresh, full-view round trip, per-window restart preference, PR replacement clears old content."}},{"criterion":"AC05","outcome":"pass","revision":"3e7376512:1e7a5c1bd9f4c1169fc45b7bc53db4ef69ba826acbf8d0f2bea85ff7b2d50e6a","evidence":{"kind":"observation","detail":"Public course file activation reaches a real composed comparison without provider writes; owner Accept of frozen v004 on native macOS, session 98de418935ba1aae897bf1379f9771cc decision e4b2ef3c-5806-44b9-8c9e-50174cfec4ce (BPR-1314 C2). Windows/Linux native not claimed."}}]
---

# Verify and close RFC-BPR-0063

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

RFC base: `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** A long description initially occupies the defined larger share of normal Overview, with Files and the side summary visible; short and absent bodies follow the content-aware behavior instead of the four-line cap. | Overview page, composed Markdown and composed public course at 100/125/200%; owner native review of v001–v004 | Pass on macOS | `3e7376512`; rerun at `5136398e3` |
| **AC02:** Pointer and keyboard resizing change the normal description/Files allocation across the legal range, with discoverable reset, accessible state and usable controls at narrow heights and 100–200% display scale. | Overview page and public course rows (pointer, Up/Down, reset by menu and double-click, accessible name/value, narrow heights, three scales); owner native v001 review exercised grip, keyboard and reset without objection | Pass on macOS; VoiceOver announcements not observed | `3e7376512`; rerun at `5136398e3` |
| **AC03:** The complete admitted description is reachable, selectable and copyable in the resized pane using either native Markdown or source fallback; description and Files scrolling remain independent without an outer scroll trap. | Markdown body and composed Markdown rows at three scales (Select All copy returns original Markdown, source bytes exact, no HTML MIME); public course independent scrolling | Pass | `3e7376512`; rerun at `5136398e3` |
| **AC04:** Refresh, reflow and full-description round trips preserve the defined reading/file state and size choice; restart restores only that window's preference, and authority replacement cannot show old PR content. | Composed public course (refresh, full-view round trip, restart per window, PR replacement) at three scales | Pass | `3e7376512`; rerun at `5136398e3` |
| **AC05:** The composed application retains file activation, Start/Continue review and full-description return semantics without unnecessary model resets or provider writes; the owner judges the changed Overview in a fresh short attended session using a frozen build. | Public course file activation reaches a real composed comparison; owner Accept of frozen v004 (BPR-1314 C2) | Pass on macOS | `3e7376512`; rerun at `5136398e3` |

## Closeout (2026-10-08)

Closed by dispatch `parallel-integrator-r5-001` against RFC base
`f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. The RFC0063 product commits are
`f9c243018` and the BPR-1313 persistence (resizable summary and its saved
size), `c4e49f971` (composed public course), `1c5c28ce9` (reader space, View
source in the context menu, Select All copies the original Markdown),
`9fa43a9b3` (toggle in the reader's corner) and `3e7376512` (Exit full view
in the Enter style). The owner rejected v001–v003 and accepted v004 in
session `98de418935ba1aae897bf1379f9771cc` (decision
`e4b2ef3c-5806-44b9-8c9e-50174cfec4ce`, recorded on [[BPR-1314]]). The v001
review asked for the full native course (grip, Up/Down, reset, scrolling,
full view, files); its rejection and the later ones concerned only the
full-view control and its space, all addressed.

Later commits touched code hosting the Overview, so the ten focused rows were
rerun in a clean export of `5136398e39f94b85969785a5ec707a2ab6866316`. They
pass 13/13 with the core and architecture boundaries. The operator
specification (`docs/ux/workbench-specification.md`) describes Resize summary
and Enter/Exit full view.

Limits: VoiceOver announcements and Windows/Linux native behaviour are not
claimed. Accessible names and values are covered only by Qt checks. RFC-BPR-0063 and its
index row are set to Auto-Verified, and the
[ledger](../../verification/RFC-BPR-0062-0065.md) records the closeout.
