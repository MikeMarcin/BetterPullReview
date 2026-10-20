# Security policy

## Report a vulnerability

Report privately through GitHub's
[Report a vulnerability](https://github.com/MikeMarcin/BetterPullReview/security/advisories/new)
form, also linked from the repository's
[Security page](https://github.com/MikeMarcin/BetterPullReview/security). You
need to be signed in to GitHub.

> [!WARNING]
> Never put exploit details, private repository content, credentials or
> diagnostic databases in a public issue or pull request.

If the form is unavailable:

1. Use an existing private contact with the repository owner, if you have one.
2. Otherwise, [open an issue](https://github.com/MikeMarcin/BetterPullReview/issues/new)
   that asks only for a private security contact, with no vulnerability
   details.
3. Wait for that private channel before sharing a reproduction.

Maintainers keep private vulnerability reporting enabled while the repository
is public. Fix verification and disclosure are coordinated through the private
channel.

## What to include

- the affected version or commit, and for packaged builds the exact installer
  or archive name
- operating system and architecture
- expected and observed behavior
- the security impact
- a minimal reproduction using disposable repositories and synthetic data

Never send live credentials. If a credential was exposed, revoke it with its
issuer first, then describe the credential type and where it was exposed
without including its value.

## Supported versions

| Version | Security fixes |
|---|---|
| Current `main` | Yes |
| Latest stable release | Yes |
| Older releases and branches | No backports; a fix may require upgrading |

There is no guaranteed response or remediation deadline.

## Scope

BetterPullReview is a local desktop client with explicit GitHub and
external-agent integrations. The
[security overview](docs/security/corporate-security-overview.md) describes its
credentials, local data, processes and network access.

Release qualification is not complete: native, vault, crash and privacy,
accessibility and vulnerability work remains open, as recorded in the
[release qualification ledger](docs/verification/RFC-BPR-0038.md). A stable
version label or a passing secret scan does not close those gaps.

Test only systems and data you are authorized to use, and do not include other
people's data in a report.
