# Appearance and Accessibility

## Architectural ownership

Appearance is intentionally outside the functional core. The host owns settings persistence, resource policy, scheduling, and caches. The Qt presentation adapter owns system-scheme observation, fonts, colors, layout, syntax presentation, accessibility, previews, and visual-state composition. Appearance changes publish bounded presentation invalidations rather than changing review semantics.

## Dif asset appearance and accessibility boundary

RFC-BPR-0030 supplies the current ten-clip asset contract. Its exact attended
review covers the neutral master and every source/runtime cell,
declared-speed motion at 64, 72, and 96 logical pixels, registration and
handoffs, System/Light/Dark/high-contrast context plates at 1.0x, 1.25x, and
2.0x, and the static `rest` fallback. Those reviewed images retain Dif's stable
brand colors but include no background plate, stylesheet, text, status badge,
or palette override.

The clip descriptions are deliberately nonsemantic. Home uses `calm_idle`,
`greet`, `blink`, and static `rest`; the six contextual gestures consume only
accepted presentation facts and play in the current corner or prime spot.
None means that Dif
reviewed, approved, rejected, or found a defect. The runtime preserves
equivalent text and controls when the mascot is absent or motion is reduced;
animation cannot become the only carrier of application state.

The historical attended verdict required the plus/minus card to stay outside
Dif's core base sprite. RFC-BPR-0030's accepted production revision is
prop-free, and the current runtime composes no prop. The accepted rasters still
cannot be silently cropped or repainted: any changed pixel requires a new
digest-bound asset verdict.

RFC-BPR-0031 owns the current widget, timer, motion-preference,
accessibility-tree, focus, layout, high-DPI, palette-change,
platform-observation, and public-path behavior. Its later owner amendment,
implemented and accepted by [[BPR-1209]], places the same sprite at 28 logical
pixels in the status bar's leading corner, outside the drawer. First-time
Home and the Compose workshop retain their prime spots. A transient status
message hides the corner art; hidden/reduced preferences and focus/input
exclusion remain unchanged. The RFC-BPR-0031 closeout ledger and BPR-1209's
exact later verdict retain their separate native and visual evidence.

## Current system-native presentation

The RFC-BPR-0002 GitHub window currently follows the palette, style, and
interface font supplied by Qt and the operating system. It does not install a
light or dark palette, select a style, apply QSS, or replace the application
font. Derived title and status fonts, standard icons, delegate geometry, and
layout are refreshed when Qt reports application palette, font, style, or
theme changes.

The focused-workbench navigation row derives every silhouette from standard
`QStyle` icons. Files, Review details, private review progress, and
Conversations retain the style's native pixels. The four Previous/Next file
and change actions retain the native arrow masks but recolor their normal and
disabled foreground pixels from the live `ButtonText` palette roles. This
prevents a style's fixed black arrow pixels from disappearing against a dark
widget-local palette while preserving platform-specific arrow geometry. All
icons are reacquired after palette, font, style, or theme changes. No QSS or
custom image asset fixes them to one palette or platform.

Rendered tests inspect the actual normal and disabled arrow pixmaps in all four
built-in profiles. They require exact palette-foreground pixels, the profile's
ordinary or high-contrast text floor for normal arrows, a 3.00:1 non-text floor
for disabled arrows, and distinct normal/disabled rasters. The real UIforETW
capture records the same pixel counts and contrast ratios beside each
full-window image, so a black-on-dark regression cannot pass merely because an
icon object is non-null.

The correspondence map follows the same rendered-pixel rule. Ordinary ribbons
blend the live `WindowText` role over the inherited gutter, while selected
ribbons retain the live `Highlight` fill and gain a `WindowText` edge. The
UIforETW capture uses each public `visible_links()` path to mask the composed
map grab, excludes overlapping paths, and observes the dominant gutter pixel
outside the paths. For each known path it records selected and unselected
dominant pixels, the number of mapped samples, the number at or above the
3.00:1 non-text floor, and median contrast. Unselected ribbons require at
least half of a meaningful sample to clear the floor; selected ribbons require
a meaningful above-floor edge sample and a dominant raster distinct from the
same unselected path. Palette values or a non-null pen alone are not evidence.

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

RFC-BPR-0006 implements user-selectable appearance profiles. A fresh
installation defaults to System, which is a default-value property proven
without any appearance change. System mode follows the operating-system color
scheme and chooses the corresponding variant; an explicit mode ignores the
observed scheme. A high-contrast system preference selects the high-contrast
variant, or recommends one when an explicit user choice outranks the
preference — the core can only recommend in that case, never override.

