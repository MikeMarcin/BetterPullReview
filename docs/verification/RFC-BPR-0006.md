# RFC-BPR-0006 Verification Plan and Ledger

RFC base: `91e6b7044ced7b73af6a1d1993c9ed97f4f6b2d4`

Status: **components complete, composition absent.** Every component this RFC
specifies is built and tested. None of them is connected to the application.
BPR-0180 measured it and allocated BPR-0181 through BPR-0185 to close it.
Documentation reconciled by BPR-0112. RFC-BPR-0006 stays `Proposed` and its
BPR-0115 closeout card stays in `backlog/`.

Read the Composition section before reading the claim matrix. Every `Verified`
row below is true of the component it names and true of nothing the user can
reach.

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

## Composition

BPR-0180 asked the linker what the shipped binary contains. Against the `dev`
build:

| Measurement | Result |
|---|---|
| `nm build/dev/src/app/better_pull_review \| c++filt \| grep -c appearance_application_host` | `0` at the time of measurement; **non-zero after BPR-0182** |
| `nm build/dev/src/app/better_pull_review \| c++filt \| grep -c packaged_syntax_classifier` | `0` |
| Non-test references to `appearance_profile` in `src/` | one stale include at the time of measurement; **the composition owns one after BPR-0183** |
| `grep -rln appearance src/app/` | `main.cpp` only, and only its test probe |
| `BetterPullReview::AdapterSyntax` in the composition's link list | absent |

So there is no appearance mode, no font setting, no theme, and **no syntax
highlighting in the diff panes**. `diff_workbench.cpp`'s only `Highlight` is
`QPalette::Highlight`, which colours a selection.

The application does have Qt's native palette propagating through the composed
window. That is `QMacStyle` following the operating system, which happens
whether or not this RFC exists. It is not `core::appearance_mode::system`
resolving, because the application never asks — and mistaking one for the other
is what kept this invisible.

Every component below was proven non-vacuous: assertions broken, failures
observed, behaviour restored. That establishes a test constrains its subject.
It cannot establish that the subject is reachable from the product, and no
break test on an uncomposed component ever could. The check that catches this
is the one above: ask the linker.

## Claim matrix

