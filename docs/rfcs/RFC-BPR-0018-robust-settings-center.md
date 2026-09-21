---
id: "RFC-BPR-0018"
title: "Robust Settings Center and Preference Ownership"
status: "Proposed"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0014"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# RFC-BPR-0018: Robust Settings Center and Preference Ownership

## Current reconciliation, 2026-09-20

The ordered 0018 → 0024 → 0025 → 0026 closeout run adopts the existing
`close-rfc-0018` milestone. Its twenty implementation/follow-up cards are Done;
[[BPR-0593]] records current checks and remaining acceptance. The missing
[Settings guide](../development/settings.md) now describes the composed
categories, defaults, persistence, resets and platform limitations.

The owner's planning amendment permits updating stale design context. The
initial fragmentation described below is historical: all seven categories are
composed, RFC-BPR-0024 supplies the profile-owned singleton, RFC-BPR-0032 revises
Accounts & hosts, and RFC-BPR-0034 supplies the current Appearance page and
per-theme palettes. Their accepted observations remain bound to their original
candidates. RFC-BPR-0035 now owns the General page's application-wide updater;
its separate release/platform acceptance is not implied by this Settings RFC.
These ownership corrections change none of AC-01 through AC-14.

The macOS Settings review below and the later Accounts/Appearance reviews are
accepted. Windows/Linux packaged inventories are complete on [[BPR-0730]] and
[[BPR-0731]]. The [2026-09-20 Mac capture and comparison](../verification/BPR-0593-macos-settings-inventory.md)
now supplies the missing like-for-like native inventory at the Windows source
revision: all seven pages match Windows/Linux actionable controls. Current Mac
dev/release trees also match, and real Preferences/Command-comma/same-profile
window courses pass. The required appearance restart contract is repaired and
passes without production changes. The ledger still requires Windows/Linux
native menu/shortcut courses and a current refresh of the later updater/CLI
rows; neither is inferred from the baseline capture. RFC-BPR-0014 remains
Proposed, so canonical dependency order independently prevents Auto-Verified.

## Accepted attended history, 2026-09-12

The shared UX pack's latest prepared page is
[RFC-BPR-0028 attended v011](../verification/reviews/rfc-bpr-0028-attended-v011/operator-packet.md).
The owner accepted its sole [[BPR-0646]] epoch-handle candidate at session
sequence 373; the earlier [v010](../verification/reviews/rfc-bpr-0028-attended-v010/operator-packet.md)
one-pane verdict remains bound to its own bundle. The initial fifteen-candidate
[v006 packet](../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md)
included this RFC's macOS Settings candidate [[BPR-0683]] and its digest-bound
offline page. Those decisions do not complete this RFC's Settings closeout. The bound
`attended-preparation-pack-head` milestone and remaining exits are recorded
on [[BPR-0593]]. This RFC remains Proposed.

## Status and scope

This is an activated post-pack RFC with implementation and attended evidence.
It does not alter the canonical fourteen-RFC sequence and blocks no pack-v2
implementation or closeout. Proposed is its current acceptance/dependency state,
not a claim that Settings remains unimplemented.

**Implementation resumed by owner direction, 2026-09-07.**
The product owner's 2026-09-04 daily-workbench amendment adds direct sign-in,
remembered server management, and one Settings window per BPR profile. The
requested sequence is RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026.
Registration waives no existing eligibility or canonical closeout requirement.
Registration baseline: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Entry: [[BPR-0592]] is complete. Closeout: [[BPR-0593]]. The
[authority inventory](../development/settings-authority-inventory.md) maps
remaining work to BPR-0678 through BPR-0683; 0028 supplied the existing three pages.

This RFC first composes Settings in the existing default environment.
RFC-BPR-0024 later supplies multiple independent BPR profiles and windows;
Settings accepts their profile binding without a reverse RFC dependency.
An application BPR profile is distinct from a configured GitHub host profile.