Resolution reports the variant together with orthogonal source facts, because
the variant alone cannot say why it applies: a dark variant looks identical
whether an explicit mode chose it or an observation did. An unknown observed
scheme is distinct from a light one, so a defaulted resolution is
distinguishable from an observed one.

The *live* operating-system scheme transition is owner-attended evidence and is
tracked separately; the static default and the resolution table are proven
unattended.

Planned built-in profiles:

- BetterPullReview Light
- BetterPullReview Dark
- BetterPullReview High Contrast Light
- BetterPullReview High Contrast Dark

The built-in dark profile uses a charcoal editor surface, muted green comments, crisp blue/cyan identifiers and control tokens, warm gold/orange accents, pale primary text, and low-chroma red/green diff backgrounds inspired by the readability of Visual Studio and Visual Assist dark configurations without copying a proprietary theme.

## Fonts

Separate settings exist for interface and code fonts. Code settings include family, point size, weight, line height, tab width, ligatures, syntax bold, and syntax italics. The default code font on every platform is the bundled JetBrains Mono NL ([[BPR-1270]]); if it cannot be loaded, the fixed-width system font is. A profile that already stored its appearance keeps the code font it stored, and resetting the code font takes the default.

The comparison has its own zoom ([[BPR-1336]]): **Diff zoom**, a percentage of
Code size, stored per profile with the code font. View > Zoom In (⌘= or ⌘+,
Ctrl+= or Ctrl++), Zoom Out (⌘−, Ctrl+−) and Actual Size (⌘0, Ctrl+0; [[BPR-1535]])
step it, as do ⌘-scroll (Ctrl+wheel) and pinch over the code: one step per
wheel notch or per 0.15 of pinch. The steps are 50, 67, 75, 80, 90, 100, 110,
125, 150, 175, 200, 250 and 300 percent. The core owns the steps, and the size
shown, Code size times the zoom, is held to the 6 to 72 pt bounds. Zoom In is
off when no further step would show a larger size, and Zoom Out when none
would show a smaller one.

Zoom never rewrites Code size, so Actual Size returns to the size the reviewer
chose. It scales the comparison only: both panes and the one-pane document,
their gutters (which re-width to their digits), their tab stops, the connector
(remeasured) and the detail pane under them. Drawers, Overview and composers
keep Code size. A pill at the top centre of the comparison shows the
percentage and the size on screen for 1.2 seconds, without animation, and the
same words are a polite accessibility announcement. Settings shows the zoom as
the Diff zoom row under Code size, and the Type card's Reset returns it to
100%.

A font change keeps the reader's line in place. When the current line is on
screen, it stays at its position in the viewport. Otherwise the line at the
synchronization rail stays. Each pane is scrolled by whole lines of the new
height in the same turn as the font change, so the line never leaves. The
remeasured geometry then places the comparison exactly.

The applied code typography is published as a fact in its own right rather than
inferred from a font change. Tab width is not a font property, and Qt sends no
event when a widget is given the font it already has, so a tab-width-only
change previously reached no surface and left measured geometry stale against
text that had visibly moved. Surfaces that measure text subscribe to the
publication and invalidate on the value that actually moved, so the reason
recorded for a rebuild stays true.

Code line height is the one typography setting that does not yet reach the
screen. `QPlainTextDocumentLayout` ignores line height in every mode Qt offers,
and ignores block margins too, so honouring it means replacing the code panes'
text-rendering strategy — which trades against the virtualization the
comparison surface depends on for large files. The setting is deliberately left
visibly unapplied rather than faked, and the choice between applying it and
retiring it is tracked as its own decision.

## Colors

Theme customization exposes semantic roles, not arbitrary QSS or executable styles:

- Application surfaces and text
- Code background, gutter, line numbers, selection, current line — the gutter and line-number pair is rendered by the source panes since BPR-0455; before that the contrast rules validated a pair nothing drew
- Added, removed, intraline added/removed, moved, hunk header
- Unread, accepted, defect, fixed, verified, stale
- Syntax token categories

Color is never the only state signal. Icons, labels, gutter marks, line prefixes, outlines, and accessible names provide redundant meaning.

### Semantic customization and local import

