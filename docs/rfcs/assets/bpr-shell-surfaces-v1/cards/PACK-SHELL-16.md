---
schema: "implement-rfc.card.v1"
id: "PACK-SHELL-16"
title: "Rebuild the Appearance page as theme strip, type rows with preview, and colours for the theme on screen"
status: "Backlog"
rfcs: ["RFC-BPR-0034"]
depends_on: ["PACK-SHELL-15"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"appearance","acceptance":["C2","C3","C4","C5"],"alias_requires_live_allocation":true}
---

# PACK-SHELL-16: Rebuild the Appearance page as theme strip, type rows with preview, and colours for the theme on screen

## Outcome

Lay the six preview cards in a sideways-scrolling well with the wider snippet, the diagonal System split by clipping, resolution and customised-count captions and keyboard selection, and remove the mode combo box; turn the typography group boxes into eleven one-line rows with an `info_glyph` (popover on hover and focus, accessible description) and a labelled `type_preview` painted by the review window's painter; replace the role combo box and hex field with a `colour_role_list` (filter, four sticky groups, swatch, name, value, platform picker with live preview) titled for the theme on screen, with Import, Export and Reset all in its header and one Keep changes and Cancel bar shown only while previewing.

## Acceptance

- Tile inventory, order, scroll, keyboard selection and both System halves match the built-in palettes pixel-for-pixel where asserted; the combo box is gone.
- Every Type row is one line; each consequence is reachable by hover, focus and accessible description; the preview repaints after each accepted change with the expected runs.
- The colours list holds 47 roles in four groups; the picker previews live and Keep changes runs the existing commit; the bar exists only while previewing.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
