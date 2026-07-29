---
id: RFC-BPR-0006
title: Syntax Highlighting, Themes, Fonts, and Appearance Profiles
status: Proposed
area: BPR
milestone: M1 Native Read-Only Workbench
depends_on: [RFC-BPR-0005]
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
- Add separate UI and code font family, size, weight, line-height, tab-width, ligature, bold, and italic settings.
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

## Highlighting design

A custom `AbstractHighlighter` collector produces semantic spans and line-end state. KSyntaxHighlighting definitions and approved base themes are packaged in resources; standard-path discovery and downloads are disabled in the production build. Token spans are cached by blob OID and definition version independently from theme colors and text layout.

## Appearance model

An appearance profile has paired light/dark variants. System mode resolves one variant from the OS scheme. High-contrast preference selects a high-contrast variant or applies documented overrides.

Colors are semantic roles, including code background, gutters, additions, removals, intraline emphasis, hunk headers, unread/accepted/defect/fixed/verified states, and syntax categories. State also has non-color indicators.

## Acceptance criteria

- C++, CMake, Markdown, JSON, Python, and plain-text fixture files select expected definitions.
- A hunk beginning inside a multiline comment/string receives the same classification as the complete blob.
- Switching theme or color does not re-run lexical parsing.
- Changing font invalidates layout while preserving source selection and approximate scroll anchor.
- System mode responds to OS scheme changes; explicit mode does not.
- UI and code fonts, sizes, line height, tab width, ligatures, bold, italic, and semantic colors persist and preview immediately.
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