The settings surface lists the closed role vocabulary — 58 of its 60 roles
(not the two hunk-header roles) in five groups, Surfaces and text, Diff, Review
states, Syntax and Comment labels, with sticky group headers and counts — and
edits roles inside a total
`appearance_palette`; a raw `QPalette`, QSS fragment, or widget stylesheet is
never the saved authority. The list is built from the core's published count,
by group and within a group in the core's own order, so a role the core
declares cannot be one the editor omits, and a filter field narrows it. A row is a swatch, the role's name in
words and its value in the code font; the swatch opens the platform colour
picker and the change previews in the review window as the picker moves, and an
edited row shows its new value beside the old one. The role combo box and the
hex field are gone (RFC-BPR-0034). On a fresh System profile the editor starts
from the palette currently resolved by presentation, not from the light
bootstrap record whose unstored palette deliberately means “no user choice.”
Untouched roles therefore remain the colours on screen when the operating
system currently resolves System to Dark.

**Each theme keeps its own colours.** The store holds one customised palette
per variant — light, dark, high-contrast light and high-contrast dark — and the
Qt appearance profile keys its override table by variant, so a light
customisation is never applied over the dark profile and tuning one theme
cannot silently forget another. Until RFC-BPR-0034 there was exactly one stored
palette tagged with the variant it was made for, and committing while a
different variant was on screen made that the customised variant and discarded
the previous overrides. Everything scoped now names the theme on screen, which
the composition supplies and Qt never guesses: the Colours card is titled for
it ("Colours for Dark") under the subtitle "Edits change Dark only. Each theme
keeps its own colours."; a commit changes that variant's customisation and no
other; Import previews into it and says so; Export writes it and says which
theme was written; Reset all resets it. A theme's customised-colour count is
the number of roles whose override differs from that variant's built-in colour,
which is the comparison the profile already makes, and it is the count the
theme tiles caption.

