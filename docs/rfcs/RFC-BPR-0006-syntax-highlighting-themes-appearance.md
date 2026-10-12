---
id: "RFC-BPR-0006"
title: "Syntax Highlighting, Themes, Fonts, and Appearance Profiles"
status: "Auto-Verified"
area: "BPR"
milestone: "M1 Native Read-Only Workbench"
depends_on: ["RFC-BPR-0005"]
pack_version: 2
---

# RFC-BPR-0006: Syntax Highlighting, Themes, Fonts, and Appearance Profiles

## Summary

Add complete-blob syntax classification, dark/light/system/high-contrast appearance modes, configurable UI and code fonts, semantic colors, and safe local theme profiles.


## Motivation

Code-review readability requires editor-grade syntax treatment and user-controlled appearance. Themes cannot be bolted on after the renderer because colors, fonts, row heights, cache invalidation, high contrast, and accessibility all affect rendering contracts.

## Goals

- Add syntax highlighting through KSyntaxHighlighting's low-level API.
- Highlight complete old/new blobs independently.
- Support System, Light, and Dark appearance modes plus high-contrast variants.
- Add separate UI and code font family, size, weight, tab-width, ligature, bold, and italic settings, plus a UI line height.

The code font has no line-height setting. It was specified here and retired by
product-owner decision on 2026-08-06, after it was measured that
`QPlainTextDocumentLayout` ignores line height in every mode Qt offers —
`ProportionalHeight`, `FixedHeight`, `MinimumHeight`, and `LineDistanceHeight`
all leave the block height unchanged, and block margins are ignored with them.
Honouring it would mean replacing the code panes' text-rendering strategy,
which RFC-BPR-0005's bounded-widget and large-file evidence rests on. Code line
spacing is whatever the code font supplies. The setting is not deferred and no
later RFC inherits it; `core::code_font_settings` has no field it could occupy.
- Add semantic UI, diff, review-state, and syntax color customization.
- Provide immediate preview, safe import/export, and deterministic packaged resources.

## Non-goals

- Downloading grammars or themes.
- Loading arbitrary QSS, HTML, executable styles, repository-provided definitions, or third-party plugins.
- Semantic AST highlighting.

## Architectural allocation

- Appearance, fonts, colors, system-scheme observation, accessibility presentation, `QFont`, `QColor`, and KSyntaxHighlighting are imperative-shell concerns and do not enter the functional core.
- **Host:** owns settings persistence, resource loading policy, background scheduling, syntax-span/layout caches, cancellation, and resource limits.
- **Qt adapter:** resolves system/light/dark/high-contrast presentation, applies semantic color roles, owns font/layout mechanics, renders previews, and translates settings gestures.
- **Core:** may receive only semantic settings-completion or availability facts when another command depends on them; it does not decide what color or font is rendered.
- Theme/font changes invalidate only the affected presentation/layout caches and publish bounded deltas rather than rebuilding review state or reparsing source unnecessarily.
- Any geometry-affecting font, zoom, line-spacing, tab, theme, screen, or DPI
  change invalidates RFC-BPR-0004's Qt-local measured alignment axis and ribbon
  geometry. It cannot change semantic mapping, pairing confidence, groups, or
  stable identities; a fresh installation still defaults to System appearance.

## Highlighting design

A custom `AbstractHighlighter` collector produces semantic spans and line-end state. KSyntaxHighlighting definitions and approved base themes are packaged in resources; standard-path discovery and downloads are disabled in the production build. Token spans are cached by blob OID and definition version independently from theme colors and text layout.

## Appearance model

An appearance profile has paired light/dark variants. System mode resolves one variant from the OS scheme. High-contrast preference selects a high-contrast variant or applies documented overrides.

Colors are semantic roles, including code background, gutters, additions, removals, intraline emphasis, hunk headers, unread/accepted/defect/fixed/verified states, and syntax categories. State also has non-color indicators.

## Acceptance criteria

- C++, CMake, Markdown, JSON, Python, and plain-text fixture files select expected definitions.
- A hunk beginning inside a multiline comment/string receives the same classification as the complete blob.
- Switching theme or color does not re-run lexical parsing.
- Changing font invalidates the Qt-local measured axis and ribbon geometry
  while preserving semantic selection, then reprojects the one shared
  comparison position without changing mapping or confidence.
