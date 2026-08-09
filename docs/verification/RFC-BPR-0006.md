# RFC-BPR-0006 Verification Plan and Ledger

RFC base: `91e6b7044ced7b73af6a1d1993c9ed97f4f6b2d4`

Status: **predefined appearance and syntax composition now survives appearance
store-open failure; required user and visual gates remain open.** BPR-0181
through BPR-0185 connected the appearance host, predefined profiles, gesture,
and syntax classifier. BPR-0236 adds the composition-owned session fallback:
the window starts from System defaults, keeps session profile switching and
syntax classification, identifies persistence as unavailable, and returns to
System defaults on restart without recording a false durable write. The
RFC-required semantic palette customization, preview, persistence, and safe
theme-import gesture were never built; BPR-0237 owns them. BPR-0225 also found
that the selected diff they style is requested only through a tests-enabled
fixture helper, while the recorded UIforETW public test never requests a diff.
BPR-0227 and BPR-0233 own those public-selection and recorded-intraline gaps.
BPR-0114 and BPR-0220 retain the frontmost native appearance run and its
unexplained two-rebuild result.

Read the Composition and Deferred gates sections before the claim matrix. A
`Verified` row proves only the direct surface it names.

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
| `nm build/dev/src/app/better_pull_review \| c++filt \| grep -c packaged_syntax_classifier` | `0` at the time of measurement; **non-zero after BPR-0185** |
| Non-test references to `appearance_profile` in `src/` | one stale include at the time of measurement; **the composition owns one after BPR-0183** |
| `grep -rln appearance src/app/` | `main.cpp` only, and only its test probe |
| `BetterPullReview::AdapterSyntax` in the composition's link list | absent at the time of measurement; **present after BPR-0185** |

So there is no appearance mode, no font setting, no theme, and **no syntax
highlighting in the diff panes**. `diff_workbench.cpp`'s only `Highlight` is
`QPalette::Highlight`, which colours a selection.

The application does have Qt's native palette propagating through the composed
window. That is `QMacStyle` following the operating system, which happens
whether or not this RFC exists. It is not `core::appearance_mode::system`
resolving, because the application never asks — and mistaking one for the other
is what kept this invisible.

