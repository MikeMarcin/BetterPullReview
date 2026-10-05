# BetterPullReview RFC Index

Current delivery priority (owner direction, 2026-09-06): follow the
[private-alpha route](../development/private-alpha.md) to a personal work build
for GitHub.com and GitHub Enterprise. Reuse implemented surfaces, address only
the essential reading/publication gaps, and defer nonessential features and
hardening. This bounded delivery priority does not change RFC statuses or
explicit RFC dependencies, and does not by itself activate the deferred redesign RFCs.

RFCs close when their acceptance criteria and every explicit RFC dependency are
satisfied. Index position and RFC number do not impose a global start or closeout
order. A dependent RFC does not become `Auto-Verified` before every listed
dependency is `Auto-Verified`.

Owner amendment, 2026-09-27: the canonical-order rule is outdated and removed
globally. The project workflow now uses dependency sequencing. Historical order
statements in cards, plans and evidence do not reinstate that obsolete global
gate. Separately recorded owner scheduling decisions, authored milestone
prerequisites, scoped pack order and required human/native acceptance remain in
force; this amendment alone does not activate deferred work.

All live RFCs reside directly in `docs/rfcs/`, regardless of status or milestone.
There is no `future/` status folder. RFC front matter and this index carry RFC
status; issue-card lanes carry implementation backlog and progress. `plans/`
and imported `assets/` remain source/reference material, not alternate live RFCs.