- System mode responds to OS scheme changes; explicit mode does not.
- UI and code fonts, sizes, UI line height, tab width, ligatures, bold, italic, and semantic colors persist and preview immediately. The code line height is retired; see the goals above.
- Theme imports reject unknown fields, URLs, includes, oversized files, and executable/style payloads.
- High-contrast and color-vision checks prove state remains understandable without color alone.
- Dependency scans prove no font/color/KSyntaxHighlighting/Qt types enter the functional core; theme/font interactions update host caches and Qt presentation without mutating review semantics.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Full-blob state is used | Highlighting integration | Compare hunk token roles against whole-file golden spans |
| Theme switch avoids reparse | Cache instrumentation | Switch modes and inspect lexical cache hit counters |
| System mode follows OS | Real application | Toggle OS scheme and observe profile variant change |
| Custom colors are safe | Import validator | Feed QSS/URL/include/oversized malicious profiles and reject |
| Appearance stays outside core | Dependency and interaction tests | Core has no Qt/theme types; host/Qt update only affected caches and presentation |

## Documentation impact

Supported definitions, resource policy, appearance model, font/color roles, accessibility, cache invalidation, import/export schema, and security limits are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.

## Owner direction, 2026-09-11

The built-in dark profile's syntax roles follow Visual Studio's dark theme
with the Visual Assist colouring (green comments, grey preprocessor lines
including the include path), lightened only where a Visual Studio value
falls under this RFC's 4.5:1 floor on the diff row fills; the light and
high-contrast profiles are unchanged. A stored customisation from before
that change is reset once when the appearance store migrates to schema
version 4. [[BPR-0720]] carries the values, the evidence and the attended
criterion. The C and C++ classifier gives calls, types and ALL_CAPS macros
their roles ([[BPR-0726]], accepted 2026-09-11 in the v007 attended
session); the owner rejected the palette in the same session because
macros paint the directive grey and functions Visual Studio's pale yellow
rather than the Visual Assist purple and gold of the reference, which
[[BPR-0738]] carries. The owner then corrected the reading (2026-09-11,
mandate r9): functions are orange and class/struct types yellow in the
reference, and a distinct macro role is necessary; BPR-0738 holds the
per-token resample (`FF8000`, `FFD700`, `BD63C5`) and adds `syntax_macro`
to the role model. Landed 2026-09-11: `syntax_macro` is a role of the
vocabulary, the C and C++ classifier gives it to ALL_CAPS words, and the
dark profile paints types `FFD700`, functions `FF8000`, variables
`BDB76B`, attributes `4EC9B0` and macros `CE8AD4` (the reference's
`BD63C5` lightened along its hue to clear the 4.5:1 floor); the appearance
store moves to schema version 5 and gives a stored palette its macro row
from its own preprocessor colour, with no second reset. The attended
criterion on BPR-0720 is asked again in v008.

## Owner direction, 2026-09-11 (the v008 verdicts)

The owner rejected both palette candidates in the v008 attended session
(`cbd48b91…`, the bundle at 35438244, handed back at sequence 348) with one
requirement stated twice. On [[BPR-0720]]: "comments are green, things are
mostly right, but preprocess being grey is taken too far compared to the
reference. The preprocessor directives themselves should be grey but not
necessarily their arguments. Macros in preprocessor should still color as
macros (purple). Strings in #include statements should color as the reference
image." On [[BPR-0738]]: "most things are good now but include paths should be
painted like the reference and things like macros in `#ifndef MACRO` should be
painted as macros."

This revises the direction above. Its parenthetical, "grey preprocessor lines
including the include path", now reads: the **directive** is grey; its
arguments are not. An ALL_CAPS argument takes the `syntax_macro` role, the
same purple a macro takes in ordinary code, and an `#include` path is painted
as the Visual Assist reference paints it rather than the directive grey. The
comment green, the identifier role colours landed under BPR-0738, the one-time
reset and the schema-5 store are not disputed and are unchanged. [[BPR-0744]]
carries the change: the classifier already emits roles at sub-line ranges, and
what collapses a directive line is the packaged grammar's default Preprocessor
attribute together with the identifier post-pass running only over
`syntax_normal` runs.

Two questions this RFC asked through BPR-0738's candidate are still open and
are asked again: whether the standard-library aliases' italic in the reference
matters, and whether `CE8AD4` is acceptable as the macro purple in place of the
reference's `BD63C5` (which reaches only 3.18 against the added fill under the
selection wash, where this RFC's floor is 4.5) or whether the floor should
yield instead. The v008 notes answer neither; `CE8AD4` stands only because
nothing has replaced it, not because it was approved.

