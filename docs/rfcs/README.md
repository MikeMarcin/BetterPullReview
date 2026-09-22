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
| [RFC-BPR-0010](RFC-BPR-0010-review-queue-observers-notifications.md) | Review Queue, Observer Rules, and Local Notifications | Proposed | RFC-BPR-0007, RFC-BPR-0008 |
| [RFC-BPR-0011](RFC-BPR-0011-ai-agent-profiles-context-supplements.md) | AI Agent Profiles, Launch, Authentication Isolation, and Context Supplements | Proposed | RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007 |
| [RFC-BPR-0012](RFC-BPR-0012-ai-review-chat-findings-mcp.md) | AI Review Findings, Comment Workshop, Side Chat, and MCP Context | Proposed | RFC-BPR-0008, RFC-BPR-0011 |
| [RFC-BPR-0013](RFC-BPR-0013-performance-resilience-resource-limits.md) | Performance, Resource Limits, Resilience, and Recovery | Proposed | RFC-BPR-0004, RFC-BPR-0005, RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0010, RFC-BPR-0012 |
| [RFC-BPR-0014](RFC-BPR-0014-cross-platform-packaging-security-release.md) | Cross-Platform Packaging, Signing, Security Hardening, and Release Readiness | Proposed | RFC-BPR-0013 |

On 2026-09-20 the owner closed RFC-BPR-0013's selected-diff investigation under
an explicit acceptance amendment: the unusual-target comparison-basis and
partial-pane anomaly is deferred to BPR-0944, not marked fixed. BPR-0794/0795
are Done. The [RFC-0013 reconciliation](../verification/RFC-BPR-0013.md)
retains its remaining wider acceptance gaps and the Proposed 0010/0012
predecessors; BPR-0617 therefore remains Backlog and RFC-0013 remains Proposed.
No new attended review or repeat owner loading check is requested by that amendment.

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
on BPR-0940 under RFC-0013 and is not described as a passing suite. RFC-BPR-0010
is the next canonical closure frontier; RFCs 0010–0014 retain their own gates.

The imported files in [plans](plans/README.md) are immutable planning source.
Live work is tracked on the [issue board](../issues/README.md).

RFC-BPR-0014's [live twelve-stage plan](plans/rfc-bpr-0014-release-readiness/milestone-plan.json)
now maps the authored pack-v2 stages to BPR-0951–0965. Its
[verification ledger](../verification/RFC-BPR-0014.md) distinguishes inherited
icon/private-macOS-release evidence from pending clean Windows, macOS
Intel/Apple Silicon and Linux package/security acceptance. BPR-0965 retains
RFC-BPR-0013 and canonical closeout ordering; implementation planning changes
no RFC status or native requirement.

## Additional RFCs

The table above is the complete fourteen-RFC pack-v2 sequence. The following
live RFCs are outside that original pack and cannot block its implementation or
closeout. Their status and eligibility are explicit below; unstarted work is
tracked on the issue board's backlog, in the same way as other RFC work.

