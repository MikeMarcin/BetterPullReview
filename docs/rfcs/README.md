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
| [RFC-BPR-0008](RFC-BPR-0008-github-review-round-trip-interactions.md) | GitHub Review Synchronization and Round-Trip Interactions | Proposed | RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007 |
| [RFC-BPR-0009](RFC-BPR-0009-defect-fix-verification-workflow.md) | Defect Classification, Fix Claims, and Verification | Proposed | RFC-BPR-0008 |
| [RFC-BPR-0010](RFC-BPR-0010-review-queue-observers-notifications.md) | Review Queue, Observer Rules, and Local Notifications | Proposed | RFC-BPR-0007, RFC-BPR-0008 |
| [RFC-BPR-0011](RFC-BPR-0011-ai-agent-profiles-context-supplements.md) | AI Agent Profiles, Launch, Authentication Isolation, and Context Supplements | Proposed | RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007 |
| [RFC-BPR-0012](RFC-BPR-0012-ai-review-chat-findings-mcp.md) | AI Review Findings, Comment Workshop, Side Chat, and MCP Context | Proposed | RFC-BPR-0008, RFC-BPR-0011 |
| [RFC-BPR-0013](RFC-BPR-0013-performance-resilience-resource-limits.md) | Performance, Resource Limits, Resilience, and Recovery | Proposed | RFC-BPR-0004, RFC-BPR-0005, RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0010, RFC-BPR-0012 |
| [RFC-BPR-0014](RFC-BPR-0014-cross-platform-packaging-security-release.md) | Cross-Platform Packaging, Signing, Security Hardening, and Release Readiness | Proposed | RFC-BPR-0013 |

The imported files in [plans](plans/README.md) are immutable planning source.
Live work is tracked on the [issue board](../issues/README.md).

## Additional RFCs

The table above is the complete fourteen-RFC pack-v2 sequence. The following
live RFCs are outside that original pack and cannot block its implementation or
closeout. Their status and eligibility are explicit below; unstarted work is
tracked on the issue board's backlog, in the same way as other RFC work.

| RFC | Title | Status | Eligibility |
|---|---|---|---|
| [RFC-BPR-0015](RFC-BPR-0015-github-cli-production-enablement.md) | GitHub CLI Production Enablement | Deferred | RFC-BPR-0014 closed and official `gh` supplies the required safety contract |
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
| [RFC-BPR-0030](RFC-BPR-0030-dif-production-animation-revision.md) | Dif Production Animation Revision: Ten Registered Prop-Free Clips | Proposed | Imported for later implementation; additive partial supersession of RFC-BPR-0019. Technical asset and provenance gates remain open; RFC-BPR-0006 is its formal dependency |
| [RFC-BPR-0031](RFC-BPR-0031-dif-workbench-animation-integration.md) | Dif Workbench Integration: Profile Preferences, Bounded Qt Playback, and Truthful Cues | Proposed | Imported for later implementation; consumes RFC-BPR-0030's accepted revision and depends formally on RFC-BPR-0005 and RFC-BPR-0006. No app or art acceptance is claimed |

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
workbench-integration proposal. They partially supersede the earlier Dif
contracts only for their explicit deltas. Their [source pack and live ID
mapping](assets/bpr-dif-integration-v1.LIVE_IMPORT.md) and [adopted six-milestone
plan](plans/bpr-dif-integration-v1/milestone-plan.json) retain the source and
execution order. Import leaves all eighteen cards in Backlog and both RFCs
Proposed; technical readiness and canonical formal closeout are separate.

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