> **Superseded, 2026-08-08.** Every measurement above was true when taken and
> is false now. BPR-0183 composed the profile, BPR-0185 linked the classifier,
> and BPR-0184 supplied the gesture. The appearance the application paints is
> the profile's, and `--diff-evidence-appearance` renders any of the six modes
> on demand. All four shipped palettes have since been rendered and accepted at
> attended review: light and dark in
> `docs/verification/reviews/diff-scheme-acceptance-v002/`, both high-contrast
> profiles in `docs/verification/reviews/high-contrast-scheme-acceptance-v001/`.
>
> The section is kept rather than rewritten because the reasoning in it — that a
> component can be exhaustively tested and still be unreachable from the
> product, and that only the linker settles it — is what found the gap. The
> numbers are a record of the state it found.

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
| Definitions are selected correctly | `packaged_syntax_classifier`, in isolation, and the shipped binary | Classify C++, CMake, Markdown, JSON, Python, and plain-text fixtures; `verify_composed_hosts.py` on the shipped binary | Each fixture selects its expected definition, and the classifier is present in the product | Verified. Six fixture languages each select their expected definition from packaged records. **BPR-0185 linked the adapter**: `packaged_syntax_classifier` is now in the shipped binary and is on the standing COMPOSED list, and the composed application constructs it - `SYNTAX_CLASSIFIER_COMPOSED_OK` is the runtime half, because a symbol proves the link and not the constructor |
| Hunk classification equals whole-blob classification | `packaged_syntax_classifier`, in isolation, and a real `diff_workbench` | Classify a hunk beginning inside a multiline comment or string; read the colour off the pane's block layout | The hunk receives the same classification as the complete blob, which a hunk-local highlighter would fail | Verified of the component and of the pane. **BPR-0185 added `qt_syntax_pane_composition`**, which links both adapters, classifies a blob whose block comment opens four hundred lines above the assertion, scrolls until the opening delimiter is off screen, and reads the comment colour off `QTextBlock::layout()->formats()`. Indexing the spans by window offset instead of blob line leaves the marker line unpainted, which was measured |
| Theme change does not re-parse | `appearance_application_host` with fake ports, and the composed application | Switch theme and colour, counting lexical parses; switch appearance mode on the running application with a classified comparison on screen | The parse count does not increase | Verified of the component and of the product. The span cache key omits the theme, so a palette change cannot evict; the suite pins one classification to one lexical pass. **BPR-0182 composed the host and BPR-0185 measured the composed claim**: `SYNTAX_THEME_REPAINT_OK` asserts that a mode change on the running application leaves `classification_submissions()` where it was, having already adopted a classification. The Qt suite proves the other half - the panes repaint in the new palette from the retained spans without adopting a new classification |
| Font change preserves semantics | An `appearance_profile` and a `diff_workbench` constructed together by a test | Change the code font | The measured axis and ribbon geometry invalidate, semantic selection survives, and the shared position reprojects without changing mapping or confidence | Verified of that pair. Geometry rebuilds and the measured extent grow while the mapping is byte-identical and selection survives. **This row previously named its surface `Composed comparison`.** The application now constructs an appearance profile and the window owns a diff workbench with either durable settings or BPR-0236's session fallback, but this row remains pair-harness evidence. Product reachability waits for BPR-0227 |
| Fresh install defaults to System | `appearance_settings`, the type's own default | Inspect the default with no persisted value | The resolved mode is System | Verified of the type and of the product. With no persisted value the resolved selection is System, and the composed application now reads it: `application_appearance_gesture_contract`'s first phase reports `system` from a fresh root. Previously verified of the type alone, when no application path constructed the host |
| Appearance and syntax survive appearance-store open failure | Two processes of the real executable with only `appearance-settings.sqlite3` obstructed | `ctest --preset dev -R application_appearance_store_fallback_contract --output-on-failure` | The first process reports the host, classifier, profile-applied window colours, nonzero adopted classifications, session gesture, persistence-unavailable status, and zero durable writes; the second reports System mode and tab width four, and neither may report persistence available | Verified. The exact database-path obstruction leaves the epoch catalog, review ledger, and workbench settings on their ordinary paths. Removing the fallback makes the host assertion fail; restoring the durable-store classification guard makes the nonzero syntax-adoption assertion fail; treating the session write as durable makes the zero-write or restart-default assertion fail |
| System mode follows the OS; explicit does not | Real macOS composition | Scripted OS scheme change under the owner's standing authorisation, on a real display | System mode follows and an explicit mode does not | **Both halves proven, 2026-08-08, unattended.** On a real display with the scheme driven Light→Dark: `NATIVE_APPEARANCE_TRANSITION:Light->Dark`, `APPEARANCE_SYSTEM_FOLLOWS_OK`, `APPEARANCE_EXPLICIT_IGNORES_OK`, `NATIVE_APPEARANCE_SIGNAL_OK`, and `APPEARANCE_VARIANTS:0:1:0:1` — System resolved light→dark, an explicit mode selected through BPR-0184's gesture stayed **light** across the same change, and returning to System resolved dark again. The middle value is the one QMacStyle following the operating system cannot fake. What remains is narrower than the claim: the probe must be **frontmost**, which cannot be scripted, and eight probe terms rooted in `StatePassed` are unaudited until that foreground run — one of them, `DIFF_GEOMETRY_REBUILD_DELTA:2`, is a product question rather than a focus artefact. Formerly: *"Unblocked by BPR-0184, still attended"*, and before that, filed as owner-attended when no mode existed to select |
| Predefined settings persist and preview immediately | The composed application with a healthy durable store, across separate processes | Drive the composed predefined-profile settings surface, then run a second process against the same application data root | Each implemented predefined value previews without restart and survives one | Verified for the healthy-store predefined subset. **BPR-0184 added the gesture and `application_appearance_gesture_contract`**, three phases of the real binary: a fresh composition reports `system` and a tab width of four, a run driving the composed controls reports `dark:8` with `APPEARANCE_GESTURE_OK`, and a separate process against the same root making no gesture reports `dark:8`. BPR-0236 separately proves that unavailable storage is labeled session-only and restores System defaults rather than claiming persistence; semantic palette customization is BPR-0237. Previously verified of the component only, with the note below. | <!-- superseded:  **This row previously named its surface `Real application`.** The host is absent from the shipped binary and no adapter implements `appearance_settings_port`, so nothing persists and nothing previews; BPR-0181 and BPR-0182 own that. Within the component: each typography field previews without restart and a core-rejected record is refused whole rather than clamped, and BPR-0116 added the typography publication so a tab-width-only change invalidates measured geometry. The code line height is **retired**, not outstanding: `QPlainTextDocumentLayout` ignores every line-height mode Qt offers — measured, not assumed — so the product owner retired it on 2026-08-06 and BPR-0118 removed it structurally. `code_font_settings` now has no field one could occupy, and the RFC's goal and criterion were amended in place  -->|
| Theme import is safe | Import validator, in isolation | Offer unknown fields, URLs, includes, oversized files, and executable payloads | Each rejection is distinct; a catch-all that refuses everything is not sufficient | Verified only of the validator component. Each refusal is typed and named, and a valid document round-trips to an identical palette. No application path imports or previews a theme; BPR-0237 owns the required product surface |
| Semantic colors can be customized and previewed | Product appearance surface | Edit semantic roles, preview without commit, persist, restart, and restore | The selected comparison changes immediately, committed roles survive restart, and cancellation restores the prior palette without semantic or geometry churn | Not implemented. Predefined profiles exist, but no palette-customization surface or path to `core::import_theme` exists; BPR-0237 owns the criterion |
| State survives without colour | The four shipped palettes, in isolation | Inspect every state signal | Icons, labels, marks, prefixes, outlines, or accessible names carry the meaning too | Verified of the palettes and of the product. BPR-0117 ran the enumerated pairs against all four shipped palettes; 191 pairs failed and were repaired by refitting lightness, and the pairs colour alone cannot separate are recorded and asserted exactly. **All four are now applied and all four have been rendered and accepted at attended review.** The recorded colour-only obligations grew by two as a consequence of that review — the light profile's warning against error after its syntax refit, and the dark profile's intraline pair after its bands were softened to the owner's reference — each accepted deliberately rather than paid for by keeping tones the owner rejected. BPR-0204's ratchet additionally names the tightest contrast pair in each profile and holds its headroom, which is how high-contrast light was found to have none (BPR-0210) |
| The core stays free of presentation types | Dependency scan | Scan the core target | No font, colour, Qt, or KSyntaxHighlighting type appears | Verified. No font, colour, Qt, or KSyntaxHighlighting identifier appears in the core, and the dependency scan passes |

