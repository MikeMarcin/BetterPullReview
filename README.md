# BetterPullReview

A native desktop app for reviewing GitHub pull requests and local code.
Read diffs, keep private notes, and follow changes across review rounds.

**[Download](#install) · [Quick start](#quick-start) · [Guides](#guides) · [Contribute](#development)**

- Compare a pull request with earlier revisions that BetterPullReview has retained.
- Draft comments and reviews beside the code, then publish when you're ready.
- Write a **Review Tour** that walks reviewers through the problem, solution and
  selected source lines.
- Use **Review Cycles** to review a local folder, file or checkout without a
  GitHub account or an AI agent.

> [!NOTE]
> GitHub owns shared comments, reviews and approvals. Your notes, drafts and
> review progress stay local; publishing is a separate action.

## Install

The [latest release](https://github.com/MikeMarcin/BetterPullReview/releases/latest)
is **v0.3.0**. If GitHub prompts you to sign in, use an account with repository
access.

| Platform | Installer |
|---|---|
| macOS 13+ · Apple Silicon and Intel | [Download DMG](https://github.com/MikeMarcin/BetterPullReview/releases/download/v0.3.0/BetterPullReview-0.3.0-macos-universal.dmg) |
| Windows 10/11 · x64 | [Download MSI](https://github.com/MikeMarcin/BetterPullReview/releases/download/v0.3.0/BetterPullReview-0.3.0-windows-x64.msi) |
| Ubuntu 26.04 · x64 | [Download DEB](https://github.com/MikeMarcin/BetterPullReview/releases/download/v0.3.0/BetterPullReview-0.3.0-linux-ubuntu-26.04-x64.deb) |

On macOS, open the DMG and copy the app to `~/Applications`. On Windows, run
the per-user MSI installer. On Ubuntu, follow the release's
[signature verification steps](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.3.0#linux-signature-verification)
before installing the DEB with your package manager.

Install Git 2.39+ for pull-request history and comparisons. Optional GitHub CLI
access is macOS-only and requires `gh` 2.96+ and Git 2.49+.
The [package guide](docs/development/release-packages.md) covers archive installs,
system dependencies and signature checks. Windows and Linux use manual updates;
eligible signed macOS installations can use **Check for Updates…**.

> [!WARNING]
> v0.3.0 is a stable release, but native installation, authentication, vault,
> accessibility, removal, crash-privacy and vulnerability qualification remain
> incomplete. Ubuntu also exceeds the large-review memory-growth test limit.
> The [release notes](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.3.0#known-limitations)
> record the evidence, gaps and accepted release exception.

## Quick start

### Review a pull request

1. Open BetterPullReview and choose **Sign in** on Home. Follow the available
   sign-in flow for your account; manage connections in **Settings > Accounts & hosts**.
2. Paste a pull-request URL into Home's **Open** field and press Return.
3. Select a changed file, read the comparison and write your feedback. Use
   **Publish** for an inline comment or **Submit review** for a whole review.

Add repositories in **Settings > Repositories** to find your authored and
requested pull requests on Home. For GitHub Enterprise Server, add your server
explicitly in **Accounts & hosts**; available sign-in methods depend on the host.

> [!TIP]
> On macOS, Windows or Linux, paste a public GitHub.com pull-request link on
> Home to read it without signing in. This mode is read-only and has no
> account-specific review state or thread resolution. It is unavailable on GHES.

### Review local code

1. Choose **File > Open Project…** and select a folder, file or Git checkout.
2. Add notes or suggested edits, then compose a review round to copy, save or export.
3. After the code changes, use **Capture revision** to compare it with the
   previous revision and record your verification.

Review Cycles does not write into the folder you review. See the
[workflow guide](docs/ux/review-cycles-workflow.md) for sharing rounds, using an
agent and publishing a finished result as a new pull request.

## Guides

| Guide | Covers |
|---|---|
| [Settings](docs/development/settings.md) | Accounts, repositories, appearance and optional agents |
| [Review Tours](docs/development/review-tours.md) | Writing, previewing and publishing a guided review |
| [Private review](docs/ux/private-review-workflow.md) | Local conversations, drafts and their publication boundaries |
| [Review Cycles CLI](docs/development/cycle-cli.md) | Working with local review rounds through `bpr cycle` |
| [Storage and removal](docs/development/storage-and-diagnostics.md) | Local data, diagnostics, profile purge and uninstall |
| [Application updates](docs/development/application-updates.md) | Updates and recovery on eligible macOS installations |
| [Security overview](docs/security/corporate-security-overview.md) | Network access, credentials, subprocesses and AI boundaries |
| [Security policy](SECURITY.md) | Private vulnerability reporting and supported versions |

Uninstalling the app leaves private data behind. Profile purge is a separate
operation and excludes shared updater and registry state.

## Development

Read [Contributing](CONTRIBUTING.md) and the
[build prerequisites](docs/development/build-and-test.md#prerequisites), then run:

```sh
cmake --workflow --preset dev
```

The [build guide](docs/development/build-and-test.md) covers the Qt-free core,
focused tests and coding checks. Architecture and implementation evidence live
in the [documentation index](docs/README.md), [RFCs](docs/rfcs/README.md) and
[issue board](docs/issues/README.md). Builds, tests and releases run locally or
on the target platform; GitHub Actions is not used.

## License and third-party notices

Original BetterPullReview code and documentation are licensed under the
[MIT License](LICENSE). Third-party and adapted material retains its existing
copyright and licence notices; this grant does not replace those terms.

Packages include `THIRD-PARTY-NOTICES.txt` and `SOURCE-OFFERS.txt` in their
`ThirdParty` directory (`Contents/Resources/ThirdParty` on macOS). Each release
also supplies an SPDX inventory, checksums and a separate corresponding-source
archive for each platform. These components retain their own licences.

Binary recipients must have equivalent access to the exact corresponding source
at no additional charge. Corporate and offline redistribution must include that
source archive; see the [licence and source obligations](docs/development/release-packages.md#exact-dependency-and-license-evidence).