The product owner selected the AllTheThings settings window as the interaction
and visual-quality reference: one native, persistent settings window with a
stable sidebar; scrollable pages; grouped cards; concise labels and explanatory
text; direct controls; live appearance previews; and reset actions placed next
to the values they own. BetterPullReview adopts that system, not the other
product's setting inventory. Search indexing, launch-at-login, menu-bar status,
and automatic update controls are not copied merely because they appear in the
reference.

The goal is not a larger appearance panel. It is one coherent place to inspect
and change the product's durable and session preferences without centralizing
their policy, credentials, or effects in a widget or generic settings map.

## Problem

BetterPullReview already has typed settings and durable stores for appearance,
workbench layout, queue filters and sorting, observer rules, notification
behavior, GitHub host/account selection, agent profiles, private-context
selection, retention, and diagnostics. Their ownership is intentionally split
across core policy, application hosts, narrow stores, Qt presenters, and the
composition root.

At the registration baseline, the visible controls were not a coherent product
surface. Appearance was embedded beside the review workbench; queue preferences
lived with queue controls; account and repository configuration occupied session
chrome; other preferences were represented only by their immediate feature.
This had four costs:

- reviewers cannot discover what is configurable or distinguish durable from
  session-only choices;
- each new feature is tempted to add another local panel and persistence path;
- resets, unavailable persistence, validation failures, and platform-managed
  permissions have no consistent presentation;
- cross-platform keyboard, focus, scaling, and accessibility behavior must be
  rediscovered for every small settings surface.

## Product contract

BetterPullReview provides one singleton, modeless **Settings** window per BPR
profile, initially the existing default environment. The profile being edited
is identified in the window. The
native application menu and `Command+,` on macOS, and the platform-appropriate
menu action plus `Ctrl+,` elsewhere, raise that profile's existing window rather
than creating duplicates. Escape closes the window only when doing so does not steal
an active recorder, picker, or confirmation interaction.

The window uses a stable sidebar and scrollable content pages. A page contains
named sections and visually grouped cards. Each row has a short control label,
one plain-language consequence, the control itself, and—only when necessary—a
status or refusal explanation. Resizing, 100–200% scale, long translations,
keyboard-only navigation, screen readers, high contrast, reduced motion, and
all supported appearance modes are first-class layouts rather than later
exceptions.

Changes apply through typed domain intents. There is no global Apply button and
no untyped dictionary commit. A control reflects accepted host state, not its
own optimistic value. Rejected or unavailable changes restore the authoritative
value and explain why. Destructive reset, credential removal, cache purge, and
privacy operations require explicit confirmation and report their exact scope.

## Direct sign-in and remembered servers

First launch exposes **Sign in to GitHub** and **Add enterprise server** without
opening Settings, expanding Details, choosing a transport, or scrolling a small
pane. The same actions are available from Accounts & Hosts and from a direct
reconnect affordance when an accepted session expires or fails.

On supported device-authorization profiles, Sign in immediately begins the
existing RFC-BPR-0016 flow and presents its user code, copy action, and browser
action in a dedicated, reachable surface. There is no second Get code button.
Waiting, cancellation, expiry, refusal, and browser-handoff failure remain
visible through the existing typed host projection. The manual-token path
remains available, including GHES servers whose current device flow is
unsupported. This redesign changes entry and presentation, not authentication
capabilities or browser-opening authority.

Accounts & Hosts provides explicit add/edit server configuration, one account
per server, concise connection status, and reconnect/sign-out actions. Remember
validated user-configured servers and their account selections across launches;
do not require repeated command-line GHES configuration. Reuse existing device
binding metadata and platform-vault persistence for supported remembered grants.
Manual tokens retain their existing session-only behavior and are labeled so;
this RFC does not silently make every credential durable.

Server changes go through typed configuration intents and accepted host state.
Changing authority invalidates affected work and never reuses old credentials
for another host. RFC-BPR-0024 partitions those records by BPR profile and
propagates sign-out across its windows. This RFC introduces the default-scope
server configuration record and ports; it does not preempt profile identity.

