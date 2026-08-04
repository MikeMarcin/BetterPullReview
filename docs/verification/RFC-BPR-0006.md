# RFC-BPR-0006 Verification Plan and Ledger

RFC base: `91e6b7044ced7b73af6a1d1993c9ed97f4f6b2d4`

Status: available-target implementation complete except the owner-attended
scheme toggle. Documentation reconciled by BPR-0112. RFC-BPR-0006 stays
`Proposed` and ends its available-target work at the BPR-0115 closeout card in
`backlog/`.

## Architecture allocation

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Appearance-mode resolution, typography value legality, semantic role vocabulary | Qt-free tables and replay tests; dependency scan proving no font, color, Qt, or KSyntaxHighlighting type enters the core |
| Application host | Settings persistence, resource policy, token-span caches keyed by blob and definition version independently of theme | Fake-port tests covering persistence, restore, and cache invalidation without semantic change |
| Qt adapter | Profile variants, immediate preview, font application, measured-axis and ribbon invalidation, syntax presentation, contrast and redundancy | Qt adapter tests, three-scale cases, and fixture-driven definition selection |

Appearance changes publish bounded presentation invalidations. They never
change review semantics.

## Dependency acquisition decision

RFC-BPR-0006 mandates KSyntaxHighlighting through its low-level API, so the
library itself is not optional. Only the acquisition route was open, and it is
recorded here because the reasoning matters more than the outcome.

The repository has two precedents. Qt is a caller-installed CMake package that
is never fetched, and the dependency boundary states that CPM is not used to
replace caller-installed platform packages. SQLite is CPM-acquired
download-only source, pinned by version and cryptographic digest and staged
into a first-party static target.

KSyntaxHighlighting is a Qt-dependent KDE Frameworks library, which on its face
sits closer to the Qt precedent. That route was investigated first and is not
available: this host has no Homebrew formula for KSyntaxHighlighting, and none
for the KDE Frameworks generally. Requiring a caller-installed package that
cannot be installed would make the RFC unimplementable rather than deferred.

CPM source acquisition is therefore the only route that satisfies the RFC at
all. This is a forced choice, not a preference, and it carries a real cost that
BPR-0109 must record: KSyntaxHighlighting builds with extra-cmake-modules, so
ECM becomes a transitive build dependency and must appear in the dependency
inventory with its own pinned version and digest.

Acquisition does not relax the RFC's non-goals. Definitions and approved themes
remain packaged as resources, and standard-path discovery and downloads stay
disabled in the production build. Build-time source acquisition and runtime
grammar downloading are different things, and only the latter is forbidden.

## Ordering

The appearance, typography, and theme-import surfaces need no new dependency,
so they land first. The build-foundation change is isolated in BPR-0109 and the
syntax work follows it. That delivers the unblocked half before any dependency
risk is taken, and keeps a build regression from blocking unrelated slices.

## Claim matrix

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Definitions are selected correctly | Fixture files | Classify C++, CMake, Markdown, JSON, Python, and plain-text fixtures | Each fixture selects its expected definition | Verified. Six fixture languages each select their expected definition from packaged records |
| Hunk classification equals whole-blob classification | Complete-blob highlighter | Classify a hunk beginning inside a multiline comment or string | The hunk receives the same classification as the complete blob, which a hunk-local highlighter would fail | Verified. Dropping spans that opened before the hunk's first line fails the case, so the projection is load-bearing |
| Theme change does not re-parse | Instrumented highlighter | Switch theme and colour, counting lexical parses | The parse count does not increase | Verified. The span cache key omits the theme, so a palette change cannot evict; the suite pins one classification to one lexical pass |
| Font change preserves semantics | Composed comparison | Change the code font | The measured axis and ribbon geometry invalidate, semantic selection survives, and the shared position reprojects without changing mapping or confidence | Verified. Geometry rebuilds and the measured extent grow while the mapping is byte-identical and selection survives |
| Fresh install defaults to System | Settings default | Inspect the default with no persisted value | The resolved mode is System | Verified. With no persisted value the resolved selection is System |
| System mode follows the OS; explicit does not | Real macOS composition | Owner-authorised OS scheme change | System mode follows and an explicit mode does not | Deferred, BPR-0114, owner-attended |
| Settings persist and preview immediately | Real application | Change each setting, restart | Each value previews without restart and survives one | Partially verified. Each typography field previews without restart and a core-rejected record is refused whole rather than clamped; BPR-0116 added the typography publication so a tab-width-only change invalidates measured geometry. Code line height is computed but reaches no surface, because `QPlainTextDocumentLayout` ignores every line-height mode — measured, not assumed — and applying it means replacing the panes' rendering strategy against RFC-BPR-0005's virtualization evidence. BPR-0118 owns that decision |
| Theme import is safe | Import validator | Offer unknown fields, URLs, includes, oversized files, and executable payloads | Each rejection is distinct; a catch-all that refuses everything is not sufficient | Verified. Each refusal is typed and named, and a valid document round-trips to an identical palette |
| State survives without colour | Contrast and redundancy checks | Inspect every state signal | Icons, labels, marks, prefixes, outlines, or accessible names carry the meaning too | Verified. BPR-0117 ran the enumerated pairs against all four shipped palettes; 191 pairs failed and were repaired by refitting lightness, and the ten pairs colour alone cannot separate are recorded and asserted exactly |
| The core stays free of presentation types | Dependency scan | Scan the core target | No font, colour, Qt, or KSyntaxHighlighting type appears | Verified. No font, colour, Qt, or KSyntaxHighlighting identifier appears in the core, and the dependency scan passes |

## Deferred gates

One owner-attended gate: BPR-0114, the live operating-system scheme change. The
static fresh-install default is a separate claim and is proven unattended.

No RFC-BPR-0006 requirement needs target-native Windows execution, a GitHub
mutation, a credential, or a network service at runtime. The one build-time
network need is the first CPM fetch of the pinned sources, after which the
cache serves offline configures.

## Closeout blockers

RFC-BPR-0006 cannot become `Auto-Verified` until RFC-BPR-0005 is, which
requires RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002, and therefore real
Windows x64 evidence. BPR-0115 stays in `backlog/` and makes no closure claim.
