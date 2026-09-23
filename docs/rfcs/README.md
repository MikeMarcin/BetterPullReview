# BetterPullReview RFC Index

Current delivery priority (owner direction, 2026-09-06): follow the
[private-alpha route](../development/private-alpha.md) to a personal work build
for GitHub.com and GitHub Enterprise. Reuse implemented surfaces, address only
the essential reading/publication gaps, and defer nonessential features and
hardening. This bounded delivery priority does not change RFC statuses or
canonical closeout order, and does not by itself activate the deferred redesign RFCs.

The RFCs normally begin and always close in the order below. A dependent RFC
does not become `Auto-Verified` before every listed dependency is
`Auto-Verified`.

All live RFCs reside directly in `docs/rfcs/`, regardless of status or milestone.
There is no `future/` status folder. RFC front matter and this index carry RFC
status; issue-card lanes carry implementation backlog and progress. `plans/`
and imported `assets/` remain source/reference material, not alternate live RFCs.

Product-owner exception: real Windows x64 evidence retained by BPR-0036 is
temporarily unavailable. RFC-BPR-0002 therefore remains the closure frontier,
but that hardware-only gate does not block implementation, tests,
documentation, security review, simplification, or later-RFC work on
available targets. RFC-BPR-0003 and RFC-BPR-0004 retain explicit closeout
cards.

By further product-owner direction the implementation frontier now advances
past RFC-BPR-0004 through the remainder of the pack on available targets.
Implementation, tests, documentation, security review, and simplification for
RFC-BPR-0005 through RFC-BPR-0014 may proceed while earlier RFCs remain
`Proposed`. Each such RFC ends its available-target work at a closeout card in
`10-backlog/` and stays `Proposed`.

Every requirement that cannot be met on an available target is recorded on a
live issue card naming exactly what is missing and who must supply it. Three
classes are deferred: target-native Windows x64 execution, owner-attended
checks that need a person at the keyboard, and external authority such as a
designated safe GitHub mutation target or a signing identity.

The exception does not waive any Windows claim, change an RFC status, permit
out-of-order Auto-Verified closeout, or make GitHub Actions an evidence
surface. Closure remains strictly ordered: no RFC becomes `Auto-Verified`
before every listed dependency is `Auto-Verified`.

| RFC | Title | Status | Depends on |
|---|---|---|---|
| [RFC-BPR-0001](RFC-BPR-0001-product-contract-native-architecture.md) | Product Contract, Native Architecture, and Repository Foundation | Auto-Verified | — |
| [RFC-BPR-0002](RFC-BPR-0002-github-host-auth-capability-layer.md) | GitHub Host, Authentication, and Capability Layer | Auto-Verified | RFC-BPR-0001 |
| [RFC-BPR-0003](RFC-BPR-0003-git-cache-pr-identity-review-epochs.md) | Local Git Cache, Pull Request Identity, and Review Epochs | Auto-Verified | RFC-BPR-0001, RFC-BPR-0002 |
| [RFC-BPR-0004](RFC-BPR-0004-diff-computation-line-mapping-grouping.md) | Diff Computation, Correspondence Mapping, Grouping, and Alignment-Space Scrolling | Auto-Verified | RFC-BPR-0003 |
| [RFC-BPR-0005](RFC-BPR-0005-native-workbench-virtualized-review-surface.md) | Native Workbench Shell and Virtualized Review Surface | Auto-Verified | RFC-BPR-0004 |
| [RFC-BPR-0006](RFC-BPR-0006-syntax-highlighting-themes-appearance.md) | Syntax Highlighting, Themes, Fonts, and Appearance Profiles | Auto-Verified | RFC-BPR-0005 |
| [RFC-BPR-0007](RFC-BPR-0007-local-review-ledger-actionability.md) | Local Review Ledger, File and Conversation Receipts, and Actionability | Auto-Verified | RFC-BPR-0003, RFC-BPR-0005 |
| [RFC-BPR-0008](RFC-BPR-0008-github-review-round-trip-interactions.md) | GitHub Review Synchronization and Round-Trip Interactions | Auto-Verified | RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007 |
| [RFC-BPR-0009](RFC-BPR-0009-defect-fix-verification-workflow.md) | Defect Classification, Fix Claims, and Verification | Auto-Verified | RFC-BPR-0008 |
| [RFC-BPR-0010](RFC-BPR-0010-review-queue-observers-notifications.md) | Review Queue, Observer Rules, and Local Notifications | Auto-Verified | RFC-BPR-0007, RFC-BPR-0008 |
| [RFC-BPR-0011](RFC-BPR-0011-ai-agent-profiles-context-supplements.md) | AI Agent Profiles, Launch, Authentication Isolation, and Context Supplements | Auto-Verified | RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007 |
| [RFC-BPR-0012](RFC-BPR-0012-ai-review-chat-findings-mcp.md) | AI Review Findings, Comment Workshop, Side Chat, and MCP Context | Auto-Verified | RFC-BPR-0008, RFC-BPR-0011 |
| [RFC-BPR-0013](RFC-BPR-0013-performance-resilience-resource-limits.md) | Performance, Resource Limits, Resilience, and Recovery | Auto-Verified | RFC-BPR-0004, RFC-BPR-0005, RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0010, RFC-BPR-0012 |
| [RFC-BPR-0014](RFC-BPR-0014-cross-platform-packaging-security-release.md) | Release Packaging and Maintenance Foundation | Auto-Verified | RFC-BPR-0013 |