RFC-BPR-0026 may hand setup an exact pending PR destination. Successful accepted
authentication returns that destination to its owner; cancellation leaves the
existing review usable. Until that RFC is composed, no URL-opening capability
is claimed. Transport, capability, rate, and diagnostic details belong in
Settings disclosures. Ordinary review chrome retains only compact status and
direct recovery actions. Appearance, agent configuration, and storage management
also move to Settings; the existing diff and tuned colors are preserved.

## Information architecture

The initial sidebar is ordered by reviewer task, not implementation module:

1. **General** — startup landing behavior, session restoration policy, status
   detail, and other product-wide preferences that do not belong to a more
   specific domain.
2. **Accounts & Hosts** — GitHub.com and GHES profiles, selected account,
   authentication state, credential replacement/removal actions, capability
   summaries, and host-specific refusals. Credential material is never shown or
   copied.
3. **Reviews & Queue** — default queue filters and sort, quiet/current handling,
   observer-rule management, review-layout restoration, and review-navigation
   preferences. This page changes local behavior and never mutates GitHub
   assignment or subscription state.
4. **Appearance** — System, Light, Dark, and high-contrast selection; UI and
   code typography; code-specific options; semantic colors; theme import,
   preview, commit, cancel, and reset. The current embedded appearance panel is
   migrated here rather than duplicated.
5. **Notifications** — metadata-only notification enablement, platform
   permission status, quiet behavior, preview, and deep-link explanation. A
   setting cannot claim an operating-system permission that the platform does
   not expose.
6. **Agents & Context** — allowed agent profiles, host/profile mappings,
   selected private-context sources, invocation constraints, and session
   defaults. No agent credential, hidden context, or unselected private data is
   projected into this page.
7. **Storage & Privacy** — cache and ledger locations as non-editable facts
   unless a domain explicitly supports relocation; retention; per-scope and
   global purge; diagnostic detail; previewable diagnostic export; and a plain
   inventory of what remains after uninstall.

RFC-BPR-0027 owns the diagnostic logging, verbosity, privacy and export
contracts exposed here. This Settings RFC presents those typed controls when
available; logging can first ship through Help without depending on this window.

Sections may be absent only when the corresponding capability is structurally
unavailable in that build. A temporarily unavailable setting remains visible,
disabled, and explained so failure is not mistaken for product omission.

## Visual and interaction system

The AllTheThings reference contributes these normative patterns:

- a visually stable sidebar with icon, label, selected state, and full-row hit
  target;
- page headings separated from cards, with consistent card insets and row
  rhythm;
- control labels and consequences aligned independently of control width;
- direct theme previews instead of a text-only theme selector;
- local reset affordances beside the exact group they reset;
- bounded page scrolling rather than an ever-growing dialog or accordion;
- native window chrome and platform focus behavior, not a web-settings clone.

The existing appearance allocation remains intact: the functional core owns
semantic roles, mode resolution, legality, exchange vocabulary, and contrast;
the application host owns accepted settings and persistence; the Qt appearance
profile applies that accepted state to native presentation. The settings window
consumes the same projections, including the code/UI font distinction; it does
not introduce a second stylesheet, hard-coded dark palette, or platform-specific
pixel copy. Preview thumbnails are rendered from the same accepted semantic
role values that the workbench will adopt.

The sidebar collapses to an accessible compact navigation mode only when the
minimum content width cannot be retained. It never hides the current category
or relies on icon recognition alone. Page and row identity remain stable across
appearance changes, language changes, capability updates, and host/account
switches.

## Architecture allocation

