# BetterPullReview Documentation

Start with the [project README](../README.md) for installation and a quick
start. This index lists the rest of the documentation, grouped by who it is for.

## Using the app

| Guide | Covers |
|---|---|
| [Settings](development/settings.md) | Accounts, repositories, appearance and optional agents |
| [Private review](ux/private-review-workflow.md) | Local conversations, drafts and what publishing sends to GitHub |
| [Review Cycles workflow](ux/review-cycles-workflow.md) | Reviewing a local folder, file or checkout across rounds |
| [Review Cycles CLI](development/cycle-cli.md) | Working with local review rounds through `bpr cycle` |
| [Review Tours](development/review-tours.md) | Writing, previewing and publishing a guided review |
| [Source-line comments](development/source-line-comments.md) | Commenting on changed and unchanged lines of a pull request's text files |
| [Storage and removal](development/storage-and-diagnostics.md) | Local data, diagnostics, profile purge and uninstall |
| [Application updates](development/application-updates.md) | Updates and recovery on eligible macOS installations |
| [Release notes](releases/) | What changed in each version |

## Security and IT

| Document | Covers |
|---|---|
| [Security overview](security/corporate-security-overview.md) | Network access, credentials, subprocesses and AI boundaries |
| [Security policy](../SECURITY.md) | Private vulnerability reporting and supported versions |
| [Install and verify packages](development/release-packages.md) | Archive installs, system dependencies, signatures and licence obligations |
| [Release security qualification](security/release-policy.md) | What a release must pass before it ships |
| [Release updater security](security/release-updater.md) | The macOS updater's security boundary |
| [Security and threat model](architecture/security-threat-model.md) | Threats considered and how the design answers them |
| [AI agent boundary](architecture/ai-agent-boundary.md) | What an optional agent can see and do |

## Contributing

| Document | Covers |
|---|---|
| [Contributing](../CONTRIBUTING.md) | What to read, how work is tracked, architecture rules and verification |
| [Build and test](development/build-and-test.md) | Prerequisites, first build, presets, tests and coding checks |
| [Build and test reference](development/build-and-test-reference.md) | Dependency boundaries, test-harness rules, courses, fuzzing and per-RFC checks |
| [Coding standard](../_coding-standard/STANDARD.md) | First-party C++ style and rules |
| [Local secret scanning](development/betterleaks.md) | Optional BetterLeaks pre-commit scanning |
| [Remote stations](development/remote-stations.md) | Setting up other machines for platform-specific work |
| [Tour terminal development](development/tour-cli.md) | Building and testing the `bpr` executable |
| [Fixture catalog](verification/fixture-catalog.md) | Recorded and constructed test fixtures |
| [Ported code provenance](development/ported-code-provenance.md) | Origin and licence of adapted code |

Engineering references behind the user guides:

- [Settings authority inventory](development/settings-authority-inventory.md)
- [Profile and window authority inventory](development/profile-window-authority-inventory.md)
- [Application updates: release operator gate](development/application-updates-release-gate.md)

## Architecture and records

**Product and architecture**

- [Product contract](architecture/product-contract.md)
- [System architecture](architecture/system-architecture.md)
- [Deterministic core, imperative host and Qt presentation](architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md)
- [Domain and state model](architecture/domain-and-state-model.md)
- [Foundation implementation](architecture/foundation-implementation.md)
- [GitHub host implementation](architecture/github-host-implementation.md) and [GitHub provider contract](architecture/github-provider-contract.md)
- [Git cache and review epochs](architecture/git-and-epoch-model.md)
- [Diff pipeline and rendering](architecture/diff-pipeline-and-rendering.md)
- [Review ledger](architecture/review-ledger.md), [review queue](architecture/review-queue.md) and [review round trip](architecture/review-round-trip.md)
- [Release readiness](architecture/release-readiness.md), [release maintenance](architecture/release-maintenance.md) and [release updater](architecture/release-updater.md)

**UX specifications**

- [Workbench specification](ux/workbench-specification.md)
- [Appearance and accessibility](ux/appearance-and-accessibility.md)
- [Review state semantics](ux/review-state-semantics.md)
- [AI experience](ux/ai-experience.md)

**Planning and evidence**

- [RFC index](rfcs/README.md): designs and their status
- [Issue board](issues/README.md): work items; a card's folder is its status
- [Master verification matrix](verification/master-verification-matrix.md)
- [Release gates](verification/release-gates.md)
- [Foundation security baseline](security/foundation-security.md)
- [Reference baseline](reference-baseline.md)

RFC status is tracked only in the RFC front matter and the RFC index. Issue
folder placement is the source of truth for workflow state.