On 2026-09-20 the owner closed RFC-BPR-0013's selected-diff investigation under
an explicit acceptance amendment: the unusual-target comparison-basis and
partial-pane anomaly is deferred to BPR-0944, not marked fixed. BPR-0794/0795
are Done. That amendment left the wider acceptance and then-open predecessors
unresolved. The later r4 terminal amendment below explicitly supersedes that
stopping boundary; no historical target observation is upgraded to a pass.

On 2026-09-21, owner amendment r4 to land RFC-BPR-0014 was applied by explicit
coordinator assignment to RFC-BPR-0008's blocking closeout. BPR-0242 closes its
implemented scope with existing exact live comment/reply evidence, current local
public-path/safety checks and documented source-pin reconciliation. BPR-0143,
BPR-0144 and BPR-0641 remain parked standalone follow-ups for unobserved live
mixed-client/state-loss, real review attribution and private-PR smoke acceptance.
The [RFC-0008 ledger](../verification/RFC-BPR-0008.md#owner-amended-closeout-r4--2026-09-21)
distinguishes that acceptance change from passing observations. The later RFCs
were not closed by that step.

The same explicit r4 assignment now closes RFC-BPR-0009 through BPR-0243.
Its [ledger](../verification/RFC-BPR-0009.md#owner-amended-closeout-r4--2026-09-21)
records passing lifecycle, persistence, Qt defect-card and shipped multi-defect
composition evidence. BPR-0153 remains a standalone parked follow-up for the
unobserved GitHub-only collaborator comprehension/reply criterion; no human
verdict is inferred. The existing broad workbench layout failure remains open
on BPR-0940, now a standalone r4 follow-up, and is not described as a passing suite.

RFC-BPR-0010 now closes through BPR-0244 under the same explicit r4 assignment.
Its [ledger](../verification/RFC-BPR-0010.md#owner-amended-closeout-r4--2026-09-21)
records current local queue, durable rule/preferences and notification
composition checks, preserving the exact accepted v006 native checkpoint.
BPR-0169 remains Parked for independent live server non-mutation observation;
BPR-0168 remains Backlog for terminal candidate identity replay and any required
successor native verdict after BPR-0169. Both are standalone follow-ups, and
neither pending result is inferred from RFC status.

RFC-BPR-0011 closes through BPR-0245 under the same r4 assignment. Its
[ledger](../verification/RFC-BPR-0011.md#owner-amended-closeout-r4--2026-09-21)
binds current local process/public-composition checks and exact historical
Windows/Linux evidence. Installed-agent compatibility, live agent identity
and human judgment of the existing-checkout wording remain unobserved optional
follow-ups. Native package qualification and the separately owned
BPR-0825/BPR-0875 obligations remain open; RFC status supplies none of those
observations.

RFC-BPR-0012 closes through BPR-0507 under r4, retaining its original nine
fixture/core/Qt acceptance criteria. Its
[ledger](../verification/RFC-BPR-0012.md#owner-amended-closeout-r4--2026-09-21)
records current finding, chat, workshop, MCP and ordinary synchronization
checks, and reconciles the completed historical fixes. The broader run's Dif
workshop-cue failure is preserved as standalone BPR-0966; no pass is inferred
for it, live external-agent behavior or native packages. That closure cleared
RFC-BPR-0013's predecessor gate.

RFC-BPR-0013 now closes through BPR-0617 under r4. Its
[ledger](../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
accepts the recorded bounded hardening scope and preserves all original wider
criteria as explicit follow-ups: BPR-0967–0972 for platform/performance/current
memory failures, rendered backoff/offline, fuzzing, pathological privacy and
broader structural evidence; BPR-0817/0825/0940/0944/0945 retain their existing
human/layout/comparison/tidy obligations. RFC0033 remains associated with
BPR-0817. No failed or missing observation is marked passing.

RFC-BPR-0014 is Auto-Verified under owner amendment r4 for its implemented
foundation, completing the original fourteen-RFC pack. BPR-0965 is Done for
that explicitly amended scope. RFC-BPR-0038 retains all nine original release
criteria and owns unchanged-criteria Blocked BPR-0958–0962 plus BPR-0973
closeout. The transfer supplies no native, trust, licensing, vulnerability or
publication acceptance.

The imported files in [plans](plans/README.md) are immutable planning source.
Live work is tracked on the [issue board](../issues/README.md).

RFC-BPR-0014's [extended shared plan](plans/rfc-bpr-0014-release-readiness/milestone-plan.json)
keeps M1–M11 unchanged, amends M12 under r4 and adds RFC0038's M13. Its
[verification ledger](../verification/RFC-BPR-0014.md) distinguishes inherited
icon/private-macOS-release evidence from pending clean Windows, macOS
Intel/Apple Silicon and Linux package/security acceptance. BPR-0965 retains
RFC-BPR-0013 and canonical closeout ordering. M8/M9 remain unaccepted under
the successor; completing RFC0014 does not complete the shared qualification plan.

## Additional RFCs

The table above is the complete fourteen-RFC pack-v2 sequence. The following
live RFCs are outside that original pack and cannot block its implementation or
closeout. Their status and eligibility are explicit below; unstarted work is
tracked on the issue board's backlog, in the same way as other RFC work.

| RFC | Title | Status | Eligibility |
|---|---|---|---|
| [RFC-BPR-0015](RFC-BPR-0015-github-cli-production-enablement.md) | Opt-in GitHub CLI Account Profile | Auto-Verified | Closed 2026-09-23 after RFC-BPR-0014 by [BPR-0788](../issues/60-done/BPR-0788-close-rfc-0015.md) for the qualified macOS scope: the six criteria were reconciled at `0f67560e` (r3) and rechecked at `c1a336a0` (r4). Owner amendment r4 approved the pending review of the CLI authority/protocol changes after `26f1c22e`; that is an owner decision, not an independent reviewer's report. Windows/Linux compose no CLI entry, no live pull request was written, and BPR-0803 remains separate Backlog work |
| [RFC-BPR-0016](RFC-BPR-0016-github-device-authorization-flow.md) | GitHub Device Authorization Flow | Auto-Verified | Closed 2026-09-04 at `0d19c990` by [BPR-0285](../issues/60-done/BPR-0285-anchor-rfc-0016-verification.md): RFC-BPR-0002 was Auto-Verified first, the recorded distributable identity is admitted, and the final-head course replayed green target-native on macOS, Linux, and Windows x64 |
| [RFC-BPR-0017](RFC-BPR-0017-unauthenticated-public-review-read.md) | Unauthenticated Public Review Read | Auto-Verified | Closed 2026-09-04 by BPR-0309 after RFC-BPR-0002 eligibility, validated owner build decision, focused macOS evidence and clean review of `d9006237`; macOS enabled, Linux/Windows unavailable |
| [RFC-BPR-0018](RFC-BPR-0018-robust-settings-center.md) | Robust Settings Center and Preference Ownership | Auto-Verified | RFC-0014 foundation dependency satisfied. BPR-0593 closes the recorded accepted scope under owner amendment r2; standalone BPR-0974 remains Blocked for Windows Defender/security disposition, full native course/current inventory, and BPR-0975 remains Blocked for Linux native focus/lifetime. No false-positive verdict, Linux course pass or RFC-0038 release qualification is inferred |
| [RFC-BPR-0019](RFC-BPR-0019-dif-visual-identity-sprite-assets.md) | Dif Visual Identity, Sprite Assets, and Animation Contract | Deferred | RFC-BPR-0006 and RFC-BPR-0013 closed, RFC-BPR-0014 closed or an explicit post-pack frontier exception, and confirmed asset ownership/provenance |
| [RFC-BPR-0020](RFC-BPR-0020-animated-dif-mascot-qt-composition.md) | Animated Dif Mascot Qt Presentation and Workbench Composition | Deferred | RFC-BPR-0019 closed, its live appearance/workbench/resource paths inventoried, and RFC-BPR-0014 closed or an explicit post-pack frontier exception |
| [RFC-BPR-0021](RFC-BPR-0021-conversation-revision-identity.md) | Conversation Revision Identity and the Comment-Code History Join | Deferred | RFC-BPR-0002 and RFC-BPR-0003 closed, and recorded probe evidence that a real service returns a per-comment commit — including unauthenticated and on GHES 3.18 |
| [RFC-BPR-0022](RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md) | Semantic Source-Line Comments and Off-Hunk Publication Compatibility | Deferred | Later explicit implementation request; dependency-ordered closeout and exact native/fallback evidence |
| [RFC-BPR-0023](RFC-BPR-0023-private-review-threads-provenance-comment-composition.md) | Private Review Threads, Provenance, and Comment Composition | Deferred | Later explicit implementation request; RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0011, and RFC-BPR-0012; shared publication integration follows RFC-BPR-0022 cards |
| [RFC-BPR-0024](RFC-BPR-0024-bpr-profiles-multiple-windows.md) | BPR Profiles and Multiple Windows | Auto-Verified | RFC-BPR-0018 closed first. BPR-0595 closes the recorded accepted scope under owner amendment r3 with owner accepts on the exact signed-out and native-geometry candidates and passing Windows station rows at `1cd7f2f1`; standalone BPR-0980 took the deferred native Windows desktop activation/`--new-window` observation on 2026-09-23, after correcting the owner's foreground right in `f73aa135`; that is the card's observation, not part of this RFC's closure, and no RFC-0038 release qualification is inferred. Owner amendment r4 (2026-09-23) keeps AC-07 closed although its restoration course was red at the close; the course passes again at `0ddf929b` and is bound on BPR-0595 as AC-07's current evidence |
| [RFC-BPR-0025](RFC-BPR-0025-review-workspace-navigation-browser-tabs.md) | Review Workspace Navigation and Browser-Style Tabs | Auto-Verified | Closed 2026-09-22 for the scope reconciled under RFC-BPR-0028 (plan `ux-redesign-pack-0028`): BPR-0981 (resident fuzzy filters with retained selection) and BPR-0985 (the reading-position restoration repair) are Done, and closeout [BPR-0597](../issues/60-done/BPR-0597-close-rfc-0025.md) records direct evidence for every surviving criterion. AC-04, the tab parts of AC-03, AC-05–AC-07 and AC-09, and AC-01's superseded chrome are recorded as superseded or unscheduled, not implemented and not passed; browser-style file tabs stay registered but unscheduled; RFC-BPR-0003, RFC-BPR-0004, RFC-BPR-0005, RFC-BPR-0006, and RFC-BPR-0024 |
| [RFC-BPR-0026](RFC-BPR-0026-personal-review-home-pr-url-navigation.md) | Personal Review Home and PR URL Navigation | Auto-Verified | Closed 2026-09-23 for the scope reconciled under plan `ux-redesign-pack-0028` ([BPR-0598](../issues/60-done/BPR-0598-inventory-personal-discovery-url-flow-and-plan.md)): BPR-0987 (exact link locations), BPR-0988 (personal discovery read), BPR-0989 (discovery scheduler and membership) and BPR-0990 (Home refresh, status, Follow and Save Repository, answering BPR-0801) are Done, and closeout [BPR-0599](../issues/60-done/BPR-0599-close-rfc-0026.md) records direct evidence for every criterion. AC-01's automatic restoration of other windows and its tabs are superseded by the owner's 2026-09-11 Home direction and RFC-BPR-0028, not passed; BPR-1017 (a new window's sign-in card, RFC-BPR-0024) is recorded outside this scope; RFC-BPR-0002, RFC-BPR-0007, RFC-BPR-0010, RFC-BPR-0016, RFC-BPR-0018, RFC-BPR-0024, and RFC-BPR-0025 |
| [RFC-BPR-0027](RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md) | Structured Diagnostic Logging, Verbosity, and Privacy | Deferred | RFC-BPR-0001 and RFC-BPR-0002; basic Quill foundation implemented in BPR-0602; advanced retention/privacy/Settings phase deferred |
| [RFC-BPR-0028](RFC-BPR-0028-review-workbench-direction.md) | Review Workbench Direction: Overview, Epoch Line, Modalities, and Conversation | Proposed | Owner-locked 2026-09-06; supersedes the RFC-BPR-0025 layout and RFC-BPR-0026 Home presentation where they conflict; Settings, explorers and file tree designs follow. 61 of 63 cards Done under plan `ux-redesign-pack-0028`. Open on 2026-09-23: closeout [BPR-1021](../issues/10-backlog/BPR-1021-close-rfc-0028.md) is in Backlog under milestone `close-rfc-0028`, waiting on BPR-0653 (the Windows title row, parked on human-review for its Windows attended capture, the private Qt API decision and the AC-01 decision) and BPR-0746 (Blocked on the declined cursor probe). AC-01 is contradicted on main by RFC-BPR-0037's composition entry row above the epoch line, which BPR-0940's window course now asserts; amending AC-01 or moving the entries is the owner's decision |
| [RFC-BPR-0029](RFC-BPR-0029-machine-independent-epoch-identity-update-history.md) | Machine-Independent Epoch Identity and Pull Request Update History | Accepted | Revised proposal accepted 2026-09-12 under mandate r17: only evidenced update occurrences receive portable identities; incomplete history and unknown historical bases remain explicit; migration is lossless even where identity is unresolved. RFC-BPR-0002, RFC-BPR-0003 and RFC-BPR-0017; supersedes RFC-BPR-0003 in part. RFC-BPR-0028's AC-02 no longer waits on it: its card BPR-0743 is Done (`641dae15`) and BPR-0646's epoch-handle ask was accepted on 2026-09-12 |
| [RFC-BPR-0030](RFC-BPR-0030-dif-production-animation-revision.md) | Dif Production Animation Revision: Ten Registered Prop-Free Clips | Auto-Verified | Closed in canonical order by BPR-0769: the exact ten-clip production revision, provenance, deterministic regeneration, measured bounds, and bound art/derivative decisions pass after RFC-BPR-0006 |
| [RFC-BPR-0031](RFC-BPR-0031-dif-workbench-animation-integration.md) | Dif Workbench Integration: Profile Preferences, Bounded Qt Playback, and Truthful Cues | Auto-Verified | Closed after RFC-BPR-0030 by BPR-0769: accepted art is composed on Home and the explicit AI tools surface with profile preferences, bounded cues, focused native/rendered evidence, resource/core-only checks, and qualified memory observations |
| [RFC-BPR-0032](RFC-BPR-0032-calm-home-and-accounts-surfaces.md) | Calm Home and Accounts & Hosts: One Language for the Surfaces Before a Review | Proposed | Imported 2026-09-13 from the owner's brief for later implementation, Home revised the same day after owner review; a presentation revision that implements the accepted RFC-BPR-0028 Settings and Home statements as one card per server and state-ordered Home panels with grouping, and records the owner's amendment placing Home's Group, State headers, Filter and refresh in the strip; RFC-BPR-0016, RFC-BPR-0018 and RFC-BPR-0028 |
| [RFC-BPR-0033](RFC-BPR-0033-profile-chooser-and-the-last-window.md) | Profile Chooser and the Last Window: Profiles Are a Menu, Not a Gate | Proposed | Imported 2026-09-13 for later implementation; removes the last-window chooser presentation, adds a per-platform empty-desktop policy and redesigns the chooser as rows; RFC-BPR-0024, whose text it amends in presentation only |
| [RFC-BPR-0034](RFC-BPR-0034-appearance-page-and-per-theme-colours.md) | Appearance Page and Per-Theme Colour Customisation | Auto-Verified | Closed 2026-09-22 by [BPR-0791](../issues/60-done/BPR-0791-close-rfc-0034.md) once RFC-BPR-0006 and RFC-BPR-0018 were `Auto-Verified`. The page is a theme strip, one-line rows with info-glyph popovers, a labelled preview and a colours card for the theme on screen, and the store keeps one customised palette per theme after a migration. The owner accepted the macOS candidate built from `ed2b3f14`, and the focused and end-to-end appearance courses pass again at `f06088b8`. The real macOS colour panel has no automated course, C1's two storage limitations stand, and C4's painter drift is standalone [BPR-0986](../issues/10-backlog/BPR-0986-paint-the-hunk-header-and-current-line-roles-in-the-review-panes.md) |
| [RFC-BPR-0035](RFC-BPR-0035-github-release-auto-updater.md) | GitHub Release Auto-Updater | Proposed | Milestones 1–3 accepted; M4 source `518c384a` fixes the native menu and documents recovery/security. Owner amendment r3 defers Intel validation to nonblocking BPR-0950; BPR-0949 is Done under its Apple Silicon scope. BPR-0911 is Blocked on live private-release/vault AC-03; BPR-0912 remains Backlog with RFC-BPR-0014/canonical-order gates. Full M4/RFC acceptance remains open |
| [RFC-BPR-0036](RFC-BPR-0036-native-markdown-rendering.md) | Native Markdown Rendering for Review Content | Proposed | Technical implementation accepted at `a3ab46ee`; native overview/public conversations and md4qt comparison recorded on BPR-0917; RFC-BPR-0008 is now closed, while canonical predecessor eligibility remains pending |
| [RFC-BPR-0037](RFC-BPR-0037-first-pr-review-workflow.md) | First-PR Review Workflow: Navigation, Source Loading and Conversations | Proposed | Original technical closeout accepted at `e4096e05`; BPR-0934/AC-10 folder controls accepted at `a88c0d19` and landed on main by merge `0ddf929b`, with test-only correction `b6e14081`. RFC-BPR-0008, RFC-BPR-0013 and RFC-BPR-0015 are now closed, while RFC-BPR-0028 and RFC-BPR-0029 must still become Auto-Verified; Markdown remains separate under RFC-BPR-0036 |
| [RFC-BPR-0038](RFC-BPR-0038-native-package-release-security-qualification.md) | Native Package and Release Security Qualification | Proposed | Owner r4 successor to RFC0014: unchanged Blocked BPR0958–0962 retain all native/trust/license/privacy/security gaps; BPR0973 owns closeout after actual qualification and canonical predecessors |
| [RFC-BPR-0039](RFC-BPR-0039-manual-first-review-tour-authoring.md) | Manual-First Review Tour Authoring | Proposed | Imported 2026-09-23 from the review-tour pack for later implementation; author prepares Problem, Solution and ordered tour stops without AI; RFC-BPR-0003, 0008, 0024, 0036 and 0037 |
| [RFC-BPR-0040](RFC-BPR-0040-github-native-tour-format-import-and-navigation.md) | GitHub-Native Tour Format, Import, and Navigation | Proposed | Imported 2026-09-23; the published tour is ordinary GitHub comments with a Start here index and Previous/Next/overview links, re-importable without hidden metadata |
| [RFC-BPR-0041](RFC-BPR-0041-staged-tour-publication-and-recovery.md) | Staged Tour Publication and Recovery | Proposed | Imported 2026-09-23; one native pending review staged, wired and submitted as COMMENT, with unknown outcomes reconciled rather than retried |
| [RFC-BPR-0042](RFC-BPR-0042-published-tour-maintenance-and-reconciliation.md) | Published Tour Maintenance and Reconciliation | Proposed | Imported 2026-09-23; later updates preserve comment identity and discussion, website edits are remote truth with preserved local conflicts |
| [RFC-BPR-0043](RFC-BPR-0043-scoped-review-preparation-ai-assistance.md) | Scoped Review Preparation AI Assistance | Proposed | Imported 2026-09-23; optional in-place undoable writing commands and skeleton-only outlines after the manual lifecycle; no bulk Ready or autonomous publication |

The 2026-09-04 daily-workbench registration accepts the design in RFC-BPR-0018
and RFC-BPR-0024 through RFC-BPR-0026, with implementation explicitly deferred.
The requested workstream order is **0018 → 0024 → 0025 → 0026** within the
canonical order above; it does not authorize bypassing earlier eligibility or
closeout gates. Start later with `$implement-rfc` for that exact sequence.
[[BPR-0592]] through [[BPR-0599]] are the unstarted entry/closeout pairs; the
later planning pass allocates additional bounded implementation cards as needed.
On 2026-09-23 that workstream is complete: RFC-BPR-0018, RFC-BPR-0024,
RFC-BPR-0025 and RFC-BPR-0026 are Auto-Verified for their reconciled scopes and
all eight entry and closeout cards are Done.

Owner direction, 2026-09-07: review and implement Claude's RFC-BPR-0028
redesign **and** the broader RFC-BPR-0018/0024/0025/0026 work. This explicitly
activates the post-pack implementation frontier in the requested order
0018 → 0024 → 0025 → 0026 on available targets. Reconcile existing 0028
implementation before adding new authority. Its superseding layout and Home
decisions remain authoritative; browser-style file tabs remain unscheduled.
Earlier dependency and target-native/human criteria still govern terminal
closeout. RFC statuses remain nonterminal while the entry inventories establish
bounded implementation cards and reuse relevant existing evidence.

The ownership split is Settings/direct sign-in/server management (0018), BPR
profile and window isolation/restoration (0024), independent browser-style
review tabs and fuzzy-filtered drawers (0025), and personal discovery/Home/URL
opening (0026). RFC-0018 initially binds the default environment; RFC-0024
extends it to one Settings window per BPR profile without a circular dependency.
The registration baseline and per-criterion not-run evidence are in each
closeout card. Accepted screenshots were design references, not runtime evidence.

The same registration moves the former `future/` RFCs into this directory.
Historical captured paths in immutable imported packs and answered review
evidence describe their original commits and are preserved as provenance;
live links and import destinations use the current paths. Moving a document
does not change its status or invalidate otherwise unchanged product evidence.

RFC-BPR-0016 and RFC-BPR-0017 address the same operator complaint by opposite
routes and are alternatives rather than companions. RFC-BPR-0017 records why
RFC-BPR-0016 is the stronger answer.

RFC-BPR-0018 adopts the interaction system of the product owner's
AllTheThings settings center while preserving BetterPullReview's typed domain
ownership. It is a presentation and preference-orchestration RFC, not authority
to copy unrelated AllTheThings settings or to centralize domain state.

RFC-BPR-0021 is the product owner's own request, accepted from a rendered
mockup on 2026-08-21 after two weeks recorded in [[BPR-0212]] without an RFC.
Its eligibility gate is unusual and deliberate: the design is complete but one
fact it rests on is unproven, because the review-thread selection this product
issues does not currently ask GitHub for a comment's commit. The RFC records
that as requiring probe evidence rather than assuming it, since the same datum
must also be readable unauthenticated for RFC-BPR-0017 and identically on GHES
3.18 for the fixture-security contract.

RFC-BPR-0019 and RFC-BPR-0020 are the ordered Dif mascot program. RFC-BPR-0019
creates the original, provenance-complete BPR asset and clip contract;
RFC-BPR-0020 alone may compose those reviewed assets into the Qt workbench and
real application. The product owner's Nib bootstrap contributes process
guidance only. Neither RFC authorizes importing Nib, Mel, AllTheThings art or
code, foreign animation architecture, or mascot-authored review conclusions.

RFC-BPR-0030 and RFC-BPR-0031 are the imported ten-clip Dif revision and
workbench integration. They partially supersede the earlier Dif contracts only
for their explicit deltas. Their [source pack and live ID
mapping](assets/bpr-dif-integration-v1.LIVE_IMPORT.md) and [adopted six-milestone
plan](plans/bpr-dif-integration-v1/milestone-plan.json) retain the source and
execution order. At import all eighteen cards were Backlog and both RFCs were
Proposed. BPR-0769 now records their dependency-ordered `Auto-Verified`
closeout without rewriting the immutable imported packet.

RFC-BPR-0039 through RFC-BPR-0043 are the imported review-tour pack: an
author-prepared guided review whose published form is ordinary GitHub comments,
usable by reviewers who never open BPR. The manual lifecycle (RFC-BPR-0039 to
0042) precedes the optional AI assistance of RFC-BPR-0043. Their [source pack and
live ID mapping](assets/bpr-review-tour-v1.LIVE_IMPORT.md) and [adopted
eight-milestone plan](plans/bpr-review-tour-v1/milestone-plan.json) retain the
source and execution order. Import leaves all twenty-six cards in Backlog and all
five RFCs Proposed; BPR-1016 owns the single ordered closeout.

RFC-BPR-0032, RFC-BPR-0033 and RFC-BPR-0034 are the imported shell-surfaces pack: the owner's
2026-09-13 verdict that Settings' Accounts & hosts page, Home and the profile
chooser are cluttered, that the chooser must stop appearing when the last
window closes, and, in the same day's review, that the Appearance page should
follow and each theme should keep its own colours. The first two are presentation
and composition revisions that implement
the accepted RFC-BPR-0028 Settings and Home statements and RFC-BPR-0018's row
language; they change no credential, provider, publication, queue, navigation,
profile-identity or runtime-ownership authority; RFC-BPR-0034 adds one domain change,
one customised palette per theme, with a store migration. Their [source pack and live ID
mapping](assets/bpr-shell-surfaces-v1.LIVE_IMPORT.md), [design
reference](assets/bpr-shell-surfaces-v1/design/shell-surfaces-v1.html) and
[adopted five-milestone plan](plans/bpr-shell-surfaces-v1/milestone-plan.json)
retain the source and execution order. Import leaves all seventeen cards in
Backlog and the three RFCs Proposed; the three closeouts are independent of each other.

On 2026-09-16 the owner reviewed the shipped macOS build and returned seventeen
verdicts through the attended-review workflow: sixteen acceptances and one
rejection, that closing the last window closes the menu bar, which is now
[BPR-0796](../issues/60-done/BPR-0796-keep-the-macos-menu-bar-when-the-last-window-closes.md).
They reviewed the fixed build the same day and rejected that too — "it stays
running in the bottom dock but not in the top menu bar" — which turned out to be
about which application owns the menu bar rather than about which menus are on
it, and which needed no further change to `src/`. The eighteenth verdict and its
adjudication are on that card. On the same day they answered the replacement
candidate `bpr-0796-empty-desktop-menu-bar-activation` with `accept` and the note
"I don't care about this right now. move on." — the nineteenth verdict, bound to
the fixed build's own digest. The attended gate B5 names is therefore cleared;
the note closes the question rather than reporting a fresh observation, and the
card says so.
RFC-BPR-0034 closed on 2026-09-22. Its own criteria were already met and its
closeout rows complete, and its only gate was RFC-BPR-0018. [BPR-0791](../issues/60-done/BPR-0791-close-rfc-0034.md)
records the recheck at the closing head. RFC-BPR-0032's own criteria are met
and its closeout rows are complete. It stays `Proposed` only because closure
is strictly ordered and RFC-BPR-0028 is not `Auto-Verified`. RFC-BPR-0033 also
stays `Proposed`. Its recorded gate, RFC-BPR-0024, is now `Auto-Verified`, but
two things remain. Its closeout rows have not been rechecked at the current
head. And [BPR-0817](../issues/30-in-progress/BPR-0817-a-linux-close-exits-1-when-the-app-created-its-data-folder-group-writable.md),
which is associated with it, is no longer waiting on the owner. On 2026-09-23
the owner delegated its two questions to its implementer, and the decisions are
recorded as an owner-delegated amendment on the card and in RFC-BPR-0033. The
implementation is open on that card. B5's attended clause is answered. The defect behind the
first rejection is fixed — the windowless menu bar now carries the same menus a
window's bar carries — and B1's macOS half and B2's native half are met on the
fixed build. The first console in this pack that was unlocked closed most of what
had been left to a person: the windowless bar is photographed and is the same
image as the windowed one, and the Dock tile, ⌘N, the Profiles menu and Quit were
all exercised on the real screen. What had been left to the owner — whether
the application was the front one when they closed that window — was put to them
on a replacement candidate bound to the same digest, and they closed the question
rather than answering it on its merits. The Windows and Linux halves of B1 were
later observed on target-native stations and pass for a fresh data root, on
[BPR-0798](../issues/60-done/BPR-0798-windows-native-empty-desktop-quit-evidence.md)
and
[BPR-0799](../issues/60-done/BPR-0799-linux-native-empty-desktop-quit-evidence.md).
[BPR-0782](../issues/60-done/BPR-0782-close-rfc-0033.md) still records them as
not run, because it predates those observations.

RFC-BPR-0022 and RFC-BPR-0023 were reconciled and imported together on
2026-09-04 with implementation explicitly deferred. The off-hunk pack retains
0022; the private-thread pack was renumbered to 0023. RFC-BPR-0022 owns semantic
line publication and native/file-permalink compatibility through RFC-BPR-0008.
RFC-BPR-0023 owns private turns, honest provenance, composition, and local
lineage; Squash to Draft stays local and Publish stays separate. Their shared
publication cards consume one logical receipt and distinguish approved prose
from the permalink-prefixed public body. RFC-BPR-0021 keeps the general
historical comment/revision join. The imported pack prompts and earlier
frontier language authorize no implementation of these two deferred RFCs.

RFC-BPR-0027 records the owner's request for useful local diagnostic logging,
Standard/Verbose detail and privacy/export controls inspired by AllTheThings.
Quill v13.0.0 supplies the implemented basic foundation in
[[BPR-0602]], after [[BPR-0600]] planning. [[BPR-0603]] retains deferred advanced
controls; [[BPR-0601]] remains the full closeout. No new live-run authority is
granted and partial implementation does not close the RFC. The foundation does not wait for RFC-BPR-0018 or
RFC-BPR-0024; those RFCs later compose the same diagnostic controls and scope.

RFC-BPR-0029 is the owner's own request. It grew from the RFC-BPR-0028 epoch
line he rejected in v008: an epoch is an update to the pull request, and its
identity is the same on every machine. Asked whether that should amend the
closed RFC-BPR-0003 or stand alone, he answered "It's own rfc" on 2026-09-12. Its
design is [[BPR-0743]]'s design pass. The RFC is Proposed and not accepted,
nothing is built, and its three owner asks come before any implementation. It
supersedes RFC-BPR-0003 in part only once accepted, and RFC-BPR-0003 is not
reopened.