| Region | Responsibility |
|---|---|
| Functional core | Domain-owned setting types, defaults, validation, reset semantics, dependency rules, typed intents/outcomes, and deterministic descriptions of unavailable or destructive scope. No Qt, filesystem, database, provider, process, clock, or hidden global state |
| Application hosts | Load/save sequencing, migrations, capability observation, credential/cache/notification effects, cancellation, serialized acceptance, failures, and projections. Each host retains authority for its own domain |
| Persistence adapters | Versioned, bounded records and atomic domain transactions. Existing domain stores remain separate; there is no universal key/value settings database |
| Qt adapter | Settings window, sidebar/pages/cards, focus and accessibility, event-to-intent translation, live previews, confirmations, and incremental projection by stable setting identity. No direct store, GitHub, Git, vault, filesystem, notification, or agent effect |
| Composition root | Supplies the exact profile-bound domain capabilities, routes typed intents to their owning hosts, aggregates projections, and owns one Settings window per profile plus native menu integration |

The settings center is an aggregator of capabilities, not a service locator.
Each page receives the smallest typed presenter/intent ports it needs. A
`QWidget`, model, or delegate cannot obtain a database, vault, provider,
filesystem path, notification service, process launcher, or application object.

## State, identity, and concurrency

Every projected row has a stable identity composed from its domain and setting
identifier. A projection carries the authoritative value, default, durability
state, availability, validation constraints, reset scope, and generation. It
does not carry credentials, source/comment bodies, agent output, or arbitrary
provider payloads.

An intent carries domain identity, setting identity, expected generation, and a
typed candidate. The host serializes decisions on the application decision
lane. A completion from a stale window generation, host/account generation, or
superseded preview cannot overwrite newer state. Closing the settings window
cancels previews and UI-only observations but does not cancel an already
accepted durable commit or destructive operation; those finish through their
own bounded lifecycle and publish the result.

Changes made by another composed surface of the same profile are projected into
its open settings window. Other profiles are unaffected. The settings window
does not poll stores or treat control state as
authority. Routine changes update only the affected rows/cards and do not reset
the sidebar, rebuild every page, or discard focus and selection.

## Persistence and migration

- Existing appearance, workbench, queue, observer, agent, and review stores
  retain their schemas and owners.
- A new preference is durable only when its RFC already defines persistence;
  the settings center does not make session state durable by accident.
- Every store migration is transactional, versioned, bounded, restart-tested,
  and fail-closed on future or malformed versions.
- Failure of one optional store disables only its owned durable controls. Other
  pages remain usable, and session-only fallback is labeled precisely where a
  domain permits it.
- Reset restores the domain's declared default through the same typed host path
  as any other change. It does not delete unrelated database rows or recreate
  an entire application-data root.
- Import/export is domain-specific. There is no whole-product settings archive
  containing credentials, repository content, private context, or opaque
  database bytes.

## Security and privacy

- Credential material, authorization headers, replay identities, approval
  artifacts, source/comment bodies, agent prompts/output, and private context
  never enter settings projections, logs, clipboard actions, screenshots, or
  exports.
- Account controls expose identity and credential *state* only. Enrollment,
  replacement, and removal use existing sensitive and vault-owned flows.
- File and directory selection uses the existing application-host capability
  and canonical path policy. A text field never becomes an ambient path bypass.
- Observer and notification settings remain local and cannot cause GitHub
  assignment, subscription, label, or review mutation.
- Destructive storage/privacy actions name exact affected scopes, support
  cancellation before commitment, and produce secret-free completion evidence.
- Update controls require their own selected mechanism and verified authority.
  RFC-BPR-0035 now owns the user-confirmed updater composed in General, with
  installation/platform limits and remaining release qualification recorded in
  its ledger. This Settings RFC grants no update or installation authority;
  the reference application's update toggle is not precedent.

## Performance and resource limits

- At most one settings window per BPR profile and one materialized page per
  category in that window are live.
  Pages may be created lazily, but their stable identities and focus restoration
  must be deterministic.
- The catalog and every page have fixed admitted-setting and row limits. A
  plugin or provider cannot append arbitrary settings.
- Capability refresh and store loading are asynchronous and cancellable. The Qt
  thread performs no database, vault, filesystem, network, or process work.