| RFC | Title | Status | Eligibility |
|---|---|---|---|
| [RFC-BPR-0015](RFC-BPR-0015-github-cli-production-enablement.md) | Opt-in GitHub CLI Account Profile | Proposed | Mac CLI read and fixture publication qualified; RFC-BPR-0014 and remaining closeout evidence gate canonical status |
| [RFC-BPR-0016](RFC-BPR-0016-github-device-authorization-flow.md) | GitHub Device Authorization Flow | Auto-Verified | Closed 2026-09-04 at `0d19c990` by [BPR-0285](../issues/60-done/BPR-0285-anchor-rfc-0016-verification.md): RFC-BPR-0002 was Auto-Verified first, the recorded distributable identity is admitted, and the final-head course replayed green target-native on macOS, Linux, and Windows x64 |
| [RFC-BPR-0017](RFC-BPR-0017-unauthenticated-public-review-read.md) | Unauthenticated Public Review Read | Auto-Verified | Closed 2026-09-04 by BPR-0309 after RFC-BPR-0002 eligibility, validated owner build decision, focused macOS evidence and clean review of `d9006237`; macOS enabled, Linux/Windows unavailable |
| [RFC-BPR-0018](RFC-BPR-0018-robust-settings-center.md) | Robust Settings Center and Preference Ownership | Proposed | RFC-BPR-0014 closed or an explicit post-pack frontier exception, plus a complete existing-setting authority and persistence inventory |
| [RFC-BPR-0019](RFC-BPR-0019-dif-visual-identity-sprite-assets.md) | Dif Visual Identity, Sprite Assets, and Animation Contract | Deferred | RFC-BPR-0006 and RFC-BPR-0013 closed, RFC-BPR-0014 closed or an explicit post-pack frontier exception, and confirmed asset ownership/provenance |
| [RFC-BPR-0020](RFC-BPR-0020-animated-dif-mascot-qt-composition.md) | Animated Dif Mascot Qt Presentation and Workbench Composition | Deferred | RFC-BPR-0019 closed, its live appearance/workbench/resource paths inventoried, and RFC-BPR-0014 closed or an explicit post-pack frontier exception |
| [RFC-BPR-0021](RFC-BPR-0021-conversation-revision-identity.md) | Conversation Revision Identity and the Comment-Code History Join | Deferred | RFC-BPR-0002 and RFC-BPR-0003 closed, and recorded probe evidence that a real service returns a per-comment commit — including unauthenticated and on GHES 3.18 |
| [RFC-BPR-0022](RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md) | Semantic Source-Line Comments and Off-Hunk Publication Compatibility | Deferred | Later explicit implementation request; dependency-ordered closeout and exact native/fallback evidence |
| [RFC-BPR-0023](RFC-BPR-0023-private-review-threads-provenance-comment-composition.md) | Private Review Threads, Provenance, and Comment Composition | Deferred | Later explicit implementation request; RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0011, and RFC-BPR-0012; shared publication integration follows RFC-BPR-0022 cards |
| [RFC-BPR-0024](RFC-BPR-0024-bpr-profiles-multiple-windows.md) | BPR Profiles and Multiple Windows | Deferred | Design accepted; later implementation request; RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007, RFC-BPR-0008, and RFC-BPR-0018 |
| [RFC-BPR-0025](RFC-BPR-0025-review-workspace-navigation-browser-tabs.md) | Review Workspace Navigation and Browser-Style Tabs | Deferred | Design accepted; later implementation request; RFC-BPR-0003, RFC-BPR-0004, RFC-BPR-0005, RFC-BPR-0006, and RFC-BPR-0024 |
| [RFC-BPR-0026](RFC-BPR-0026-personal-review-home-pr-url-navigation.md) | Personal Review Home and PR URL Navigation | Deferred | Design accepted; later implementation request; RFC-BPR-0002, RFC-BPR-0007, RFC-BPR-0010, RFC-BPR-0016, RFC-BPR-0018, RFC-BPR-0024, and RFC-BPR-0025 |
| [RFC-BPR-0027](RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md) | Structured Diagnostic Logging, Verbosity, and Privacy | Deferred | RFC-BPR-0001 and RFC-BPR-0002; basic Quill foundation implemented in BPR-0602; advanced retention/privacy/Settings phase deferred |
| [RFC-BPR-0028](RFC-BPR-0028-review-workbench-direction.md) | Review Workbench Direction: Overview, Epoch Line, Modalities, and Conversation | Proposed | Owner-locked 2026-09-06; supersedes the RFC-BPR-0025 layout and RFC-BPR-0026 Home presentation where they conflict; Settings, explorers and file tree designs follow. Nine human-pending cards are prepared as one attended session at the pack head (`docs/verification/reviews/rfc-bpr-0028-attended-v002/`) under plan `ux-redesign-pack-0028`. BPR-0653 waits for Windows evidence |
| [RFC-BPR-0029](RFC-BPR-0029-machine-independent-epoch-identity-update-history.md) | Machine-Independent Epoch Identity and Pull Request Update History | Accepted | Revised proposal accepted 2026-09-12 under mandate r17: only evidenced update occurrences receive portable identities; incomplete history and unknown historical bases remain explicit; migration is lossless even where identity is unresolved. RFC-BPR-0002, RFC-BPR-0003 and RFC-BPR-0017; supersedes RFC-BPR-0003 in part. RFC-BPR-0028's AC-02 waits on its card BPR-0743 |
| [RFC-BPR-0030](RFC-BPR-0030-dif-production-animation-revision.md) | Dif Production Animation Revision: Ten Registered Prop-Free Clips | Auto-Verified | Closed in canonical order by BPR-0769: the exact ten-clip production revision, provenance, deterministic regeneration, measured bounds, and bound art/derivative decisions pass after RFC-BPR-0006 |
| [RFC-BPR-0031](RFC-BPR-0031-dif-workbench-animation-integration.md) | Dif Workbench Integration: Profile Preferences, Bounded Qt Playback, and Truthful Cues | Auto-Verified | Closed after RFC-BPR-0030 by BPR-0769: accepted art is composed on Home and the explicit AI tools surface with profile preferences, bounded cues, focused native/rendered evidence, resource/core-only checks, and qualified memory observations |
| [RFC-BPR-0032](RFC-BPR-0032-calm-home-and-accounts-surfaces.md) | Calm Home and Accounts & Hosts: One Language for the Surfaces Before a Review | Proposed | Imported 2026-09-13 from the owner's brief for later implementation, Home revised the same day after owner review; a presentation revision that implements the accepted RFC-BPR-0028 Settings and Home statements as one card per server and state-ordered Home panels with grouping, and records the owner's amendment placing Home's Group, State headers, Filter and refresh in the strip; RFC-BPR-0016, RFC-BPR-0018 and RFC-BPR-0028 |
| [RFC-BPR-0033](RFC-BPR-0033-profile-chooser-and-the-last-window.md) | Profile Chooser and the Last Window: Profiles Are a Menu, Not a Gate | Proposed | Imported 2026-09-13 for later implementation; removes the last-window chooser presentation, adds a per-platform empty-desktop policy and redesigns the chooser as rows; RFC-BPR-0024, whose text it amends in presentation only |
| [RFC-BPR-0034](RFC-BPR-0034-appearance-page-and-per-theme-colours.md) | Appearance Page and Per-Theme Colour Customisation | Proposed | Imported 2026-09-13 after owner review of the Appearance page; rebuilds the page as a theme strip, one-line rows with info-glyph popovers, a labelled preview and a colours card for the theme on screen, and changes the appearance store to one customised palette per theme with a migration; RFC-BPR-0006 and RFC-BPR-0018 |
| [RFC-BPR-0035](RFC-BPR-0035-github-release-auto-updater.md) | GitHub Release Auto-Updater | Proposed | Milestones 1–3 accepted; M4 source `518c384a` fixes the native menu and documents recovery/security. Owner amendment r3 defers Intel validation to nonblocking BPR-0950; BPR-0949 is Done under its Apple Silicon scope. BPR-0911 is Blocked on live private-release/vault AC-03; BPR-0912 remains Backlog with RFC-BPR-0014/canonical-order gates. Full M4/RFC acceptance remains open |
| [RFC-BPR-0036](RFC-BPR-0036-native-markdown-rendering.md) | Native Markdown Rendering for Review Content | Proposed | Technical implementation accepted at `a3ab46ee`; native overview/public conversations and md4qt comparison recorded on BPR-0917; RFC-BPR-0008 is now closed, while canonical predecessor eligibility remains pending |
| [RFC-BPR-0037](RFC-BPR-0037-first-pr-review-workflow.md) | First-PR Review Workflow: Navigation, Source Loading and Conversations | Proposed | Technical implementation and BPR-0927 closeout accepted at `e4096e05`; RFC-BPR-0008 is now closed, while RFC-BPR-0013, RFC-BPR-0015, RFC-BPR-0028 and RFC-BPR-0029 must still become Auto-Verified; Markdown remains separate under RFC-BPR-0036 |

