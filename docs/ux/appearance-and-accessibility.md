# Appearance and Accessibility

## Architectural ownership

Appearance is intentionally outside the functional core. The host owns settings persistence, resource policy, scheduling, and caches. The Qt presentation adapter owns system-scheme observation, fonts, colors, layout, syntax presentation, accessibility, previews, and visual-state composition. Appearance changes publish bounded presentation invalidations rather than changing review semantics.

## Appearance modes

```text
System
Light
Dark
```

System mode follows the operating-system color scheme and chooses the corresponding variant from the selected BetterPullReview profile. High-contrast system preference selects or recommends a high-contrast variant.

Built-in profiles:

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
