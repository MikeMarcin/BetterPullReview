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

## Appearance modes (RFC-BPR-0006)

```text
System
Light
Dark
```

RFC-BPR-0006 will add user-selectable appearance profiles. Its System mode
will follow the operating-system color scheme and choose the corresponding
variant from the selected BetterPullReview profile. High-contrast system
preference will select or recommend a high-contrast variant.

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

The current GitHub window publishes one combined polite announcement for a
status and diagnostic presentation update. Repeated identical text is
deduplicated so one accepted projection does not become a burst of field-level
announcements. Connection and technical facts hidden by disclosures remain
available through named keyboard-operable controls.
