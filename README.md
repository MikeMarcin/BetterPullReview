# BetterPullReview

A native desktop app for reviewing GitHub pull requests and local code.
Read diffs, keep local comments, and follow changes across review rounds.

**[Download](#install) · [Quick start](#quick-start) · [Guides](#guides) · [Contribute](#development)**

- Compare a pull request with earlier revisions that BetterPullReview has retained.
- Write comments beside the code. A comment stays Local (only on this computer)
  or becomes a Draft that you publish when you're ready.
- Prepare a pull request from a Git worktree with **Prepare Pull Request…** on
  Home, then create it on GitHub as a draft pull request once its branch is pushed.
- Use **Ask** to put a question about the pull request, a file or a thread to
  an optional agent; long answers open in a file tab.
- Write a **Review Tour** that walks reviewers through the problem, solution and
  selected source lines.

> [!NOTE]
> GitHub owns shared comments, reviews and approvals. Your Local comments,
> drafts and review progress stay on your computer; publishing is a separate action.

## Install

The [latest release](https://github.com/MikeMarcin/BetterPullReview/releases/latest)
is **0.4.2**. Downloads and corresponding source are publicly available.

| Platform | Installer |
|---|---|
| macOS 14.4+ · Apple Silicon and Intel | [Download DMG](https://github.com/MikeMarcin/BetterPullReview/releases/download/0.4.2/BetterPullReview-0.4.2-build.60-macos-universal.dmg) |
| Windows · x64 | [Download MSI](https://github.com/MikeMarcin/BetterPullReview/releases/download/0.4.2/BetterPullReview-0.4.2-build.60-windows-x64.msi) |
| Ubuntu 26.04 · x64 | [Download DEB](https://github.com/MikeMarcin/BetterPullReview/releases/download/0.4.2/BetterPullReview-0.4.2-build.60-linux-ubuntu-26.04-x64.deb) |

Tested on Windows 11 and Ubuntu 26.04 (WSL2). Other Windows and Linux versions
may work but are untested.

On macOS, open the DMG and copy the app to `~/Applications`. On Windows, run
the per-machine MSI installer. On Ubuntu, follow the release's
[signature verification steps](https://github.com/MikeMarcin/BetterPullReview/releases/tag/0.4.2#linux-signature-verification)
before installing the DEB with your package manager.

Install Git 2.39+ for pull-request history and comparisons. Optional GitHub CLI
access requires `gh` 2.96+ and Git 2.49+. On Windows and Linux, `gh`'s file
writes are not confined as they are on macOS.
The [package guide](docs/development/release-packages.md) covers archive installs,
system dependencies and signature checks. Windows and Linux use manual updates;
eligible signed macOS installations can use **Check for Updates…**.

> [!WARNING]
> 0.4.2 is a stable release, but native installation, authentication, vault,
> accessibility, removal, crash-privacy and vulnerability qualification remain
> incomplete.
> The [release notes](https://github.com/MikeMarcin/BetterPullReview/releases/tag/0.4.2#verification-and-limitations)
> record the evidence, gaps and accepted release exceptions.

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

### Prepare a pull request locally

1. Choose **Prepare Pull Request…** on Home and select a Git worktree.
2. Read the proposed diff and add notes; no GitHub account is needed yet.
3. Once its branch is pushed, create it on GitHub as a draft pull request.

A harness can hand a worktree over with `bpr draft open`; see the
[local PR drafts CLI](docs/development/draft-cli.md). Review Cycles and
`bpr cycle` were removed in 0.5.0 in favour of local PR drafts.

## Guides

| Guide | Covers |
|---|---|
| [Settings](docs/development/settings.md) | Accounts, repositories, appearance and optional agents |
| [Review Tours](docs/development/review-tours.md) | Writing, previewing and publishing a guided review |
| [Private review](docs/ux/private-review-workflow.md) | Local conversations, drafts and their publication boundaries |
| [Local PR drafts CLI](docs/development/draft-cli.md) | Handing a worktree to a local PR draft with `bpr draft open` |
| [Storage and removal](docs/development/storage-and-diagnostics.md) | Local data, diagnostics, profile purge and uninstall |
| [Application updates](docs/development/application-updates.md) | Updates and recovery on eligible macOS installations |
| [Security overview](docs/security/corporate-security-overview.md) | Network access, credentials, subprocesses and AI boundaries |
| [Security policy](SECURITY.md) | Private vulnerability reporting and supported versions |

Uninstalling the app leaves private data behind. Profile purge is a separate
operation and excludes shared updater and registry state.

## Development

<img src="docs/images/dif.png" alt="Dif, the BetterPullReview mascot" width="64" align="right">

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

Bug reports with reproducible cases, documentation fixes and pull requests are
welcome. Development is supported through
[GitHub Sponsors](https://github.com/sponsors/MikeMarcin).

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