The 2026-09-04 daily-workbench registration accepts the design in RFC-BPR-0018
and RFC-BPR-0024 through RFC-BPR-0026, with implementation explicitly deferred.
The requested workstream order is **0018 → 0024 → 0025 → 0026** within the
canonical order above; it does not authorize bypassing earlier eligibility or
closeout gates. Start later with `$implement-rfc` for that exact sequence.
[[BPR-0592]] through [[BPR-0599]] are the unstarted entry/closeout pairs; the
later planning pass allocates additional bounded implementation cards as needed.

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
All three RFCs stay `Proposed`, and none of the three is waiting on evidence
that was simply never gathered. RFC-BPR-0032's and RFC-BPR-0034's own criteria
are met and their closeout rows are complete; they stay `Proposed` only because
closure is strictly ordered and RFC-BPR-0018 — and, for RFC-BPR-0032, also
RFC-BPR-0028 — are not `Auto-Verified`. RFC-BPR-0033 now stays `Proposed` for one
reason only: RFC-BPR-0024 is `Deferred`. B5's attended clause is answered. The defect behind the
first rejection is fixed — the windowless menu bar now carries the same menus a
window's bar carries — and B1's macOS half and B2's native half are met on the
fixed build. The first console in this pack that was unlocked closed most of what
had been left to a person: the windowless bar is photographed and is the same
image as the windowed one, and the Dock tile, ⌘N, the Profiles menu and Quit were
all exercised on the real screen. What had been left to the owner — whether
the application was the front one when they closed that window — was put to them
on a replacement candidate bound to the same digest, and they closed the question
rather than answering it on its merits. The Windows and Linux halves of B1 have never been observed
and are recorded as not run, on
[BPR-0798](../issues/60-done/BPR-0798-windows-native-empty-desktop-quit-evidence.md)
and
[BPR-0799](../issues/60-done/BPR-0799-linux-native-empty-desktop-quit-evidence.md).

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
