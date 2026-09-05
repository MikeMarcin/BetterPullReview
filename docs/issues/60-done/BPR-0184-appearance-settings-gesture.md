---
schema: "implement-rfc.card.v1"
id: "BPR-0184"
title: "Qt: the appearance settings gesture"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0183"]
---

# Qt: the appearance settings gesture

RFC-BPR-0006 puts "translates settings gestures" among the Qt adapter's
responsibilities. There was no gesture: `appearance_profile::select_mode` was
reachable only from tests, so no person could choose an appearance mode, a
font, or a tab width.

## What this slice did

`appearance_settings_panel` offers mode, both font families, sizes, weights, UI
line height, tab width, ligatures, and the two syntax emphasis switches, and
turns each gesture into an `application::appearance_intent` the appearance host
adopts. The composition wires the panel's dispatch to the host and routes the
host's published presentation change back into the profile, so a gesture moves
the record and the screen together.

### The gesture goes to the host, never to presentation

The tempting shortcut was to call `appearance_profile::select_mode` from the
widget: it is one line and the screen changes immediately. It would also change
the screen **without changing the record**, so the next launch would silently
disagree with what the reviewer saw.

So the panel dispatches into the host, the host validates and persists, and
presentation is driven by what the host *published* rather than by what was
asked for. An intent is a request; only the host knows what was accepted, and
offering the intent to presentation would show a refused value as though it had
applied.

That route was missing before this card in a way nothing noticed:
`AppearanceProfile` was built from the host's settings once at construction and
never heard from it again. `select_other_appearance_mode`, added for BPR-0185's
probe, moved the host and left the window alone.

### Rejections are shown, never repaired

`report` puts every control back to the last adopted record and names each
violation by target, field, and reason. Restoring happens *first*, because a
reviewer told their value was refused must be looking at the value actually in
effect.

### Echo suppression is load-bearing

A control set programmatically emits the same signal a reviewer's gesture does.
Without suppression, adopting the host's answer would dispatch the intent that
produced it, and a refusal would immediately re-offer the record that was just
refused. `SuppressedEchoes` is asserted non-zero so the mechanism is exercised
rather than merely present.

### The controls cannot express most illegal values

Numeric ranges come from `core::bounds_of`, the same function the validator
enforces with. That is deliberate — and it means the refusal path is reachable
from this surface by exactly one field, the family, which the core bounds by
byte length rather than by value. That is the field both the component test and
the composed probe drive.

## A false marker, found and removed

The first version of the composed evidence reported
`APPEARANCE_ON_ENTRY:<mode>:<tab>`, sampled immediately after
`AppearanceHost->restore()` and documented as "what the durable record held
when this process started".

It was wrong. `restore()` is an effect: it returns before the record has been
read. The marker reported `system` on a process whose window was already
showing the stored dark palette — measured, not reasoned about. A marker that
names the wrong moment is worse than no marker, because it reads as evidence.

Replaced with `APPEARANCE_SETTINGS:<mode>:<tab>`, the settings actually in
effect. The restart claim survives the correction, because a composition starts
from `system` and a tab width of four: a run that makes no gesture and reports
anything else read it from the durable record. The contract's first phase
establishes that default explicitly, so the claim does not rest on a reader
knowing it.

## What was measured

| Claim | Evidence |
|---|---|
| Every setting is reachable and translates to an intent | `qt_appearance_settings_panel` drives each control and reads the dispatched intent |
| The mode control offers the core's vocabulary | Built from `core::appearance_mode_count` and asserted against it, not against a restated list |
| A gesture previews without a restart | `APPEARANCE_GESTURE_OK` — judged on the window's palette, which a host-only check would not catch |
| A gesture survives a restart | Three phases of the real binary: `system:4` fresh, `dark:8` after the gesture, `dark:8` from a separate process against the same root |
| A refused value changes nothing | `APPEARANCE_REFUSAL_OK` — the record is unchanged, the surface reports a typed reason, and the control shows the value in effect |
| The two validators agree | `APPEARANCE_DIVERGENCE:0`, asserted on every phase |

### Non-vacuity

Each break compiled; the error count was checked before the failure was
believed.

| Break | Failure |
|---|---|
| Remove echo suppression | `showing what the host published dispatches nothing` |
| `report` no longer restores the controls | `a refused control is put back to the value that is in effect` |
| Restate the mode list as a fixed five | `the mode control offers exactly the modes the core declares` |
| Unwire the panel's dispatch in the composition | `settings gesture omitted APPEARANCE_GESTURE_OK` |
| Cut the route from published presentation to the profile | `settings gesture omitted APPEARANCE_GESTURE_OK` |

## What this card does NOT deliver

- **No palette customisation.** `appearance_intent` is a closed set of two, and
  a colour gesture is not in it. No card has designed that surface, and an
  intent nothing sends would make the vocabulary a plan rather than a contract.
  `core::import_theme` exists and no path reaches it.
- **No font picker.** The family is a line edit over an opaque platform
  identifier the core never interprets. A reviewer must know the family name.
- **No live preview of an illegal value.** A refused record leaves the previous
  one in effect, which is correct, but the reviewer sees the refusal only after
  the field loses focus.
- **The settings live in the main window's scroll area**, not in a preferences
  window. That is a placement decision BPR-0191 will revisit when the workbench
  shell becomes the application's window.

## Required direct evidence

- Every listed setting is reachable, changes the composed presentation without
  a restart, and survives one. **Met**, by the panel suite and the three-phase
  contract.
- A value the core rejects is refused whole, the typed reason is shown, and the
  previous value stays in effect — asserted on the composed surface, not on the
  intent. **Met**: `APPEARANCE_REFUSAL_OK` compares the host's settings record
  before and after, reads the surface's status text, and reads the control.
- Every gesture translates to an intent; no widget path mutates presentation
  directly. **Met**: the panel holds no `appearance_profile` and cannot; the
  composition is the only thing that touches presentation, and only from a
  publication.
- The mode control offers all six modes in `core::appearance_mode`, and the
  test reads the vocabulary from the core. **Met**, and `mode_text` has no
  `default` case, so a seventh mode is a compile error rather than a control
  that quietly offers six.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered. **Met**, tabulated above.

## What this unblocks

BPR-0114's explicit-mode half. There is now a way to select an explicit mode
and a durable record that remembers it. The remaining half — that a live
operating-system scheme change moves System and leaves an explicit mode alone —
still needs a person at the keyboard, and the card stays in `backlog/` for
that reason alone rather than for want of a mode to select.