Historical original-pack delivery exception (retained for provenance): real
Windows x64 evidence retained by BPR-0036 was
temporarily unavailable. RFC-BPR-0002 was then the closure frontier,
but that hardware-only gate does not block implementation, tests,
documentation, security review, simplification, or otherwise authorized work on
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
closeout with unmet explicit dependencies, or make GitHub Actions an evidence
surface. Every listed dependency must be `Auto-Verified` before closeout.

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
public-path/safety checks and documented source-pin reconciliation. BPR-0143
and BPR-0144 remain parked standalone follow-ups for unobserved live
mixed-client/state-loss and real review attribution. BPR-0641's private-PR
smoke was accepted by the owner on 2026-09-25 (session 06ad5734) and the card
is Done; the file comparison that did not render is BPR-1199.
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
the explicit RFC-BPR-0013 dependency. M8/M9 remain unaccepted under
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
| [RFC-BPR-0019](RFC-BPR-0019-dif-visual-identity-sprite-assets.md) | Dif Visual Identity, Sprite Assets, and Animation Contract | Auto-Verified | Closed 2026-09-26 through [BPR-0295](../issues/60-done/BPR-0295-anchor-rfc-0019-verification.md) after RFC-BPR-0006/0013 and all earlier canonical RFCs. The exact v008 AC1 amendment accepts disclosed provenance gaps; RFC-0030/0031 govern changed clips, budgets, regeneration and runtime selection. The [final ledger](../verification/RFC-BPR-0019.md#final-reconciliation--2026-09-26) binds fresh graph evidence and resolved independent findings; no package or new native visual qualification |
| [RFC-BPR-0020](RFC-BPR-0020-animated-dif-mascot-qt-composition.md) | Animated Dif Mascot Qt Presentation and Workbench Composition | Auto-Verified | Closed 2026-09-26 through [BPR-1220](../issues/60-done/BPR-1220-close-rfc-0020.md) after RFC-BPR-0019 and every earlier canonical RFC. The [ledger](../verification/RFC-BPR-0020.md) reconciles all eleven original criteria with accepted RFC-0030/0031 deltas and the later 28-pixel corner verdict; fresh motion, appearance, public composition and scale checks pass. Windows/Linux System observation remains unsupported; BPR-0966's Windows cue follow-up and package qualification remain separate |
| [RFC-BPR-0021](RFC-BPR-0021-conversation-revision-identity.md) | Conversation Revision Identity and the Comment-Code History Join | Deferred | RFC-BPR-0002 and RFC-BPR-0003 closed, and recorded probe evidence that a real service returns a per-comment commit — including unauthenticated and on GHES 3.18 |
| [RFC-BPR-0022](RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md) | Semantic Source-Line Comments and Off-Hunk Publication Compatibility | Auto-Verified | Closed 2026-09-27 under [owner amendments r4](../issues/_checkpoints/BPR-0575/m4-r4-amendments.md); fifteen criteria and BPR-0575 accepted at source0f3188093. Exact fallback, GET-only recovery and GitHub reply continuity observed; native-success/GHES, installed-uninstall and Windows limits retained. Owner acceptance substitutes for separate independent review; D main integration pending coordinator. |
| [RFC-BPR-0023](RFC-BPR-0023-private-review-threads-provenance-comment-composition.md) | Private Review Threads, Provenance, and Comment Composition | Auto-Verified | Closed 2026-09-27 by [BPR-0591](../issues/60-done/BPR-0591-anchor-rfc-0023-verification.md): all sixteen cards Done, eighteen direct criteria and architecture claims passed, final independent review findings resolved, designated GitHub publication/readback and linked ledger-loss read supplied; dependencies 0007/0008/0011/0012 Auto-Verified |
| [RFC-BPR-0024](RFC-BPR-0024-bpr-profiles-multiple-windows.md) | BPR Profiles and Multiple Windows | Auto-Verified | RFC-BPR-0018 closed first. BPR-0595 closes the recorded accepted scope under owner amendment r3 with owner accepts on the exact signed-out and native-geometry candidates and passing Windows station rows at `1cd7f2f1`; standalone BPR-0980 took the deferred native Windows desktop activation/`--new-window` observation on 2026-09-23, after correcting the owner's foreground right in `f73aa135`; that is the card's observation, not part of this RFC's closure, and no RFC-0038 release qualification is inferred. Owner amendment r4 (2026-09-23) keeps AC-07 closed although its restoration course was red at the close; the course passes again at `0ddf929b` and is bound on BPR-0595 as AC-07's current evidence |
| [RFC-BPR-0025](RFC-BPR-0025-review-workspace-navigation-browser-tabs.md) | Review Workspace Navigation and Browser-Style Tabs | Auto-Verified | Closed 2026-09-22 for the scope reconciled under RFC-BPR-0028 (plan `ux-redesign-pack-0028`): BPR-0981 (resident fuzzy filters with retained selection) and BPR-0985 (the reading-position restoration repair) are Done, and closeout [BPR-0597](../issues/60-done/BPR-0597-close-rfc-0025.md) records direct evidence for every surviving criterion. AC-04, the tab parts of AC-03, AC-05–AC-07 and AC-09, and AC-01's superseded chrome are recorded as superseded or unscheduled, not implemented and not passed; browser-style file tabs stay registered but unscheduled; RFC-BPR-0003, RFC-BPR-0004, RFC-BPR-0005, RFC-BPR-0006, and RFC-BPR-0024 |
| [RFC-BPR-0026](RFC-BPR-0026-personal-review-home-pr-url-navigation.md) | Personal Review Home and PR URL Navigation | Auto-Verified | Closed 2026-09-23 for the scope reconciled under plan `ux-redesign-pack-0028` ([BPR-0598](../issues/60-done/BPR-0598-inventory-personal-discovery-url-flow-and-plan.md)): BPR-0987 (exact link locations), BPR-0988 (personal discovery read), BPR-0989 (discovery scheduler and membership) and BPR-0990 (Home refresh, status, Follow and Save Repository, answering BPR-0801) are Done, and closeout [BPR-0599](../issues/60-done/BPR-0599-close-rfc-0026.md) records direct evidence for every criterion. AC-01's automatic restoration of other windows and its tabs are superseded by the owner's 2026-09-11 Home direction and RFC-BPR-0028, not passed; BPR-1017 (a new window's sign-in card, RFC-BPR-0024) was recorded outside this scope and is Done at `bfd927e7`; RFC-BPR-0002, RFC-BPR-0007, RFC-BPR-0010, RFC-BPR-0016, RFC-BPR-0018, RFC-BPR-0024, and RFC-BPR-0025 |
| [RFC-BPR-0027](RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md) | Structured Diagnostic Logging, Verbosity, and Privacy | Auto-Verified | Closed under owner amendment r3, 2026-09-27: the obsolete global canonical-order rule is removed; explicit RFC-BPR-0001/0002 dependencies are Auto-Verified. BPR-0601 records all nine product criteria and the three exact human Accept judgments. M1–M5 are landed; the RFC-0027 ledger preserves native developer, unpublished package and crash limits |
| [RFC-BPR-0028](RFC-BPR-0028-review-workbench-direction.md) | Review Workbench Direction: Overview, Epoch Line, Modalities, and Conversation | Proposed | Owner-locked 2026-09-06; supersedes the RFC-BPR-0025 layout and RFC-BPR-0026 Home presentation where they conflict; Settings, explorers and file tree designs follow. 62 of 63 cards Done under plan `ux-redesign-pack-0028`. Open on 2026-09-24: closeout [BPR-1021](../issues/10-backlog/BPR-1021-close-rfc-0028.md) is in Backlog under milestone `close-rfc-0028`, waiting only on BPR-0653 (the Windows title row, parked on human-review for its Windows attended capture). The owner's verdicts of 2026-09-24 amended AC-01 to admit the review surface's entry rows between the strip and the epoch line (with the standalone design follow-up BPR-1106), accepted Qt's private Windows native interface, and withdrew BPR-0746's unobtainable AC1 and AC2 by amendment, so BPR-0746 is Done |
| [RFC-BPR-0029](RFC-BPR-0029-machine-independent-epoch-identity-update-history.md) | Machine-Independent Epoch Identity and Pull Request Update History | Auto-Verified | Closed 2026-09-23 by [BPR-0750](../issues/60-done/BPR-0750-close-rfc-0029.md) once RFC-BPR-0008 closed and its own dependencies RFC-BPR-0002, RFC-BPR-0003 and RFC-BPR-0017 were `Auto-Verified`. All nine criteria have direct evidence; the closing pass at `58a2d885` was 24/24 across core, host, provider, SQLite, migration, Qt and composed-application rows. Only evidenced update occurrences receive portable identities; incomplete history and unknown historical bases remain explicit; migration is lossless even where identity is unresolved. Supersedes RFC-BPR-0003 in part. The GHES-origin legacy catalogue is synthetic and the agent-process course is macOS-local |
| [RFC-BPR-0030](RFC-BPR-0030-dif-production-animation-revision.md) | Dif Production Animation Revision: Ten Registered Prop-Free Clips | Auto-Verified | Closed in canonical order by BPR-0769: the exact ten-clip production revision, provenance, deterministic regeneration, measured bounds, and bound art/derivative decisions pass after RFC-BPR-0006 |
| [RFC-BPR-0031](RFC-BPR-0031-dif-workbench-animation-integration.md) | Dif Workbench Integration: Profile Preferences, Bounded Qt Playback, and Truthful Cues | Auto-Verified | Closed after RFC-BPR-0030 by BPR-0769: accepted art is composed on Home and the explicit AI tools surface with profile preferences, bounded cues, focused native/rendered evidence, resource/core-only checks, and qualified memory observations. Owner amendment 2026-09-25 (placement only; criteria text kept): Dif leaves the conversations drawer and rests in the status bar's corner unless a screen gives him a prime spot, built by BPR-1209 |
| [RFC-BPR-0032](RFC-BPR-0032-calm-home-and-accounts-surfaces.md) | Calm Home and Accounts & Hosts: One Language for the Surfaces Before a Review | Proposed | Imported 2026-09-13 from the owner's brief for later implementation, Home revised the same day after owner review; a presentation revision that implements the accepted RFC-BPR-0028 Settings and Home statements as one card per server and state-ordered Home panels with grouping, and records the owner's amendment placing Home's Group, State headers, Filter and refresh in the strip; RFC-BPR-0016, RFC-BPR-0018 and RFC-BPR-0028 |
| [RFC-BPR-0033](RFC-BPR-0033-profile-chooser-and-the-last-window.md) | Profile Chooser and the Last Window: Profiles Are a Menu, Not a Gate | Auto-Verified | Closed 2026-09-23 by [BPR-0782](../issues/60-done/BPR-0782-close-rfc-0033.md) once RFC-BPR-0024 was `Auto-Verified`. Closing the last window presents no chooser; macOS stays resident with its full menu bar, and Windows and Linux quit at exit 0 ([BPR-0798](../issues/60-done/BPR-0798-windows-native-empty-desktop-quit-evidence.md), [BPR-0799](../issues/60-done/BPR-0799-linux-native-empty-desktop-quit-evidence.md), and BPR-0817 under the owner-delegated amendment). The chooser is a sheet of rows. The closing recheck at `871c1185` passed 18/18 after [BPR-1035](../issues/60-done/BPR-1035-the-windowless-macos-menu-bar-drops-the-pull-request-menu.md) restored the windowless bar's Pull Request menu. The owner's 2026-09-16 verdicts stay bound to their candidates, and native activation was last observed unlocked at `a78eefcf` |
| [RFC-BPR-0034](RFC-BPR-0034-appearance-page-and-per-theme-colours.md) | Appearance Page and Per-Theme Colour Customisation | Auto-Verified | Closed 2026-09-22 by [BPR-0791](../issues/60-done/BPR-0791-close-rfc-0034.md) once RFC-BPR-0006 and RFC-BPR-0018 were `Auto-Verified`. The page is a theme strip, one-line rows with info-glyph popovers, a labelled preview and a colours card for the theme on screen, and the store keeps one customised palette per theme after a migration. The owner accepted the macOS candidate built from `ed2b3f14`, and the focused and end-to-end appearance courses pass again at `f06088b8`. The real macOS colour panel has no automated course, C1's two storage limitations stand, and C4's painter drift is standalone [BPR-0986](../issues/60-done/BPR-0986-paint-the-hunk-header-and-current-line-roles-in-the-review-panes.md). On 2026-09-24 the owner amended C5 from 47 offered roles to 45 (the two hunk-header roles leave the card, not the theme documents; BPR-0986 builds it) and rejected the panes' tab markers for Sublime Text's white-space model with a setting (BPR-1107) |
| [RFC-BPR-0035](RFC-BPR-0035-github-release-auto-updater.md) | GitHub Release Auto-Updater | Proposed | Milestones 1–3 accepted; M4 source `518c384a` fixes the native menu and documents recovery/security. Owner amendment r3 defers Intel validation to nonblocking BPR-0950; BPR-0949 is Done under its Apple Silicon scope. BPR-0911 is Blocked on live private-release/vault AC-03; BPR-0912 remains Backlog with the explicit RFC-BPR-0014 dependency. Full M4/RFC acceptance remains open |
| [RFC-BPR-0036](RFC-BPR-0036-native-markdown-rendering.md) | Native Markdown Rendering for Review Content | Proposed | Technical implementation accepted at `a3ab46ee`; native overview/public conversations and md4qt comparison recorded on BPR-0917; RFC-BPR-0008 is now closed; final acceptance reconciliation remains on BPR-0917 under the explicit dependencies |
| [RFC-BPR-0037](RFC-BPR-0037-first-pr-review-workflow.md) | First-PR Review Workflow: Navigation, Source Loading and Conversations | Proposed | Original technical closeout accepted at `e4096e05`; BPR-0934/AC-10 folder controls accepted at `a88c0d19` and landed on main by merge `0ddf929b`, with test-only correction `b6e14081`. RFC-BPR-0008, RFC-BPR-0013, RFC-BPR-0015 and RFC-BPR-0029 are now closed, while RFC-BPR-0028 must still become Auto-Verified; Markdown remains separate under RFC-BPR-0036 |
| [RFC-BPR-0038](RFC-BPR-0038-native-package-release-security-qualification.md) | Native Package and Release Security Qualification | Proposed | Owner r4 successor to RFC0014: unchanged Blocked BPR0958–0962 retain all native/trust/license/privacy/security gaps; BPR0973 owns closeout after actual qualification and its explicit RFC0014 dependency |
| [RFC-BPR-0052](RFC-BPR-0052-review-workbench-scaffolding.md) | Review Workbench Scaffolding: Title Row, Review Bar, Folding Updates Line and Document Tabs | Proposed | Registered 2026-09-24 from the owner's decision on [BPR-1106](../issues/60-done/BPR-1106-a-design-round-for-where-the-composition-entries-sit-in-the-review-chrome.md): ordered after RFC-BPR-0038 and before the review-tour pack, which builds on it; supersedes RFC-BPR-0028's chrome where they conflict and schedules RFC-BPR-0025's tabs; BPR-1129 to BPR-1134 are its slices and BPR-1135 owns the closeout. Owner amendment 2026-09-25: active now, ahead of canonical order ("The design session is complete, add RFC-BPR-0052 to the active work which should include BPR-1129 to BPR-1135"); decision `1d9104f8` originally held RFC-BPR-0039 to 0051 until RFC-BPR-0028 to 0038 close; the 2026-09-26 amendment below activates RFC-BPR-0039 to 0043 only. [Milestone plan](plans/rfc-bpr-0052-review-workbench-scaffolding/MILESTONES.md): BPR-1129, 1131 and 1134, then 1130, 1132 and 1133, then 1201 to 1207 (owner-approved plan amendment, 2026-09-25), then the closeout. On 2026-09-25 the owner answered all fourteen macOS questions on the m2 build (session 06ad5734): AC-03 amended to file before change, the tab strip amended (the plus activates its tab; tabs fill, shrink, then scroll), the kept-draft page and the Findings name rejected, and seven follow-up slices BPR-1201 to 1207 added before closeout; BPR-1206's width floor decided under the owner's delegation (Xcode's tabs). Owner amendment the same day on the second tray tab: it is the reviewer's private space for non-public notes and AI conversations and review feedback, promoted to drafts in Comments; its name (Notes), the thread controls' form and Dif's placement were decided under the owner's delegation, and BPR-1208 to 1210 join m2b |
| [RFC-BPR-0039](RFC-BPR-0039-manual-first-review-tour-authoring.md) | Manual-First Review Tour Authoring | Auto-Verified | Closed under [owner amendment r2](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md), 2026-09-27, by [BPR-1016](../issues/60-done/BPR-1016-close-rfc-0043.md). Manual authoring and seven exact Mac candidates accepted; other stations remain Blocked BPR-1224; original 0036/0037 formal acceptance remains BPR-1226. |
| [RFC-BPR-0040](RFC-BPR-0040-github-native-tour-format-import-and-navigation.md) | GitHub-Native Tour Format, Import, and Navigation | Auto-Verified | Closed under [owner amendment r2](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md), 2026-09-27, by [BPR-1016](../issues/60-done/BPR-1016-close-rfc-0043.md). Ordinary Markdown, exact links, cold import and navigation verified through production-port fixtures; live GitHub.com/GHES website courses remain Blocked BPR-1222/1223; original 0021/0022 acceptance remains BPR-1226. |
| [RFC-BPR-0041](RFC-BPR-0041-staged-tour-publication-and-recovery.md) | Staged Tour Publication and Recovery | Auto-Verified | Closed under [owner amendment r2](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md), 2026-09-27, by [BPR-1016](../issues/60-done/BPR-1016-close-rfc-0043.md). Staging, exact submission, compatibility and recovery verified through production-port fixtures; all unqualified production Tour operations remain unavailable pending Blocked BPR-1222/1223. |
| [RFC-BPR-0042](RFC-BPR-0042-published-tour-maintenance-and-reconciliation.md) | Published Tour Maintenance and Reconciliation | Auto-Verified | Closed under [owner amendment r2](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md), 2026-09-27, by [BPR-1016](../issues/60-done/BPR-1016-close-rfc-0043.md). Maintenance, conflict/restart, identity/reply preservation and cold reimport verified through production-port fixtures; real website continuation remains Blocked BPR-1222/1223. |
| [RFC-BPR-0043](RFC-BPR-0043-scoped-review-preparation-ai-assistance.md) | Scoped Review Preparation AI Assistance | Auto-Verified | Closed under [owner amendment r2](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md), 2026-09-27, by [BPR-1016](../issues/60-done/BPR-1016-close-rfc-0043.md). Scoped writing, native undo/settings/process delivery and outline/adversarial policy verified; installed-provider/model evidence remains Blocked BPR-1225. |
| [RFC-BPR-0044](RFC-BPR-0044-headless-tour-runtime-and-terminal-cli.md) | Headless Tour Runtime and Terminal CLI | Proposed | Imported 2026-09-23 from the pr-tour CLI extension pack; an AI-free `bpr tour` terminal frontend over the RFC-BPR-0039 to 0042 tour services, with no GUI or display requirement |
| [RFC-BPR-0045](RFC-BPR-0045-shared-local-tour-drafts-and-cross-interface-recovery.md) | Shared Local Tour Drafts and Cross-Interface Recovery | Proposed | Imported 2026-09-23; one same-machine WIP draft shared by desktop, CLI and skill through a single profile owner; published tours recover from GitHub alone |
| [RFC-BPR-0046](RFC-BPR-0046-thin-pr-tour-agent-skill.md) | Thin pr-tour Agent Skill | Proposed | Imported 2026-09-23; a thin `pr-tour` skill over `bpr tour` that uses the active agent and the RFC-BPR-0043 hooks, distributed through the shared skill workflow |
| [RFC-BPR-0047](RFC-BPR-0047-conventional-comment-semantics.md) | Conventional Comment Semantics and Lossless Recognition | Proposed | Imported 2026-09-23 from the Conventional Comments pack; all twelve labels as optional public intent, recognized losslessly from raw Markdown and kept separate from resolution, defects and review verdicts |
| [RFC-BPR-0048](RFC-BPR-0048-feedback-authoring-native-github-integration.md) | Feedback Authoring and Native GitHub Review Integration | Proposed | Imported 2026-09-23; optional label authoring through the shared draft, with native suggested changes, all three review decisions, private tracking and tour comments preserved |
| [RFC-BPR-0049](RFC-BPR-0049-label-counters-overview-settings.md) | Label Counters, Resolution-Aware Overview and Settings | Proposed | Imported 2026-09-23; configurable per-label Overview columns over a disjoint root-conversation partition with catchall, unresolved-first and honest coverage |
| [RFC-BPR-0050](RFC-BPR-0050-feedback-ux-label-icon-design.md) | Feedback UX and Label Icon Design and Delivery | Proposed | Imported 2026-09-23; a dedicated design workstream selects and delivers the compact controls and twelve label icons; no art is preselected |
| [RFC-BPR-0051](RFC-BPR-0051-convention-aware-hooks-cli-parity.md) | Convention-Aware Skill Hooks and Shared CLI Behavior | Proposed | Imported 2026-09-23; default hooks teach the convention only when optional AI is invoked, and headless operations share the desktop's semantics |
| [RFC-BPR-0053](RFC-BPR-0053-review-cycles-domain-and-local-authority.md) | Review Cycles: Domain, Local Authority and Durable History | Auto-Verified | Closed 2026-10-04 by [BPR-1186](../issues/60-done/BPR-1186-close-rfc-0053.md): all ten criteria pass on the recorded evidence of BPR-1143–1147, 1152 and the later cards the [ledger](../verification/RFC-BPR-0053-0061.md) names, on macOS and both stations; no owner verdict needed. Windows compiles out the crash courses; no product surface removes a single cycle, and purge after a publication waits on BPR-1283; RFC-BPR-0001, RFC-BPR-0003 and RFC-BPR-0007 |
| [RFC-BPR-0054](RFC-BPR-0054-workspace-source-capture-and-revision-material.md) | Workspace Source Capture and Immutable Revision Material | Auto-Verified | Closed 2026-10-04 by [BPR-1187](../issues/60-done/BPR-1187-close-rfc-0054.md): all ten criteria pass on the recorded evidence of BPR-1148–1152, 1162, 1183, 1184, 1244 and 1284 that the [ledger](../verification/RFC-BPR-0053-0061.md) names, on macOS and both stations, the junction escape observed on native Windows; no owner verdict needed. Windows prevents rather than detects root replacement; no product surface rebinds a moved project (BPR-1279); RFC-BPR-0053, RFC-BPR-0003 and RFC-BPR-0004 |
| [RFC-BPR-0055](RFC-BPR-0055-source-feedback-and-exact-suggested-edits.md) | Source Feedback, Provenance and Exact Suggested Edits | Proposed | Imported 2026-09-25 for later implementation; [source and mapping](assets/bpr-review-cycles-v1.LIVE_IMPORT.md); [eleven-milestone plan](plans/bpr-review-cycles-v1/milestone-plan.json). No product acceptance or earlier sequencing exception is implied |
| [RFC-BPR-0056](RFC-BPR-0056-frozen-feedback-rounds-and-portable-handoffs.md) | Frozen Feedback Rounds and Portable Handoffs | Proposed | Imported 2026-09-25 for later implementation; [source and mapping](assets/bpr-review-cycles-v1.LIVE_IMPORT.md); [eleven-milestone plan](plans/bpr-review-cycles-v1/milestone-plan.json). No product acceptance or earlier sequencing exception is implied |
| [RFC-BPR-0057](RFC-BPR-0057-local-iteration-responses-and-verification.md) | Local Iteration, Recipient Responses and Reviewer Verification | Proposed | Imported 2026-09-25 for later implementation; [source and mapping](assets/bpr-review-cycles-v1.LIVE_IMPORT.md); [eleven-milestone plan](plans/bpr-review-cycles-v1/milestone-plan.json). No product acceptance or earlier sequencing exception is implied |
| [RFC-BPR-0058](RFC-BPR-0058-headless-cycle-cli-and-portable-consumer-skill.md) | Headless Cycle CLI and Portable Consumer Skill | Proposed | Imported 2026-09-25 for later implementation; [source and mapping](assets/bpr-review-cycles-v1.LIVE_IMPORT.md); [eleven-milestone plan](plans/bpr-review-cycles-v1/milestone-plan.json). No product acceptance or earlier sequencing exception is implied |
| [RFC-BPR-0059](RFC-BPR-0059-clean-pr-publication-and-cycle-continuity.md) | Clean PR Publication and Local Cycle Continuity | Proposed | Imported 2026-09-25 for later implementation; [source and mapping](assets/bpr-review-cycles-v1.LIVE_IMPORT.md); [eleven-milestone plan](plans/bpr-review-cycles-v1/milestone-plan.json). No product acceptance or earlier sequencing exception is implied |
| [RFC-BPR-0060](RFC-BPR-0060-removable-agent-adapters-and-capability-isolation.md) | Removable Agent Adapters and Capability Isolation | Proposed | Imported 2026-09-25 for later implementation; [source and mapping](assets/bpr-review-cycles-v1.LIVE_IMPORT.md); [eleven-milestone plan](plans/bpr-review-cycles-v1/milestone-plan.json). No product acceptance or earlier sequencing exception is implied |
| [RFC-BPR-0061](RFC-BPR-0061-native-review-cycles-workbench-and-usability.md) | Native Review Cycles Workbench and Usability | Proposed | Imported 2026-09-25 for later implementation; [source and mapping](assets/bpr-review-cycles-v1.LIVE_IMPORT.md); [eleven-milestone plan](plans/bpr-review-cycles-v1/milestone-plan.json). No product acceptance or earlier sequencing exception is implied |

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
RFC-BPR-0020 defines their original Qt workbench composition, now closed through
the accepted RFC-BPR-0030/0031 successor implementation. The product owner's Nib bootstrap contributes process
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

RFC-BPR-0052 is the review workbench's scaffolding, from the owner's design
session on [BPR-1106](../issues/60-done/BPR-1106-a-design-round-for-where-the-composition-entries-sit-in-the-review-chrome.md)
(2026-09-24). It has a calm title row with the modality at its right, and a
review bar that holds the active tab's controls and Compose review and
Comment. Below the bar is an Updates line that folds, and RFC-BPR-0025's
document tabs fill the comparison's header. Asked where it should live, the
owner chose an RFC of its own. It stands after RFC-BPR-0038 and before the
review-tour pack, whose tour tab and Conventional Comments labels fill places
it draws. RFC-BPR-0028 closes on what it built. On 2026-09-25 the owner
made it active work ahead of that order ("The design session is complete,
add RFC-BPR-0052 to the active work which should include BPR-1129 to
BPR-1135"); the amendment is recorded in the RFC's status section and
names this RFC alone.

RFC-BPR-0039 through RFC-BPR-0043 are the imported review-tour pack: an
author-prepared guided review whose published form is ordinary GitHub comments,
usable by reviewers who never open BPR. The manual lifecycle (RFC-BPR-0039 to
0042) precedes the optional AI assistance of RFC-BPR-0043. Their [source pack and
live ID mapping](assets/bpr-review-tour-v1.LIVE_IMPORT.md) and [adopted
eight-milestone plan](plans/bpr-review-tour-v1/milestone-plan.json) retain the
source and execution order. The original import left all twenty-six cards in
Backlog and all five RFCs Proposed; BPR-1016 owns the single ordered closeout.
The 2026-09-26 implementation mandate and current intake baseline are recorded
in the [pack ledger](../verification/RFC-BPR-0039-0043.md).

RFC-BPR-0044 through RFC-BPR-0046 are the imported pr-tour CLI extension pack,
additive to the review-tour pack: a headless `bpr tour` terminal frontend, one
local tour draft shared across desktop, CLI and skill, and a thin `pr-tour`
agent skill that calls the CLI. They reuse the RFC-BPR-0039 to 0043 contracts
rather than reauthoring them. Their [source pack and live ID
mapping](assets/bpr-pr-tour-cli-extension-v1.LIVE_IMPORT.md) and [adopted
six-milestone plan](plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json)
retain the source and execution order. Import leaves all twenty cards in Backlog
and all three RFCs Proposed; BPR-1071 owns the single ordered closeout.

RFC-BPR-0047 through RFC-BPR-0051 are the imported Conventional Comments pack,
ordered after the review-tour pack and its pr-tour CLI extension: all twelve
Conventional Comments labels as optional, lossless review intent; authoring that
keeps native suggested changes and review decisions first-class; resolution-aware
per-label Overview counters; a dedicated compact UX and label-icon design
workstream; and convention-aware default hooks with shared CLI behavior. Their
[source pack and live ID
mapping](assets/betterpullreview-conventional-comments-rfc-pack-v1.LIVE_IMPORT.md)
and [adopted eight-milestone
plan](plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json)
retain the source and execution order. Import leaves all twenty-seven cards in
Backlog and all five RFCs Proposed; BPR-1097 to BPR-1101 close them one per RFC
in canonical order.

Owner decision, 2026-09-24 (attended session `cbd48b91000b00f1e72e1ad62c7c384c`,
candidate `RFC-0039-0051-start-v001`, decision `1d9104f8`, 19:17:30Z): asked
whether to start the imported packs now, RFC-BPR-0039 first and then in
canonical order through RFC-BPR-0051, the owner selected, verbatim, "Not yet:
finish RFC-0028 to RFC-0038 first", with an empty note. This held
RFC-BPR-0039 to RFC-BPR-0051 in Backlog pending RFC-BPR-0028 through
RFC-BPR-0038 closure.

Owner amendment, 2026-09-26: "$implement-rfc RFC-BPR-0039–0043 and land it
on main after each milestone". This authorizes the review-tour pack's feature
work now and supersedes the start hold for RFC-BPR-0039 through RFC-BPR-0043
only. Follow its eight authored milestones and integrate each accepted
milestone on local main before assigning the next. The start hold still
applies to RFC-BPR-0044 through RFC-BPR-0051. Formal dependency-ordered
closure, required human/native/server evidence and the existing remote-effect
boundary remain in force; this amendment supplies no acceptance verdict.

Owner closeout amendment r2, 2026-09-27: “Just close and land it on main anyways,
can file the GHE stuff as blocked follow-ups.” This accepts the verified
RFC-BPR-0039–0043 implementation and seven exact Mac candidates. The
[r2 mandate](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md) transfers
GitHub.com live qualification to BPR-1222, GHES to BPR-1223, remaining native
stations to BPR-1224, installed-provider/model evidence to BPR-1225 and formal
RFC-0021/0022/0036/0037 predecessor acceptance to BPR-1226. Those standalone cards
are Blocked; the original predecessor RFCs remain Proposed/Deferred. Other
explicit dependencies and all unavailable production capability flags remain.
The r1 start hold for RFC-BPR-0044–0051 remains outside this amendment.

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
and its closeout rows are complete. It stays `Proposed` because its explicit dependency
RFC-BPR-0028 is not `Auto-Verified`. RFC-BPR-0033 closed on
2026-09-23. Its recorded gate, RFC-BPR-0024, is `Auto-Verified`, and
[BPR-0782](../issues/60-done/BPR-0782-close-rfc-0033.md) records the recheck of
its closeout rows at the closing head `871c1185`. That recheck first found the
windowless macOS menu bar missing the Pull Request menu that BPR-0990 had added
to the window's bar, which [BPR-1035](../issues/60-done/BPR-1035-the-windowless-macos-menu-bar-drops-the-pull-request-menu.md)
fixed. [BPR-0817](../issues/60-done/BPR-0817-a-linux-close-exits-1-when-the-app-created-its-data-folder-group-writable.md),
which is associated with it, is Done. On 2026-09-23 the owner delegated its
two questions to its implementer, and the decisions are recorded as an
owner-delegated amendment on the card and in RFC-BPR-0033. A refused agent
workspace recovery now leaves an orderly Linux quit at exit 0, and the refusal
is stated at Home in the window's status bar. Both were observed on WSLg. B5's
attended clause is answered. The defect behind the
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
[BPR-0782](../issues/60-done/BPR-0782-close-rfc-0033.md)'s B1 row and closing
section now record them.

RFC-BPR-0022 and RFC-BPR-0023 were reconciled and imported together on
2026-09-04 with implementation explicitly deferred. The off-hunk pack retains
0022; the private-thread pack was renumbered to 0023. RFC-BPR-0022 owns semantic
line publication and native/file-permalink compatibility through RFC-BPR-0008.
RFC-BPR-0023 owns private turns, honest provenance, composition, and local
lineage; Squash to Draft stays local and Publish stays separate. Their shared
publication cards consume one logical receipt and distinguish approved prose
from the permalink-prefixed public body. RFC-BPR-0021 keeps the general
historical comment/revision join. The imported pack prompts and earlier
frontier language grant no implementation authority. On 2026-09-26 the owner
explicitly activated RFC-BPR-0022 implementation and main integration, then
RFC-BPR-0023 implementation, plan revision and milestone integration under mandate
r1. The RFC-0023 [live audit](../verification/RFC-BPR-0023-live-audit.md) preserves
the earlier import boundary as history. RFC-0023 amendment r2 moves the M2
authority-boundary review to one capability-critical independent review of the
final integrated candidate at M10/BPR-0591 and directs implementation to continue.
Explicit dependencies and all eighteen feature criteria remain in force; final
independent review was supplied at M10, its findings resolved, and BPR-0591
closed the RFC on 2026-09-27 with designated publication evidence.

RFC-BPR-0027 records the owner's request for useful local diagnostic logging,
Standard/Verbose detail and privacy/export controls inspired by AllTheThings.
Quill v13.0.0 supplies the foundation in [[BPR-0602]], after [[BPR-0600]]
planning. Mandate r2 activated the full advanced implementation, now qualified
through BPR-1215–1219 with all three owner judgments accepted at cursor34.
[[BPR-0603]] records aggregate acceptance; [[BPR-0601]] closes the RFC after
the owner removed the global ordering rule in amendment r3. Exact native, package
and crash limits remain in the
[verification ledger](../verification/RFC-BPR-0027.md). No live-run authority
or release approval is granted by this acceptance.

RFC-BPR-0029 is the owner's own request. It grew from the RFC-BPR-0028 epoch
line he rejected in v008: an epoch is an update to the pull request, and its
identity is the same on every machine. Asked whether that should amend the
closed RFC-BPR-0003 or stand alone, he answered "It's own rfc" on 2026-09-12. Its
design is [[BPR-0743]]'s design pass. The owner answered its asks and
accepted the revised proposal on 2026-09-12 under mandate r17; [[BPR-0743]]
built it, and [[BPR-0750]] closed it `Auto-Verified` on 2026-09-23. It
supersedes RFC-BPR-0003 in part, in the two respects RFC-BPR-0003's own note
names, and RFC-BPR-0003 is not reopened.

RFC-BPR-0022 mandate r3 permits its closeout ahead of Deferred RFC-BPR-0020 and
RFC-BPR-0021 once its remaining acceptance passes. The
[exact owner decision](../issues/_checkpoints/BPR-0575/m4-r3-amendments.md)
changes only that ordering. Actual fallback, GET-only recovery and the accepted
GitHub-only reply course are observed. Mandate r4 accepts app-closed continuity
and fresh-host reconstruction for AC-10, with actual desktop uninstall still
unobserved, and explicitly substitutes owner acceptance for independent review
of source `0f3188093`. The [current ledger](../verification/RFC-BPR-0022.md)
records Auto-Verified closeout and the retained native-success/platform limits.
