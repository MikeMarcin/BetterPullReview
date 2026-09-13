---
schema: "implement-rfc.card.v1"
id: "PACK-SHELL-17"
title: "Verify and close RFC-BPR-0034"
status: "Backlog"
rfcs: ["RFC-BPR-0034"]
depends_on: ["PACK-SHELL-10", "PACK-SHELL-11"]
closeout_for: ["RFC-BPR-0034"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["C1","C2","C3","C4","C5","C6"],"alias_requires_live_allocation":true}
---

# Verify and close RFC-BPR-0034

RFC base: `0000000000000000000000000000000000000000`.

This card tracks formal closeout for its RFC. The checklist rows are the canonical
criteria of the live RFC; every row records the check run, its result and the commit.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **C1 — One customisation per theme.** The store keeps a customised palette per variant; migration files the existing palette under its recorded variant with nothing lost; overrides apply only while their variant is resolved; committing changes only the theme on screen; Export, Import and Reset all act on the theme on screen and say so; customised counts equal the roles that differ from the built-in palette. | Not run; import only | No producing revision |
| **C2 — The theme strip.** Six tiles, one per mode, in a sideways-scrolling well with a thin scrollbar and Left and Right selection; each painted by the existing preview painter from its built-in palette plus its overrides with the wider snippet; both System tiles split diagonally by clipping with the half in effect named in the caption; customised counts captioned; the mode combo box absent. | Not run; import only | No producing revision |
| **C3 — Type rows.** Eleven one-line rows of label, info glyph and control with sizes in points, weight as a 100 to 900 stepper, tab width as a 2, 4, 8 segment and switches for ligatures and emphasis; every consequence reachable by hover, by keyboard focus and as the glyph's accessible description; Reset in the card header. | Not run; import only | No producing revision |
| **C4 — The preview.** A labelled preview painted by the review window's painter from the accepted typography and the theme on screen, whose six code lines show an italic comment, bold keywords, a type, a function, ligature pairs, two tab depths with markers, removed and added lines, a selection, a macro and the current line, with a caption stating the values in effect, repainting on every accepted change. | Not run; import only | No producing revision |
| **C5 — The colours card.** Titled for the theme on screen with its subtitle; Import, Export and Reset all in the header; a filtered, grouped, scrolling list of all 47 roles with sticky headers, swatch, name and value; a swatch opens the picker and previews live; edited rows show new and old; one bar with Cancel and Keep changes appears only while previewing; the role combo box, hex field and separate preview buttons are absent. | Not run; import only | No producing revision |
| **C6 — Rendered and reachable.** The page in its default, editing and light-desktop states, with every tile, renders at 100, 125, 150 and 200 percent in the four appearances, every control reachable by keyboard with an accessible name and the tiles' captions announced, and the owner accepts the real macOS application through the attended-review workflow. | Not run; import only | No producing revision |

## Outcome

Reconcile every C row with direct evidence or a named unresolved criterion, run one integration pass over the appearance host, store migration and composed Settings courses, and record the truthful formal status.

## Acceptance

- All C1–C6 rows reference direct evidence or a specific unresolved criterion.
- One integration pass over the appearance migration, host and composed Settings courses is recorded with its revision.
- Formal closeout obeys canonical order and actual platform and attended results; RFC-BPR-0006 is not reopened.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
