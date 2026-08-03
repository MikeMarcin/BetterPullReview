---
id: BPR-0097
title: Implement epoch selectors, focus, accessibility, and UI automation
status: Done
rfc: RFC-BPR-0005
depends_on: [BPR-0096]
---

# Implement epoch selectors, focus, accessibility, and UI automation

Wire the independent left/right epoch selectors showing labels and exact OIDs over arbitrary retained epochs, plus keyboard command coverage, focus order, accessibility metadata, and explicit loading, failure, cancellation, and empty states.

UI automation proves navigation, selection synchronisation, focus order, state transitions, and restart persistence, and proves that actions translate to intents and consume incremental projections only.

## Implementation

The epoch placeholder is retired. The centre composes RFC-BPR-0003's
`epoch_comparison_control` unchanged above RFC-BPR-0004's `diff_workbench`, the
same compose-rather-than-rebuild move the centre already used. That control
already owns independent left and right selectors over arbitrary retained
epochs, complete copyable OIDs, and the loading, failure, cancellation, and
empty wording, so no second epoch authority or state vocabulary was introduced.

The window now also implements `epoch_projection_port`, exposes
`epoch_selectors()` and `focus_order()`, and quotes the composed control's
decided review-history sentence rather than restating it.

Focus order is declared and re-applied on every toolbar mode change, anchored
to the never-rebuilt overflow button so a rebuild cannot drift it. It
deliberately never reorders a stop inside a composed region: the epoch control
and comparison surface keep their own internal order, and the surface
contributes exactly one stop.

## Defects found and fixed

Three of the four production changes were forced by real defects that were
already shipped, not by preference.

**Three accessible-name collisions, all live.** The retired placeholder was not
the problem; its name simply disappeared. The real collisions were elsewhere.
The toolbar's change buttons were named `Previous change` and `Next change`,
identical to `diff_workbench`'s own in-surface buttons — two focusable widgets
per name, shipped by BPR-0092 and BPR-0094. The drawer toggles answered to
`Files drawer` and `Conversations drawer`, identical to the drawer panels' list
views. The overflow menu shared `Review navigation` with the button that opens
it. All three are now distinct; visible labels are unchanged.

**An ambiguous shortcut that disabled both bindings.** The drawer toggles held
`Alt+1` and `Alt+2` as window shortcuts while the composed control holds
`Alt+1..3` with widget-with-children context. Both match whenever that control
has focus, and Qt resolves an ambiguous overload by triggering *neither*. The
toggles moved to `Alt+Shift+1` and `Alt+Shift+2`, and a test now forbids any
duplicate key sequence anywhere in the assembled window. This is the kind of
defect that produces a silent dead shortcut rather than a visible error.

Composing the control also starved the drawers of their published width through
a raised centre minimum, fixed with the horizontal size policy the drawer
panels already use, which additionally protects BPR-0092's recorded constraint
that the window minimum stay below the compact threshold.

## Direct verification

Four cases registered: exact retained-epoch OIDs, explicit review-history state
in text, declared and walkable focus order, and selection synchronisation with
restart persistence.

- `qt_workbench_window` passes 3/3 in 3.2 s, the full Qt suite 24/24 in ~21 s,
  both architecture boundary suites, the format gate, and strict `clang-tidy`
  with `--warnings-as-errors`.
- Non-vacuity, exact OID: forwarding a publication whose pair OIDs keep only
  their first four bytes — the eight-character abbreviation — fails with "each
  selector states the exact complete commit OID of its selected anchor". A
  prefix-only assertion would have passed that break, so the rule is the sharp
  form rather than the convenient one.
- Non-vacuity, explicit states: replacing the quoted control text with a
  constant fails with "loading, failure, cancellation, empty, and retained
  review history each read differently in text rather than differing only in
  colour or enabled state".
- Non-vacuity, collision rule: reverting the toolbar button's accessible name
  to `Previous change` fails with the offending name and widget class quoted in
  the message. Reproduced independently after delivery, with the patch
  confirmed applied before the red was believed.

## Open composition question

The composed control brings its full review-history surface, including
RFC-BPR-0003's confirmed-destructive maintenance menu for prune and cache
deletion. That menu is unchanged and untriggered here, but it is now reachable
from the workbench shell. Whether the shell should expose only the selector
pair is a composition decision for the simplification and owner-attended cards,
not one to settle inside this slice.

## Retained risks

- The uniqueness scan covers visible focusable widgets. Qt always constructs a
  hidden zero-size first splitter handle, and the comparison splitter makes
  those handles link-map instances, so two widgets share a name with one
  permanently unreachable. The exclusion is stated as part of the rule rather
  than slipped past, and the owning file is outside this slice.
- Some composed widgets outside this scope carry an accessible name without a
  description, or rely on button text at the accessibility bridge. That is why
  the composed surface contributes only one declared stop.
- Shortcut delivery remains proven structurally rather than by a synthesised
  keypress; Qt Test is not linked in this repository.
- The horizontal size policy means the epoch region clips rather than reflows at
  very narrow widths. Readability at supported sizes is the owner-attended
  BPR-0100 gate.

## Result

The workbench presents independent epoch selectors with exact copyable OIDs
over arbitrary retained epochs, explicit textual states, a declared focus order,
and unique accessible names and key sequences. Three live accessible-name
collisions and one silently dead shortcut pair were found and fixed in the
process. BPR-0097 is complete.