## Owner direction, 2026-09-12 (the two open palette questions, closed)

The two questions the section above left open were put to the owner and both
are answered, verbatim.

- **The standard-library aliases' italic**: "**no**", it does not matter. The
  reference renders `std::string_view` and `path` in italic; the product does
  not and will not be changed to. Closed.
- **The macro purple**: "**yes it's acceptable**". `CE8AD4` stands in place of
  the reference's `BD63C5`, and this RFC's 4.5:1 floor on the diff row fills
  is not asked to yield — `BD63C5` reaches only 3.18 against the added fill
  under the selection wash, `CE8AD4` clears at 4.54. Closed, and the value now
  stands because it was approved rather than because nothing had replaced it.

Neither question is repeated in a later attended packet. What remains for the
owner on this RFC is the single attended criterion [[BPR-0720]] and
[[BPR-0738]] both carry, asked again on a bundle carrying [[BPR-0744]]'s
directive ranges.

## Attended session, 2026-09-12 (v009)

The remaining attended criterion of [[BPR-0720]] and [[BPR-0738]] — now also
[[BPR-0744]]'s — is asked as the single candidate `BPR-0744-directive-ranges` in
[rfc-bpr-0028-attended-v009](../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md),
prepared and protected at sequence 357 on the bundle built from source
`aff9fc64`. One candidate, not three: the owner's v008 notes on two separate
candidates were one requirement stated twice, and he said so.

Landed for it: a preprocessor directive keyword stays `syntax_preprocessor`,
an ALL_CAPS word inside a directive takes `syntax_macro`, a predefined macro
the packaged grammar styles as its own `Standard Macros` takes it too, and a C
or C++ `#include` path takes `syntax_string` rather than `syntax_import`. No
appearance role was added and no palette value moved, so the appearance store
stays at schema 5 and there is no second reset. One question rides the
candidate: a predefined macro spelled without a capital (`__clang__`,
`__cplusplus`) is still near-grey, and the context-aware rule that would fix it
is costed on [[BPR-0744]] rather than taken.

## Owner direction, 2026-09-12 (BPR-0748)

The owner later answered **yes** to the separate [[BPR-0748]] question:
identifiers in `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and `defined()`
take `syntax_macro` regardless of capitalization. Directive operators,
literals, header names, attribute and builtin-test operands keep their own
roles; ordinary identifiers and `#define` replacement tokens are not
broadened. This direct answer replaces the pending side-by-side question,
while the v009 candidate above remains the historical record. BPR-0748
carries the implementation and focused checks; no palette or schema value
moves.

## C++ context amendment, 2026-10-12 (RFC-BPR-0072)

[[RFC-BPR-0072]] qualifies the C++ ALL_CAPS convention with complete-blob
lexical evidence before casing. Namespace declarations, alias names and
`using namespace` chains receive `syntax_namespace`. Scoped and unscoped
enumerator declarations receive `syntax_enum_member`; a qualified use receives
that role only when its declared scope and member are unambiguous in the blob.
Unknown qualified terminal names remain ordinary identifiers, while unknown
qualifiers keep the generic type treatment and invocations retain the existing
macro/function convention. Aliases, shadows and conflicting declarations do
not establish enum identity. Unqualified enum uses are outside the guarantee.

The grammar still protects comments and strings, and complete preprocessor
lines cannot supply structural declarations. Nested initializer delimiters do
not end an enum or create enumerators. This refinement runs only for C++ in
the syntax adapter, with bounded blob-local state and cancellation; C and
Windows Resource retain their existing behavior. C++ identifier revision 6
retires revision-5 cache keys; blob/definition cache identity remains independent
of theme, and hunk spans project the complete-blob result.

The two new colour roles append to the durable vocabulary without renumbering
existing roles. Their defaults enter the existing variant profiles, contrast
pairs and Colours controls. Missing new roles in an older palette document are
unassigned in the core import and resolve to the chosen variant's built-in
values in Qt. Existing overrides and stored flags remain authoritative. Final
rendered, persistence and exchange acceptance belongs to RFC-BPR-0072; prior
palette verdicts and parked BPR-0720/BPR-0738 obligations are preserved.