Every row's `Direct surface` now names what the row was actually proven
against. Where that differs from what the row previously claimed, it is called
out.

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Definitions are selected correctly | `packaged_syntax_classifier`, in isolation | Classify C++, CMake, Markdown, JSON, Python, and plain-text fixtures | Each fixture selects its expected definition | Verified of the component. Six fixture languages each select their expected definition from packaged records. The classifier has zero symbols in the shipped binary; BPR-0185 links it |
| Hunk classification equals whole-blob classification | `packaged_syntax_classifier`, in isolation | Classify a hunk beginning inside a multiline comment or string | The hunk receives the same classification as the complete blob, which a hunk-local highlighter would fail | Verified of the component. Dropping spans that opened before the hunk's first line fails the case, so the projection is load-bearing. Not reached by any pane; BPR-0185 owns that |
| Theme change does not re-parse | `appearance_application_host`, fake ports | Switch theme and colour, counting lexical parses | The parse count does not increase | Verified of the component. The span cache key omits the theme, so a palette change cannot evict; the suite pins one classification to one lexical pass. The host is not linked into the binary; BPR-0182 composes it |
| Font change preserves semantics | An `appearance_profile` and a `diff_workbench` constructed together by a test | Change the code font | The measured axis and ribbon geometry invalidate, semantic selection survives, and the shared position reprojects without changing mapping or confidence | Verified of that pair. Geometry rebuilds and the measured extent grow while the mapping is byte-identical and selection survives. **This row previously named its surface `Composed comparison`.** The pair is composed by the test; the application composes neither member. BPR-0183 owns it |
| Fresh install defaults to System | `appearance_settings`, the type's own default | Inspect the default with no persisted value | The resolved mode is System | Verified of the type. With no persisted value the resolved selection is System. No application path reads it, because no application path constructs the host that would |
| System mode follows the OS; explicit does not | Real macOS composition | Owner-authorised OS scheme change | System mode follows and an explicit mode does not | **Blocked, not deferred.** BPR-0114 was filed as owner-attended, which asserted the behaviour existed. It does not: there is no mode to select and no variant to observe. Blocked behind BPR-0183 and BPR-0184; attended afterwards |
| Settings persist and preview immediately | `appearance_application_host`, fake ports | Change each setting, restart | Each value previews without restart and survives one | Verified of the component only. **This row previously named its surface `Real application`.** The host is absent from the shipped binary and no adapter implements `appearance_settings_port`, so nothing persists and nothing previews; BPR-0181 and BPR-0182 own that. Within the component: each typography field previews without restart and a core-rejected record is refused whole rather than clamped, and BPR-0116 added the typography publication so a tab-width-only change invalidates measured geometry. The code line height is **retired**, not outstanding: `QPlainTextDocumentLayout` ignores every line-height mode Qt offers — measured, not assumed — so the product owner retired it on 2026-08-06 and BPR-0118 removed it structurally. `code_font_settings` now has no field one could occupy, and the RFC's goal and criterion were amended in place |
| Theme import is safe | Import validator, in isolation | Offer unknown fields, URLs, includes, oversized files, and executable payloads | Each rejection is distinct; a catch-all that refuses everything is not sufficient | Verified of the component. Each refusal is typed and named, and a valid document round-trips to an identical palette. No application path imports a theme, because there is no theme surface |
| State survives without colour | The four shipped palettes, in isolation | Inspect every state signal | Icons, labels, marks, prefixes, outlines, or accessible names carry the meaning too | Verified of the palettes. BPR-0117 ran the enumerated pairs against all four shipped palettes; 191 pairs failed and were repaired by refitting lightness, and the ten pairs colour alone cannot separate are recorded and asserted exactly. None of the four is applied by the application; the window uses Qt's native palette |
| The core stays free of presentation types | Dependency scan | Scan the core target | No font, colour, Qt, or KSyntaxHighlighting type appears | Verified. No font, colour, Qt, or KSyntaxHighlighting identifier appears in the core, and the dependency scan passes |

## Composition cards

Allocated by BPR-0180. None needs Windows, a credential, attendance, or a
network service.

| Card | Slice |
|---|---|
| BPR-0181 | Adapter: durable appearance settings behind `appearance_settings_port` |
| BPR-0182 | Compose `appearance_application_host` into the application |
| BPR-0183 | Qt: the appearance profile drives the composed window |
| BPR-0184 | Qt: the appearance settings gesture |
| BPR-0185 | Link the syntax adapter and highlight the diff panes |

BPR-0118 retired the code line-height setting on the product owner's decision
and is independent of these: it rests on Qt's measured behaviour, which holds
whether or not anything is composed. It is in `done/`.

Its structural half is worth carrying into BPR-0184. Deleting the accessor
would have left `code_font_settings::Font.LineHeightPercent` validated,
persisted, and inapplicable, because the code font embeds the shared record.
The field moved to a UI-only `ui_font_settings` instead, so the code font has
no field a line height could occupy. A settings surface cannot offer what the
type cannot hold.

## Deferred gates

One owner-attended gate: BPR-0114, the live operating-system scheme change.
It is **blocked before it is deferred** — the behaviour it observes does not
exist until BPR-0183 and BPR-0184 land. The static fresh-install default is a
separate claim and is proven unattended.

No RFC-BPR-0006 requirement needs target-native Windows execution, a GitHub
mutation, a credential, or a network service at runtime. The one build-time
network need is the first CPM fetch of the pinned sources, after which the
cache serves offline configures.

## Closeout blockers

RFC-BPR-0006 cannot become `Auto-Verified` until RFC-BPR-0005 is, which
requires RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002, and therefore real
Windows x64 evidence.

It is now also blocked by its own composition. BPR-0115 stays in `backlog/`,
makes no closure claim, and cannot be prepared until BPR-0181 through BPR-0185
are in `done/` and BPR-0114 has been run against something real.