- Appearance preview coalesces rapid input and preserves the existing rule that
  palette changes do not rerun lexical parsing; font geometry rebuilds remain
  separately attributed and bounded.
- Large rule/profile lists use incremental models and bounded visible work, not
  one widget per unbounded row.

## Prohibited designs

- A universal string-key/string-value settings map or one database table that
  absorbs all domain state.
- A `SettingsService`, application singleton, or Qt property bag reachable by
  every widget.
- Direct widget access to SQLite, GitHub, Git, credentials, files, processes,
  notifications, agents, clocks, or timers.
- Duplicating the embedded appearance panel and settings-center controls as two
  independent authorities.
- Optimistic controls that remain changed after the host rejects a request.
- Hiding unsupported or failed settings without an explanation.
- Whole-page/model resets for one ordinary preference change.
- Copying AllTheThings-specific features without a BetterPullReview product
  requirement and owning RFC.
- A static screenshot or isolated dialog standing in for the composed product
  path.

## Acceptance criteria

- AC-01: The shipped application exposes a native Settings action and one singleton,
  modeless window per available BPR profile through platform-appropriate menus
  and shortcuts, with the edited profile visibly identified.
- AC-02: All seven initial categories are present when their capabilities are composed,
  with stable sidebar/page/row identities and truthful unavailable states.
- AC-03: Appearance is migrated from the embedded workbench surface to the settings
  center with no duplicate authority; existing mode, typography, palette,
  import/preview/commit/cancel/reset, and persistence behavior remains intact.
- AC-04: Every setting gesture becomes one typed domain intent, reaches only its owning
  host, and updates the control only from an accepted authoritative projection.
- AC-05: Existing domain stores migrate and reopen from prior schemas; future,
  malformed, interrupted, and unavailable cases fail at their exact scope
  without corrupting another domain.
- AC-06: Credential, privacy, cache, notification, and agent actions retain their
  existing capability and secrecy boundaries.
- AC-07: Keyboard-only and screen-reader courses reach every sidebar item, row control,
  reset, confirmation, status, and close action in a natural order.
- AC-08: Layout and rendered evidence pass at 100%, 125%, 150%, and 200% scale in
  Light, Dark, High-contrast Light, and High-contrast Dark, including long text
  and minimum window size.
- AC-09: One-setting updates preserve sidebar selection, page scroll position, focus,
  unaffected persistent indexes, and all unrelated projections without routine
  resets.
- AC-10: Hostile catalog, profile, rule, and row floods stop at declared limits with
  typed refusal; one window materializes at most one page per category; rapid
  previews coalesce; cancelled or superseded loads publish nothing late; and
  instrumentation observes zero database, vault, filesystem, network, process,
  or blocking-wait work on the Qt thread.
- AC-11: Tests-disabled packaged applications on macOS, Windows, and Linux expose the
  same setting inventory except for explicitly documented platform capability
  rows.
- AC-12: Sign in starts supported device authorization directly and shows its code and
  browser action without Details, another Get code gesture, or pane scrolling;
  cancellation, expiry, and supported manual-token alternatives remain reachable.
- AC-13: User-configured servers and account selections survive restart through typed
  host persistence, while remembered grants reuse the existing vault path and
  session-only manual credentials are never presented as durable.
- AC-14: Configuration leaves the primary review surface compact, preserves tuned
  diff/appearance behavior, and exposes direct reconnect plus an exact pending
  destination handoff without changing credential or provider authority.
## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| State remains domain-owned | Core/host architecture contracts | Inventory every setting type, intent, projection, store, and effect; reject generic maps, service locators, and Qt effect access |
| One truthful settings window per profile | Shipped application composition | Invoke menu and shortcut from windows in the same available profile, require one Settings identity, exact category inventory, and no fixture-only construction; RFC-BPR-0024 completes concurrent distinct-profile evidence |
| Persistence is exact and isolated | Domain stores plus two-process tests | Change one setting, reopen, migrate prior schemas, inject failure, and prove unrelated domains and rows are byte/semantically unchanged |
| Appearance has one authority | Appearance host, workbench, and settings center | Drive every existing appearance course through the new page and prove the old embedded editor is absent while workbench adoption remains exact |
| Controls follow host acceptance | Host/Qt cut tests | Reject candidates, reorder completions, close during preview, and require authoritative rollback with stale generations ignored |
| Accessibility and focus are complete | Qt component and composed tests | Walk declared and live focus chains, screen-reader names/descriptions/states, shortcuts, Escape rules, and confirmation focus return |
| Rendering is robust | Deterministic full-window captures | Capture every appearance/scale/long-text/minimum-size combination after geometry and semantic assertions, then run attended visual review |
| Resource use stays bounded | Hostile catalog/list fixtures, manual executor, Qt instrumentation, and preview course | Overflow every admitted catalog/list limit and require typed refusal; assert one page instance per category and bounded widget/model/signal counts; cancel and supersede loads; burst preview changes; require coalesced accepted work, zero late projection, and zero effect or blocking-wait instrumentation on the Qt thread |
| Secrets never enter presentation | Type/source/runtime audits | Prove forbidden material has no projection field and canaries do not appear in UI trees, clipboard, diagnostics, screenshots, or exports |
| Platform behavior is honest | Target-native packaged apps | Verify native menus/shortcuts/window lifetime, permission states, font/style inheritance, and documented platform-only rows on macOS, Windows, and Linux |
| Direct sign-in and recovery | Public onboarding and Settings gestures with recording device/vault/browser ports | One Sign in gesture reaches visible code/browser controls with zero disclosure or scroll; exercise cancel, expiry, unavailable device flow, manual entry, and reconnect |
| Remembered server configuration | Typed server host/store and public restart | Add a fixture GHES server, reopen, verify the exact configured authority/account, replace authority, and inject persistence failure without borrowing credentials |
| Compact review and navigation handoff | Shipped review window and setup completion port | Prove configuration is absent from primary review chrome, diff geometry/colors remain correct, and success/cancellation/stale completion returns only the exact owning pending request |

## Eligibility gates

Implementation begins only after RFC-BPR-0014 is Auto-Verified or the product
owner explicitly advances this post-pack implementation frontier. Before the
first code slice, a planning card must inventory every existing user-adjustable
value and classify it as durable, session-only, observed, platform-managed, or
not a setting. The inventory must also identify current stores, migrations,
authority, reset scope, and any owner-attended platform permission or visual
evidence.

## Suggested implementation decomposition

The 2026-09-04 registration creates only the backlog entry [[BPR-0592]] and
canonical closeout anchor [[BPR-0593]], with an immutable base and all product
evidence not run. The descriptions below guide the later bounded-slice planning
pass; additional live IDs are allocated only when implementation needs them.
Creating these two cards does not satisfy the eligibility gate above.

1. Inventory every existing user-adjustable value, authority, store, migration,
   reset, generation, failure, and privacy class.
2. Model stable typed projections, domain intents, accepted/refused outcomes,
   and incremental aggregation without generic settings state.
3. Build the accessible Qt shell with one instance per available profile, native menu/shortcut integration,
   sidebar, cards, compact mode, focus, and scale behavior.
4. Compose all seven pages and migrate the embedded appearance editor without
   duplicating authority.
5. Verify migrations, two-process persistence, failure isolation, privacy,
   target-native packages, deterministic rendering, and attended acceptance.
6. Simplify, reconcile every acceptance row, and perform independent RFC
   closeout only after canonical dependency order permits it.

## Documentation impact

The [settings guide](../development/settings.md) must explain every category, default, durability state,
reset, platform difference, unavailable state, destructive operation, and
privacy consequence. Architecture documentation must identify domain
ownership, typed intent/projection paths, persistence schemas, generations,
profile-bound singleton window lifetime, incremental update rules, and prohibited generic
settings mechanisms. Packaging and security documentation must inventory the
exact tests-disabled setting surface and its platform-specific capabilities.
