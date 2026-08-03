# Appearance and Accessibility

## Architectural ownership

Appearance is intentionally outside the functional core. The host owns settings persistence, resource policy, scheduling, and caches. The Qt presentation adapter owns system-scheme observation, fonts, colors, layout, syntax presentation, accessibility, previews, and visual-state composition. Appearance changes publish bounded presentation invalidations rather than changing review semantics.

## Current system-native presentation

The RFC-BPR-0002 GitHub window currently follows the palette, style, and
interface font supplied by Qt and the operating system. It does not install a
light or dark palette, select a style, apply QSS, or replace the application
font. Derived title and status fonts, standard icons, delegate geometry, and
layout are refreshed when Qt reports application palette, font, style, or
theme changes.

The conversation delegate uses the live style option for colors and fonts,
lets the native style paint selection and focus, and wraps remote literal
text without elision. Color is not used as the only state signal.

Direct macOS evidence records startup under the system-default Dark
appearance with QMacStyle. An earlier safe loaded-window exercise changed the
application-local color scheme from Dark to Light and the application font
from 13 to 17 points, with the corresponding live events handled. The
owner-authorized native gate then started the real application under system
Light and observed the same process follow an operating-system-global change
to Dark. It independently observed the global preference, retained
QMacStyle, focus, selection, collapsed conversation state, model identity,
geometry, and device-pixel ratio, persisted distinct inspected rasters, and
restored the original Dark setting. Offscreen forced-style renders at
controlled scales are layout evidence only and cannot certify native
system-appearance integration.

## RFC-BPR-0003 comparison control

The reusable epoch/comparison control follows the same native-System contract.
It inherits the active application palette, native style, and interface font;
installs no forced palette, style, QSS, or custom theme; and refreshes derived
geometry and presentation roles on application palette, font, style, or theme
changes. Its exact OID fields, unavailable reasons, focus, selection, and
accessible relationships remain legible in both actual system Light and Dark.

Epoch rows are stable projections identified by authority, catalog
incarnation, and epoch identity. Routine append, prune, availability, cause,
and selection changes use exact insert/remove/move/data-change signals rather
than resetting the model. The row presents the primary cause together with the
independent base/head transition facts, including explicit unknown lineage.
Both selectors have label/buddy relationships and keyboard actions; the full
OID fields are named, read-only, and copyable. Unavailable aliases remain
visibly unavailable and do not fall back to a different resident epoch. One
bounded polite status-plus-comparison announcement is emitted for a changed
projection and identical text is suppressed. Color is not the only signal for
any of these states.

The bounded history control exposes separate `Load earlier epochs` and `Return
to newest epochs` actions with keyboard shortcuts and enabled-state
descriptions. When a selected anchor is outside the one-page working set, the
selector's accessible description identifies that condition and the complete
OID remains available below it. A catalog-invalidated page is retained with
explicit stale text and disabled selection rather than being silently mixed
with the new revision.

Local destructive operations are grouped in one secondary, keyboard-focusable
`Local history maintenance` menu instead of adding three prominent buttons to
the interim control. Its actions and confirmation controls have stable
accessible names and descriptions. Each warning is window-modal, names the
exact consequence in text, and makes Cancel both the default and Escape action.

