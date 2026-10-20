# Security policy

## Reporting a vulnerability

Check the repository's [Security page](https://github.com/MikeMarcin/BetterPullReview/security)
for **Report a vulnerability**, and use that private reporting form when it is
available. Do not put exploit details, private repository content, credentials
or diagnostic databases in a public issue or pull request.

GitHub private vulnerability reporting is unavailable while this repository is
private. Existing recipients should use their established private contact with
the repository owner. If you have no private channel and the reporting form is
unavailable, [open an issue](https://github.com/MikeMarcin/BetterPullReview/issues/new)
containing only a request for a private security contact, with no vulnerability
details. Wait for that channel before sharing a reproduction. Maintainers must
enable and verify the private reporting form when making the repository public;
this policy does not claim the form is already enabled.

Include the affected version or commit, operating system and architecture,
expected and observed behavior, security impact, and a minimal reproduction
using disposable repositories and synthetic data. Never send live credentials.
If a credential was exposed, revoke it through its issuer and describe the
credential type and affected location without including its value. Coordinate
fix verification and disclosure with the maintainer through the private channel.

## Supported scope

Security fixes target current `main` and the latest stable release. Include the
exact installer or archive name for packaged builds. Older releases and branches
are not separately maintained for security backports; a fix may require an
upgrade. There is no guaranteed response or remediation deadline.

BPR is a local desktop client, with explicit GitHub and external-agent
integrations. Its [security overview](docs/security/corporate-security-overview.md)
describes credentials, local data, process and network boundaries. The
[release qualification ledger](docs/verification/RFC-BPR-0038.md) records the
current platform evidence and open native, vault, crash/privacy, accessibility
and vulnerability work. A stable version label or passing secret scan does not
close those qualification gaps. Test only systems and data you are authorized
to use; do not include other users' data in a report.
