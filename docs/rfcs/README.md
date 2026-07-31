# BetterPullReview RFC Index

The RFCs normally begin and always close in the order below. A dependent RFC
does not become `Auto-Verified` before every listed dependency is
`Auto-Verified`.

Product-owner exception: real Windows x64 evidence retained by BPR-0036 is
temporarily unavailable. RFC-BPR-0002 therefore remains the closure frontier,
but that hardware-only gate does not block implementation, tests,
documentation, security review, simplification, or later-RFC work on
available targets. RFC-BPR-0003 is the implementation frontier. The exception
does not waive any Windows claim, change an RFC status, permit out-of-order
closeout, or make GitHub Actions an evidence surface.

| RFC | Title | Status | Depends on |
|---|---|---|---|
| [RFC-BPR-0001](RFC-BPR-0001-product-contract-native-architecture.md) | Product Contract, Native Architecture, and Repository Foundation | Auto-Verified | — |
| [RFC-BPR-0002](RFC-BPR-0002-github-host-auth-capability-layer.md) | GitHub Host, Authentication, and Capability Layer | Proposed | RFC-BPR-0001 |
| [RFC-BPR-0003](RFC-BPR-0003-git-cache-pr-identity-review-epochs.md) | Local Git Cache, Pull Request Identity, and Review Epochs | Proposed | RFC-BPR-0001, RFC-BPR-0002 |
| [RFC-BPR-0004](RFC-BPR-0004-diff-computation-line-mapping-grouping.md) | Diff Computation, Line Mapping, and Change Grouping | Proposed | RFC-BPR-0003 |
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
RFC is outside that canonical pack, cannot block its implementation or
closeout, and remains deferred until its upstream eligibility gates exist:

| RFC | Title | Status | Eligibility |
|---|---|---|---|
| [RFC-BPR-0015](future/RFC-BPR-0015-github-cli-production-enablement.md) | GitHub CLI Production Enablement | Deferred | RFC-BPR-0014 closed and official `gh` supplies the required safety contract |