Preview and commit remain distinct operations, but they are one control.
Preview is a Qt-local mirror of a total semantic palette: it repaints the
selected comparison immediately and does not call the settings port. While
anything is previewed — and only then — a bar at the Colours card's foot says
what is being previewed and into which theme ("Previewing 2 colour changes to
Dark in the review window.") and offers **Cancel** and **Keep changes**. Keep
changes is the commit; Cancel discards the mirror and restores the exact
palette last adopted from the host. The separate Preview, Commit and Cancel
preview buttons are gone. The bar is removed rather than hidden when nothing is
previewed, because a hidden widget is still in the accessibility tree, and an
edit that returns a role to the colour the host last adopted ends the preview
rather than leaving one with no difference in it. Commit offers the same total
palette to `appearance_application_host`, which validates it against every
shipped appearance variant before adopting it. A changed durable record issues
exactly one settings-store effect; a palette already stored exactly is
acknowledged as committed without rewriting the same record.
A refused commit leaves the host-adopted settings and durable record unchanged;
the reviewer may correct the preview or cancel it. When BPR-0236's session-only
fallback is active, the same preview and selection behavior remains available
but the surface continues to label persistence as unavailable and restart
returns to System defaults.

The composed appearance profile is the preview authority. If mode, typography,
or a native appearance observation is adopted while a palette preview is
active, including when an asynchronous durable restore finishes, the panel
receives the profile's exact active preview beside the new committed settings
and keeps Commit and Cancel enabled. Only the host's typed successful-palette
acknowledgement clears both profile and panel preview state; a palette
invalidation by itself is not that acknowledgement. The controls therefore
cannot claim there is no preview while the selected comparison still paints
one, and an equal stored-palette commit cannot leave completed controls active.
If a reviewer accepts a settings change while the startup read is still in
flight, that newer local mutation supersedes the read before its terminal can
adopt. The stale record therefore cannot overwrite the visible session after
the newer record has been queued for persistence.

Qt owns only local-file selection. It dispatches the selected path and never
opens or parses the file. Composition supplies a narrow document reader that
opens the selected path without following a stream protocol, verifies the
opened operating-system handle is a regular disk file, and reads at most
`theme_document_byte_limit + 1` bytes on the appearance effect lane. The extra
byte lets `core::import_theme` distinguish an oversized document from a
truncated one. FIFOs, pipes, devices, directories, and other special files are
refused as unavailable before a blocking read. The core parser remains the
authority for the versioned format and retains its typed refusals for malformed
documents, unknown or duplicate fields, external references, include
directives, executable/style payloads, malformed colours, and unsupported
forward versions. A structurally valid palette is still refused, with the
variant and exact `contrast_finding` preserved, if any of the four variants
fails the conservative contrast gate. Import success publishes a preview
candidate and performs no store write until the reviewer commits it.

The page around the editor is three cards in the shared row language, under the
lede "Changes apply as you make them and show in the review window behind this
one." **Theme** is a sideways-scrolling well of six tiles, one per mode in the
order System, Light, Dark, High contrast system, High contrast light and High
contrast dark. A tile is painted by the existing preview painter from that
mode's built-in palette plus that theme's overrides, so the diff and syntax
colours are seen rather than described. The two System tiles are split
diagonally, light over dark, by clipping alone — no edge is drawn, so the edge
is nothing but the change of palette — and their caption names the half in
effect ("dark now", "light now"); a theme with overrides captions its count
("2 colours customised"). The six-way mode combo box is gone: the tiles are
the control. **Type** is thirteen one-line rows — interface family, size, weight
and line height; code family, size, diff zoom and weight; tab width as a 2, 4,
8 segment; white space; and ligatures, bold and italic syntax emphasis as
switches — with sizes in
points rather than hundredths, beside a labelled Preview painted by the review
window's own code painter from the accepted typography and the theme on
screen, captioned with the values in effect. **Colours** is the editor above.

Every row is one line: label, an info glyph and the control. The consequence
sentence lives in the glyph's popover, which is shown on hover and on keyboard
focus and is also the glyph's accessible description, so it is never reachable
by pointer only; a row whose second text is a value rather than a consequence
keeps the value in the row. The keyboard route is not a convenience. Qt's tip
window hides itself on a focus change, so a tip shown from inside the focus
handler is destroyed by the very focus change that asked for it, and a tip
bound to a rectangle hides as soon as a pointer leaves that rectangle — which
shows nothing at all to a reviewer whose pointer is elsewhere or nowhere. The
sentence is therefore posted one turn of the event loop later and unbound to a
rect, and both routes are asserted rather than assumed, because every property
a test could read looked correct while the sentence reached no one
([[BPR-0790]]). A control that shares a row with siblings, such as each tab-width
segment, carries the row's consequence itself rather than leaving it on the one
thing in the row nobody can focus.

Colour-only changes reuse the selected diff, semantic selection, token-span
objects, classification cache, shared comparison position, and measured
geometry. The workbench treats palette-change events as paint invalidations;
font, style, viewport, screen, and device-scale changes retain their existing
geometry invalidation paths. This keeps semantic colour preview from becoming
an accidental reparse or layout rebuild while still letting actual metric and
style changes remeasure the comparison.

Contrast is measured rather than asserted. The core publishes the relative
luminance ratio over sRGB linearisation and three thresholds: 4.50:1 for text,
7.00:1 for text in a high-contrast variant, and 3.00:1 for a non-text
indicator. A finding carries its measured ratio rather than a boolean, so a
near-miss is distinguishable from a gross failure.

The checked pairs are enumerated rather than universal — three fixed text
pairs, every syntax role over each canvas background, and each review-state
indicator. Asserting every possible role pair would be meaningless, since most
roles never meet on screen, and a check that cannot name its pairs cannot be
audited.

The four built-in profiles are measured against that pair set, so the check
runs on the palettes that actually ship rather than on the idea of a palette.
When it was first wired up the profiles failed it: 191 pairs missed their
minimum, every one of them a syntax colour over a *fill* rather than over the
bare canvas. Contrast depends only on relative luminance, and a syntax palette
tuned to pass on the canvas alone has no headroom left — a white canvas is
already the maximum luminance, so any fill drawn over it necessarily reduces
contrast. The fills and a small number of syntax colours were refitted in
lightness, holding hue and saturation, until every enumerated pair passes. No
threshold was lowered and no pair was dropped.

Because luminance is spent on legibility, an emphasis fill is separated from
the row fill it sits inside by chroma rather than lightness: the intraline
fills are fully saturated at the band limit and the row fills sit nearer the
canvas. At the required luminance a saturated red is very nearly the removed
row's own colour, so this is enforced by a test — a collapsed emphasis fill
passes every contrast check while making intraline marking invisible.

Changed characters keep their syntax colours on the emphasis fill
([[BPR-1709]]). A colour that reads at the emphasis floor is drawn
exactly. One that does not moves only until it reads at the text floor, and
keeps its hue while it moves. On a fill that wants dark ink it shades toward
black. On one that wants light ink it first brightens, every channel scaled by
one factor until the largest is full, and only then tints toward white. So in
dark, where a selected row's lighter mark pushes the comment green under the
floor, a changed comment reads as a brighter green (`#66B559` becomes
`#8FFE7D` on the selected added mark `#117277`, 4.50:1), not white. The
comparison panes and Line details paint each token's colour as a layer over
the mark that repeats the mark's fill. Qt does not paint a foreground-only
selection over text an earlier selection drew, so without the fill every
changed glyph showed in the normal ink.

Red against green is the one distinction the palette cannot carry by colour
alone. Both modelled dichromacies lose that axis, and added-against-removed is
measured as indistinguishable in every variant. The pairs in that position are
recorded and asserted exactly, so a palette change that creates a new one
fails; each is owed a line prefix, gutter mark, icon, outline, or accessible
name, which is what "colour is never the only state signal" means concretely.

The twelve Comment labels roles, `feedback_label_praise` to
`feedback_label_quibble`, tint the Conventional Comments marks
([conventional-comments-feedback.md](conventional-comments-feedback.md)). Each
theme variant has its own built-in tints. A stored palette or a theme document
written before they existed may leave them out: they read as the built-in
tints of the theme they customise, and no other role changes. Every other role
is still required ([[BPR-1083]]).

`state_attention`, "State attention" in Review states, is the amber of
something that wants the reviewer's attention before acting: the dot on
Approve while the reviewer's own blocking feedback is open ([[BPR-1085]]). It
is declared last, after the label tints, so no stored ordinal moves, and lists
under State stale. Its built-in values are `#AD7A00` (Light), `#D29922`
(Dark), `#8A5300` (High contrast light) and `#FFBC66` (High contrast dark),
each at least 3:1 against its variant's surface and code background. GitHub's
`#BF8700`, which the dot used before, is 2.93:1 on the light surface. A stored
palette or theme document without it reads the built-in value, as for the
label tints.

## Syntax highlighting

Use KSyntaxHighlighting through a custom low-level highlighter. Highlight complete old and new blobs independently so hunks beginning inside multiline constructs receive correct state. Cache semantic spans by blob and definition, independent of the active color theme.

This is implemented. A hunk is a *projection* of the complete-blob pass rather
than a separate highlight, because highlighting a range on its own loses the
state that opened a multiline comment or string on an earlier line. That is the
mechanism behind the acceptance criterion, and it must not be reduced to a
per-hunk call: doing so passes casual testing and fails precisely the case the
criterion exists for.

The span cache is keyed by blob identity and definition version with the theme
deliberately absent from the key, so a theme change *cannot* invalidate it. That
is a stronger guarantee than remembering not to evict on theme change.

Definitions and approved themes are packaged; standard-path discovery and
downloads stay disabled in the production build. Build-time source acquisition
of the library and runtime grammar downloading are different things, and only
the second is prohibited.

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

The workbench shell declares its focus order explicitly. Its compact row begins
with Files, Previous file, Next file, Previous change, Next change, Review
details, private review progress, and Conversations. The row is persistent and
is not rebuilt or replaced by a second toolbar at a narrower width, so its
keyboard identity cannot drift across a responsive-mode transition. The
private-progress button opens the same receipt actions in a keyboard-operable
popup and safely returns to the persistent row when the popup closes.

The declared order never reorders a stop that belongs to a composed region. The
epoch control and the comparison surface keep their own internal focus order,
and the comparison surface contributes exactly one stop to the shell's order.
A shell that reshuffled another component's internal order would be deciding
that component's presentation for it.

Each drawer's controls precede its tree in the declared order (RFC-BPR-0025,
[[BPR-0981]]): the repositories drawer's Filter pull requests and Clear pull
requests filter, then the files drawer's Files order, Filter files and Clear
files filter. A Clear filter button is shown, and so reachable, only while its
filter matches nothing. The order is walked at 100, 125 and 200 percent by
`qt_workbench_focus_order_scale_{100,125,200}`.

**No accessible name may be shared by two focusable widgets in the assembled
window.** This is enforced by a test rather than by convention, because the
rule was broken four separate times before it was enforced: a retired
placeholder duplicated the composed detail pane, the toolbar's change buttons
duplicated the comparison surface's own in-surface buttons, the drawer toggles
duplicated the drawer panels' list views, and a former navigation overflow menu
duplicated its opener. The duplicate in-surface change buttons and navigation
overflow are removed, and the compact row is the single presenter. Each former
case left two focusable widgets answering to one identity, which assistive
technology cannot disambiguate. Visible labels may repeat; accessible names
may not.

The same scan forbids duplicate key sequences, for the related reason that Qt
resolves an ambiguous shortcut by triggering neither binding, so a collision
presents as a control that silently does nothing rather than as an error.

The current GitHub window publishes one combined polite announcement for a
status and diagnostic presentation update. Repeated identical text is
deduplicated so one accepted projection does not become a burst of field-level
announcements. Connection and technical facts hidden by disclosures remain
available through named keyboard-operable controls.
