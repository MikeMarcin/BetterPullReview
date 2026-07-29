# GitHub Provider Contract

## Architectural allocation

- The functional core receives normalized host/account/capability observations, decides semantic command legality and request identity, and accepts or rejects completion events.
- The application host owns authentication, REST/GraphQL/CLI transports, retries, rate limits, capability caches, redirect/trust policy, and mutation execution.
- The Qt presentation adapter owns host/account controls, authentication/error presentation, and command projections; it never receives credentials or calls the provider directly.

## Host support

- GitHub.com
- GitHub Enterprise Server 3.18 and later, subject to capability probing

Host profiles are explicit and never inferred from untrusted content. Each profile stores hostname, product kind, API roots, authentication mode, proxy/trust behavior, and capabilities.

## API strategy

REST is the baseline for pull-request metadata, commits, files, reviews, comments, checks, search, and notifications. GraphQL is used narrowly for review-thread grouping, file viewed state, thread resolution, and other features whose REST representation is insufficient.

GHES requests pin the supported REST API version selected by the host adapter. The initial floor is `2022-11-28` for GHES 3.18. Capability probing and contract fixtures remain authoritative over naming assumptions.

## Authentication transports

### GitHub CLI broker

- Invoke an explicitly resolved `gh` executable.
- Never request or print the token.
- Pass request bodies through standard input.
- Disable interactive prompts for application jobs.
- Preserve the agent or GitHub CLI's own credential storage boundary.

### Native HTTPS

- Use `QNetworkAccessManager` and operating-system trust/proxy configuration.
- Store credentials only in Windows Credential Manager, macOS Keychain, or Linux Secret Service.
- Never store tokens in SQLite, settings, environment files, logs, crash reports, or command lines.
- Never offer an ignore-certificate-error option.

## Normalized provider surface

```text
GetViewerIdentity
GetRepository
GetPullRequest
ListPullRequestFiles
ListPullRequestCommits
ListReviews
ListReviewThreads
ListIssueComments
ListChecks
SearchPullRequests
MarkFileViewed / Unviewed
CreateReview / SubmitReview
CreateThread / ReplyToThread
ResolveThread / UnresolveThread
```

Unsupported capabilities produce explicit unavailable states and safe fallbacks; they do not silently approximate a mutation.

## Mutation safety

Every mutation requires an immutable target descriptor containing host, repository identity, PR number, expected head OID, authenticated user identity, and idempotency key where supported. Tests default to recorded contracts and a fake mutation sink. Live mutations require a user-designated safe PR.