The deterministic 1.0x, 1.25x, and 2.0x offscreen renders exercise the actual
loaded pull-request composition and verify geometry only. BPR-0049's native
palette evidence and BPR-0059's owner-activated Cocoa/QMacStyle run directly
cover the target-native macOS System Light-to-Dark behavior. The latter retains
the active focused widget, exact epoch identities and OIDs, Whole PR selection,
readable palettes, accessibility state, geometry, and zero model resets in the
same process; its distinct inspected 2200x1440 rasters are transient local
evidence, and the owner's original System Dark setting is restored afterward.
That run is bound to the source it executed rather than to the current tree;
its exact invocation, source identity, raster hashes, host identity, and the
limits of its attestation are recorded in the
[RFC-BPR-0003 probe provenance](../verification/RFC-BPR-0003.md#owner-activated-macos-appearance-probe-provenance).
Owner-authorized System-theme evidence for the later selected-file workbench
composition belongs to RFC-BPR-0004's BPR-0083 and is not a reproduction of it.

This inheritance and incremental-behavior proof is not the final-workbench
appearance gate. RFC-BPR-0005 retains a distinct integrated-workbench
presentation-polish and native visual-acceptance slice. RFC-BPR-0006 owns
user-selectable themes, fonts, syntax presentation, and high-contrast
profiles.

## Appearance modes (RFC-BPR-0006)

```text
System
Light
Dark
```

RFC-BPR-0006 will add user-selectable appearance profiles, with a fresh
installation defaulting to System. System mode will follow the
operating-system color scheme and choose the corresponding variant from the
selected BetterPullReview profile. High-contrast system preference will
select or recommend a high-contrast variant.

Planned built-in profiles:

- BetterPullReview Light
- BetterPullReview Dark
- BetterPullReview High Contrast Light
- BetterPullReview High Contrast Dark

The built-in dark profile uses a charcoal editor surface, muted green comments, crisp blue/cyan identifiers and control tokens, warm gold/orange accents, pale primary text, and low-chroma red/green diff backgrounds inspired by the readability of Visual Studio and Visual Assist dark configurations without copying a proprietary theme.

## Fonts

Separate settings exist for interface and code fonts. Code settings include family, point size, weight, line height, tab width, ligatures, syntax bold, and syntax italics. The fixed-width system font is the default.

Runtime code zoom preserves the top visible source line and does not silently overwrite saved font settings.

## Colors

Theme customization exposes semantic roles, not arbitrary QSS or executable styles:

- Application surfaces and text
- Code background, gutter, line numbers, selection, current line
- Added, removed, intraline added/removed, moved, hunk header
- Unread, accepted, defect, fixed, verified, stale
- Syntax token categories

Color is never the only state signal. Icons, labels, gutter marks, line prefixes, outlines, and accessible names provide redundant meaning.

## Syntax highlighting

Use KSyntaxHighlighting through a custom low-level highlighter. Highlight complete old and new blobs independently so hunks beginning inside multiline constructs receive correct state. Cache semantic spans by blob and definition, independent of the active color theme.

Package audited definitions and themes as resources. Disable filesystem discovery and downloads in the production build. User color customization changes BetterPullReview semantic palettes, not grammar definitions.

## Accessibility

- Complete keyboard traversal and visible focus
- Accessible names, roles, states, and relationships for all custom-painted controls
- Screen-reader representation for diff rows, change groups, comments, and actions
- High-DPI and fractional-scaling tests
- Reduced-motion behavior
- Minimum supported contrast checks and warnings in the theme editor
- Keyboard-equivalent access to every pointer action

### Workbench focus order and accessible naming (RFC-BPR-0005)

The workbench shell declares its focus order explicitly and re-applies it
whenever the toolbar changes between expanded and compact mode. The order is
anchored to a control that is never rebuilt, so a toolbar rebuild cannot make
it drift.

The declared order never reorders a stop that belongs to a composed region. The
epoch control and the comparison surface keep their own internal focus order,
and the comparison surface contributes exactly one stop to the shell's order.
A shell that reshuffled another component's internal order would be deciding
that component's presentation for it.

**No accessible name may be shared by two focusable widgets in the assembled
window.** This is enforced by a test rather than by convention, because the
rule was broken four separate times before it was enforced: a retired
placeholder duplicated the composed detail pane, the toolbar's change buttons
duplicated the comparison surface's own in-surface buttons, the drawer toggles
duplicated the drawer panels' list views, and the overflow menu duplicated the
button that opens it. Each case left two focusable widgets answering to one
identity, which assistive technology cannot disambiguate. Visible labels may
repeat; accessible names may not.

The same scan forbids duplicate key sequences, for the related reason that Qt
resolves an ambiguous shortcut by triggering neither binding, so a collision
presents as a control that silently does nothing rather than as an error.

The current GitHub window publishes one combined polite announcement for a
status and diagnostic presentation update. Repeated identical text is
deduplicated so one accepted projection does not become a burst of field-level
announcements. Connection and technical facts hidden by disclosures remain
available through named keyboard-operable controls.
