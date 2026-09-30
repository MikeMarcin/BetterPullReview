---
schema: "implement-rfc.card.v1"
id: "BPR-1156"
title: "Deliver the native focused suggestion editor"
status: "Parked"
rfcs: ["RFC-BPR-0055", "RFC-BPR-0061"]
depends_on: ["BPR-1154", "BPR-1155"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-07", "RC55-09", "RC61-04"], "pack_alias": "PACK-CYCLE-14", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC55-07","outcome":"pending","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes on macOS arm64 (core_suggestion_text_editing, qt_suggestion_editor_1_0/2_0): Alt+Shift+Down x2 then Shift+End one caret per line to its own end, typing at every caret with CRLF kept, undo, virtual space, Tab, box copy/paste with marker, cut, Alt+click, IME preedit and commit. Owner judgment pending: attended item BPR-1156-editor-gestures-v001 on the frozen candidate (revision c2a0df54a)."}},{"criterion":"RC55-09","outcome":"pass","revision":"0d0872fbd5bfce9f3ff405b42ad649b7ed5ea5cb","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Card share: the composer hands exact replacement bytes, intent and rationale to its owner with no model, summarizer or second prompt and no file write (qt_suggestion_editor, application_review_cycle_desktop_public_path). Round preparation is BPR-1158."}},{"criterion":"RC61-04","outcome":"pending","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes: composer prefilled with exact selection, before/after in context, newer-material stated without moving; related location added from a native selection in another file with the primary kept; ambiguous/deleted/unmapped correspondence stated. Owner judgment pending: BPR-1156-composer-layout-v001."}}]
---

Pack alias: **PACK-CYCLE-14** (live `BPR-1156`, allocated at import).

# BPR-1156: Deliver the native focused suggestion editor

## Outcome and bounded work

Integrate prefilled Suggest change into the shared native composer with source/range identity, before/after preview, ordinary undo and IME. Inventory actual multi-caret/virtual-space component; implement only missing narrow editing behavior, not an IDE or renderer replacement.

## Acceptance and direct evidence

Attended/automated native tests for Alt+Shift+Down then Shift+End, virtual space, box/multi-caret copy/paste, tabs, CRLF, deletion, undo, IME and stale-source refresh while composing; no checkout writes or model call.

Criteria: RC55-07, RC55-09, RC61-04.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `2631bcf31`, and the fix `384525fb9`. The Qt rows run as registered: `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.

Evidence only; the card stays parked for the owner. `core_suggestion_text_editing`, `qt_suggestion_editor_1_0` and `_2_0`, and `application_review_cycle_desktop_public_path` pass. The editor course drives the editor with key events and checks focus offscreen. That is automated keyboard and focus evidence on Linux, not a native xcb or Wayland session, and it does not answer the owner's feel question.

The build is clean under GCC with no warnings. The logs are station records in `build/logs/bpr-station-authoring/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `2631bcf3`, with `f42d4e3a` and the Linux station's `384525fb`,
`build/windows-dev` (MSVC 19.51) builds with no warning. `f42d4e3a` renamed
two locals in `suggestion_editor.cpp` for C4458. `384525fb` fixed the
course's read of an emptied optional in `review_cycle_workbench`, the same
fix this station had made. The authoring rows pass offscreen: the six core rows,
`sqlite_review_cycle_storage`, `review_cycle_{feedback,suggestion,workbench}`,
`qt_suggestion_editor_*`, `qt_review_cycle_view_*` and
`application_review_cycle_desktop_public_path`. The four new uncontained pins
are measured and confirmed. The details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, authoring".

No native Windows attended run was made. This entry adds evidence only, and the card stays Parked.
