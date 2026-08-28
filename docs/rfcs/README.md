# BetterPullReview RFC Index

The RFCs normally begin and always close in the order below. A dependent RFC
does not become `Auto-Verified` before every listed dependency is
`Auto-Verified`.

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
| [RFC-BPR-0004](RFC-BPR-0004-diff-computation-line-mapping-grouping.md) | Diff Computation, Correspondence Mapping, Grouping, and Alignment-Space Scrolling | Proposed | RFC-BPR-0003 |
| [RFC-BPR-0005](RFC-BPR-0005-native-workbench-virtualized-review-surface.md) | Native Workbench Shell and Virtualized Review Surface | Proposed | RFC-BPR-0004 |
| [RFC-BPR-0006](RFC-BPR-0006-syntax-highlighting-themes-appearance.md) | Syntax Highlighting, Themes, Fonts, and Appearance Profiles | Proposed | RFC-BPR-0005 |
| [RFC-BPR-0007](RFC-BPR-0007-local-review-ledger-actionability.md) | Local Review Ledger, File and Conversation Receipts, and Actionability | Proposed | RFC-BPR-0003, RFC-BPR-0005 |
| [RFC-BPR-0008](RFC-BPR-0008-github-review-round-trip-interactions.md) | GitHub Review Synchronization and Round-Trip Interactions | Proposed | RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007 |
| [RFC-BPR-0009](RFC-BPR-0009-defect-fix-verification-workflow.md) | Defect Classification, Fix Claims, and Verification | Proposed | RFC-BPR-0008 |
| [RFC-BPR-0010](RFC-BPR-0010-review-queue-observers-notifications.md) | Review Queue, Observer Rules, and Local Notifications | Proposed | RFC-BPR-0007, RFC-BPR-0008 |
| [RFC-BPR-0011](RFC-BPR-0011-ai-agent-profiles-context-supplements.md) | AI Agent Profiles, Launch, Authentication Isolation, and Context Supplements | Proposed | RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007 |
| [RFC-BPR-0012](RFC-BPR-0012-ai-review-chat-findings-mcp.md) | AI Review Findings, Comment Workshop, Side Chat, and MCP Context | Proposed | RFC-BPR-0008, RFC-BPR-0011 |
| [RFC-BPR-0013](RFC-BPR-0013-performance-resilience-resource-limits.md) | Performance, Resource Limits, Resilience, and Recovery | Proposed | RFC-BPR-0004, RFC-BPR-0005, RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0010, RFC-BPR-0012 |
| [RFC-BPR-0014](RFC-BPR-0014-cross-platform-packaging-security-release.md) | Cross-Platform Packaging, Signing, Security Hardening, and Release Readiness | Proposed | RFC-BPR-0013 |

The imported files in [plans](plans/README.md) are immutable planning source.
Live work is tracked on the [issue board](../issues/README.md).

## Deferred post-pack work

The table above is the complete fourteen-RFC pack-v2 sequence. The following
RFCs are outside that canonical pack, cannot block its implementation or
closeout, and remain deferred until their eligibility gates exist:

| RFC | Title | Status | Eligibility |
|---|---|---|---|
| [RFC-BPR-0015](future/RFC-BPR-0015-github-cli-production-enablement.md) | GitHub CLI Production Enablement | Deferred | RFC-BPR-0014 closed and official `gh` supplies the required safety contract |
| [RFC-BPR-0016](future/RFC-BPR-0016-github-device-authorization-flow.md) | GitHub Device Authorization Flow | Deferred | The recorded narrow exception permits implementation/hermetic verification after its application identity and real-contract gates; RFC-BPR-0002 closeout remains mandatory before this RFC can close |
| [RFC-BPR-0017](future/RFC-BPR-0017-unauthenticated-public-review-read.md) | Unauthenticated Public Review Read | Deferred | RFC-BPR-0002 closed, recorded anonymous-access probe evidence, and a product decision that the mode is wanted |
| [RFC-BPR-0018](future/RFC-BPR-0018-robust-settings-center.md) | Robust Settings Center and Preference Ownership | Deferred | RFC-BPR-0014 closed or an explicit post-pack frontier exception, plus a complete existing-setting authority and persistence inventory |
| [RFC-BPR-0019](future/RFC-BPR-0019-dif-visual-identity-sprite-assets.md) | Dif Visual Identity, Sprite Assets, and Animation Contract | Deferred | RFC-BPR-0006 and RFC-BPR-0013 closed, RFC-BPR-0014 closed or an explicit post-pack frontier exception, and confirmed asset ownership/provenance |
| [RFC-BPR-0020](future/RFC-BPR-0020-animated-dif-mascot-qt-composition.md) | Animated Dif Mascot Qt Presentation and Workbench Composition | Deferred | RFC-BPR-0019 closed, its live appearance/workbench/resource paths inventoried, and RFC-BPR-0014 closed or an explicit post-pack frontier exception |
| [RFC-BPR-0021](future/RFC-BPR-0021-conversation-revision-identity.md) | Conversation Revision Identity and the Comment-Code History Join | Deferred | RFC-BPR-0002 and RFC-BPR-0003 closed, and recorded probe evidence that a real service returns a per-comment commit — including unauthenticated and on GHES 3.18 |

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