## Composition cards

Allocated by BPR-0180. None needs Windows, a credential, attendance, or a
network service.

| Card | Slice |
|---|---|
| BPR-0181 | Adapter: durable appearance settings behind `appearance_settings_port` |
| BPR-0182 | Compose `appearance_application_host` into the application |
| BPR-0183 | Qt: the appearance profile drives the composed window |
| BPR-0184 | Qt: the appearance settings gesture — **done** |
| BPR-0185 | Link the syntax adapter and highlight the diff panes — **done** |
| BPR-0196 | Paint the diff in the panes — **done** |
| BPR-0198 | Moved code, change-group boundaries, intraline evidence — **done** |

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

One owner-attended gate: BPR-0114 — and what is attended about it is now much
smaller than the card was filed for. The behaviour it observes did not exist
until BPR-0183 and BPR-0184 landed; those are done, and on 2026-08-08 both
halves of the claim were proven on a real display without a person present,
because the product owner's standing authorisation permits the scheme change to
be scripted. The static fresh-install default is a separate claim and is also
proven unattended.

What the standing authorisation does not cover is focus. The probe requires its
window to be **active, focused and frontmost**, and says so in the marker's
name; a process launched from a background shell never satisfies that, and
raising it with `System Events` produced `APPEARANCE_VARIANTS:-1:-1:-1:-1` — no
baseline captured at all. That gate is deliberate: an appearance judgement made
against a window that was never frontmost is a judgement about a window nobody
was looking at. So the remaining attended step is one click, followed by a
per-term audit of the eight probe terms that root in `StatePassed`.

An earlier version of this section attributed BPR-0114's block to BPR-0154's
open-stage signature. That attribution is obsolete and the card retracted it in
place: *"[[BPR-0154]] is closed and it was not this card's blocker after all.
Its signature had nothing to do with a fixture or a stall."* BPR-0154 itself is
`Done` under RFC-BPR-0003 — it was not withdrawn, it simply was not this. The
real sequence was `presentation=0`, then a window check (`presentationClean`)
that required the window to carry no `WA_SetPalette` and so asserted the
feature's own absence, which BPR-0183 had made false by design.

Everything else this RFC defers to a person has been done. Four attended
batches were run between 2026-08-06 and 2026-08-08, three of them rejected in
whole or in part:

| batch | outcome |
|---|---|
| `selection_contrast_remedy` | all three subjects rejected — nothing was painted beneath the selection, so the question could not be judged |
| `diff_presentation_acceptance` | both subjects rejected — the captures were cropped to the wrong region of the window |
| `selection_shade` | candidate C chosen; one acceptance criterion unanswerable from single-line captures |
| `diff_scheme_acceptance` v001 | light accepted, dark and its selection rejected |
| `diff_scheme_acceptance` v002 | both schemes accepted, no notes |
| `high_contrast_scheme_acceptance` | both profiles accepted, no notes |

The rejections are the useful part of that record. Two of them were defects in
how the question was asked rather than in the product, and both were the
reviewer catching an error the measurements could not: a marking mistaken for
the selection because it was more prominent than the selection, and a criterion
that no single-line capture could answer.

No RFC-BPR-0006 requirement needs target-native Windows execution, a GitHub
mutation, a credential, or a network service at runtime. The one build-time
network need is the first CPM fetch of the pinned sources, after which the
cache serves offline configures.

## Closeout blockers

RFC-BPR-0006 cannot become `Auto-Verified` until RFC-BPR-0005 is, which
requires RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002, and therefore real
Windows x64 evidence.

The composition half of that block is now clear: BPR-0181 through BPR-0185 are
all in `done/`, and the composed application reports its own composition at
runtime rather than being read off a symbol table. BPR-0236 additionally proves
the same host, profile-to-window, and syntax-to-pane path under exact appearance
store-open failure, with a separate process proving session changes do not
survive as false durable state. BPR-0114 has been run against something real and
its two headline claims hold; it stays in `backlog/` for the frontmost-focus run
and the eight-term audit, not for the behaviour.

BPR-0115 therefore stays in `backlog/` and makes no closure claim, but its
autonomous preparation — replaying the claim matrix against the composed
application and resolving the documentation ledger — is no longer blocked. The
terminal transition remains blocked by BPR-0114 and by closure ordering.
