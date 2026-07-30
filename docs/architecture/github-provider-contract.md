# GitHub Provider Contract

The concrete RFC-BPR-0002 flow and lifecycle are defined in [GitHub Host
Implementation Architecture](github-host-implementation.md).

## Architectural allocation

- The functional core receives normalized host/account/capability observations, decides semantic command legality and request identity, and accepts or rejects completion events.
- The application host owns the narrow provider capability port, selected
  session policy, staged-snapshot acceptance protocol, cancellation, cache
  invalidation, and serialized completion delivery.
- The GitHub adapter implements profile validation, normalization,
  REST/GraphQL contracts, CLI and native HTTPS transports, credentials,
  proxy/trust enforcement, bounded retry/rate-limit handling, and future
  mutation execution.
- The Qt presentation adapter owns host/account controls, authentication/error presentation, and command projections; it never receives credentials or calls the provider directly.

For read completions, the host stages a bounded immutable normalized snapshot,
the core accepts or rejects its complete identity context, and only an exact
`accepted_snapshot_delta` permits cache commit and Qt publication.

## Host support

- GitHub.com
- GitHub Enterprise Server 3.18 and later, subject to capability probing

Host profiles are explicit and never inferred from untrusted content. Each profile stores hostname, product kind, API roots, authentication mode, proxy/trust behavior, REST version, and a profile revision. Capabilities are separately probed observations keyed by profile revision, host, account, server/contract facts, and probe-contract revision; capability revision is generated for the accepted cache value.

The selected v1 profile validator admits canonical lowercase ASCII FQDNs
only. GitHub.com is exactly `github.com` with REST
`https://api.github.com` and GraphQL
`https://api.github.com/graphql`; GHES 3.18 is exactly
`https://HOST/api/v3` and `https://HOST/api/graphql`. Both use REST version
`2022-11-28`. Profiles derived from provider content or redirects, hostname
aliases, IP literals, ports, non-HTTPS roots, and root drift fail before a
request can be compiled. GraphQL redirects are forbidden, and REST redirects
remain on the exact validated authority and API namespace.

The functional core sees an opaque profile identity rather than hostname, URL,
proxy, trust, or authentication configuration. The active host and account are
always visible to the user, while credential material is never projected.

## API strategy

REST is the baseline for pull-request metadata, commits, files, reviews, comments, checks, search, and notifications. GraphQL is used narrowly for review-thread grouping, file viewed state, thread resolution, and other features whose REST representation is insufficient.

GHES requests pin the supported REST API version selected by the host adapter. The initial floor is `2022-11-28` for GHES 3.18. Capability probing and contract fixtures remain authoritative over naming assumptions.

Each capability is `Unknown`, `Supported`, or `Unsupported`. Lack of probe
evidence produces `Unknown`; it does not silently enable a feature or claim
that the server does not support it.

One bounded, static GraphQL introspection query observes the exact
review-thread, viewed-state, mark/unmark, and resolve/unresolve schema
contracts. It never executes a mutation. Only a complete authenticated
schema observation may produce `Supported` or `Unsupported`; every
operational failure, GraphQL error, partial response, malformed shape, or
limit breach produces explicit all-`Unknown` unavailability. Schema support
is distinct from target authorization: normalized
`viewerCanResolve`/`viewerCanUnresolve` flags and viewed-file state describe
the selected thread/file, while actual mutation permission remains
fail-closed until RFC-BPR-0008.

## Authentication transports

### GitHub CLI broker

- Invoke an explicitly resolved `gh` executable.
- Never request or print the token.
- Pass request bodies through standard input.
- Disable interactive prompts for application jobs.
- Preserve the agent or GitHub CLI's own credential storage boundary.
- Use an argument allowlist, a neutral non-repository working directory, and
  no shell.
- Construct an explicit minimal environment allowlist rather than inheriting
  the parent environment. Token, repository/host override, debug, pager,
  editor/browser, color, TTY, update, telemetry, spinner, and prompt
  influences are not admitted and their values are never read or logged.
- Inject an unknown canary environment variable during contract tests and
  prove it is not inherited by the child.
- Bound input, output, error, duration, and pagination; fail atomically when a
  limit is reached.
- Accept a result only when the broker's effective authority and
  redirect/certificate behavior have been proven to satisfy the selected
  profile. If the selected `gh` contract cannot prove that requirement, the
  broker path remains unavailable and RFC closeout is blocked.

### Native HTTPS

- Use `QNetworkAccessManager` and operating-system trust/proxy configuration.
- Store credentials only in Windows Credential Manager, macOS Keychain, or Linux Secret Service.
- Never store tokens in SQLite, settings, environment files, logs, crash reports, or command lines.
- Never offer an ignore-certificate-error option.
- Handle redirects manually under an HTTPS and exact-authority allowlist.
- Treat locked, unavailable, access-denied, or corrupt vaults and failed
  credential deletion as typed failures. There is no plaintext, settings,
  environment, or SQLite fallback.

The portable vault capability accepts only an exact profile/profile-revision/
host/account/server-contract authority and exact request/session/identity
generation. Its credential owner is noncopyable, caps nonempty material at
2,560 bytes so every supported native vault can represent it, exposes only
read-only bytes, and securely overwrites storage before release. Store is an
ownership-transferring atomic upsert; retrieve, absence, delete, failure, and
cancellation are distinct typed outcomes. No raw operating-system error text
or credential bytes enter provider errors or projections.

Windows and Linux native evidence must exercise HTTPS TLS, system proxy,
redirect rejection, certificate rejection, runtime network scope, and binary
dependencies in addition to their target-native vault lifecycle. A mock,
cross-compile, or macOS-only run cannot prove those platform claims.

## Normalized application boundary

The application-facing `github_provider_port` accepts semantic session and
read requests and returns standard/domain C++ normalized records or typed
failures. It does not expose:

- `QString`, `QJsonValue`, or other JSON/library records;
- `QNetworkRequest`, `QNetworkReply`, or `QProcess`;
- raw REST/GraphQL response structures;
- tokens, authorization headers, raw stderr, or unrestricted response text.

Normalized records include stable viewer, repository, pull-request, commit,
review, thread, comment, file, check, capability, rate-limit, and snapshot
identities as applicable. Count, byte, page, nesting, string, and allocation
limits are enforced before a result can be staged.

Actor identity uses GitHub's opaque global node ID plus a bounded login. The
node ID is case-sensitive and scoped by the selected profile and host; it is
never decoded or compared across GitHub installations. The authenticated
numeric account ID remains part of request authority, while the viewer's node
ID and login seed author consistency within each snapshot. A present author
must have both facts, and a deleted or unavailable author is explicitly null.
Entity, actor, and non-target search node IDs share one collision domain;
only an exactly correlated reference to the selected pull request may reuse
an existing node ID.

The GraphQL review-thread/viewed-state projection includes repository and
pull-request node IDs and must match the corresponding REST records. Each
GraphQL file path maps exactly once to the REST file collection and carries
`DISMISSED`, `UNVIEWED`, or `VIEWED`. Each thread carries an explicit `LINE`
or `FILE` subject plus per-target resolve/unresolve availability: line
threads require line anchors, file threads prohibit them, and every comment
path must match its thread path. REST and GraphQL records therefore cannot be
replayed across repository, pull-request, file, actor, or thread authority
without the complete candidate failing atomically.

The v1 selected contract admits at most 4 MiB per response page, 100 records
per page, JSON nesting depth 32, 64 members per object, 3,000 files, 250
pull-request commits, 1,000 search results, 50,000 aggregate records, 256 KiB
per body, and 16 MiB of normalized text. Callers may tighten these ceilings
but cannot relax them. Overflow, duplicate identity, malformed relationship,
unknown enum, incomplete GraphQL page, or allocation failure rejects the
entire candidate snapshot.

Recorded fixtures retain an exact sanitized field projection. They are not
complete raw GitHub payloads. Raw transports own status, content type,
pagination, header, and envelope validation and must discard additive
provider fields before the exact selected projection is normalized; no raw
response type or unbounded provider field crosses the adapter.

The BPR-0019 recorded facade executes one selected page for each operation.
Every transport response must explicitly certify that the selected page is
complete; unknown completeness or a declared continuation rejects the whole
candidate. The facade also rejects incompleteness in GraphQL `pageInfo` and
projected check/search totals. It is not a production pagination
implementation: BPR-0027 must exhaust REST and GraphQL pages under the
aggregate limits before submitting one complete snapshot. Search results use
the pull request's global node ID and repository-scoped number, never the
Search Issues numeric issue ID. Before projecting away raw URLs, the
production transport must also prove each result belongs to the canonical
repository-scoped request. No one-page fixture result may be used to claim
that a larger live pull request was loaded completely.

Provider callbacks return immutable completions and never mutate the active
cache, core, Qt model, or widget. The host posts each completion to the
serialized application lane, stages it under the exact request context, and
commits it only after the core emits the matching acceptance delta.

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

RFC-BPR-0002 composes only the read operations needed for viewer, repository,
PR metadata, files, commits, reviews, threads, comments, checks, search, and
capability probing. The mutation operations above describe the stable future
provider contract but are not callable through the RFC-BPR-0002 composition.

A no-mutation verification sink parses every REST method/route and the GraphQL
document's selected operation type. It rejects every GraphQL `mutation`
operation rather than relying on query names, endpoint names, or fixture
intent.

## Failure and diagnostic boundary

Transport and normalization failures are classified into bounded categories
such as authentication, permission, network, API version, unsupported
capability, rate limit, certificate, redirect, cancellation, timeout, process
startup, malformed response, and response limit. Provider-owned details are
redacted before a safe typed event crosses inward. Qt owns user-facing prose.

Raw remote content and credential-bearing diagnostics are never retained as
RFC evidence.

## Cache isolation

Provider sessions, capabilities, retries, rate limits, staged results, and
accepted normalized snapshots are keyed by profile revision, host,
authenticated account, repository/PR identity, and contract revision as
applicable. Switching profile or authenticated account cancels or suppresses
old work, clears visible shared projections, discards stages, and prevents
cache reuse across the old identity.

## Mutation safety

Every mutation requires an immutable target descriptor containing host, repository identity, PR number, expected head OID, authenticated user identity, and idempotency key where supported. Tests default to recorded contracts and a fake mutation sink. Live mutations require a user-designated safe PR.

RFC-BPR-0002 performs no mutation. Mutation safety becomes executable product
scope in RFC-BPR-0008.

## Build and distribution boundary

GitHub Actions has no role in building, testing, packaging, releasing, or
verifying the provider. Local and target-native tools produce the evidence.

The build and packaging records must nevertheless report the adapter's actual
Qt Network and platform-vault compile, link, and runtime dependencies.
RFC-BPR-0014 owns delivered-package signing and distribution certification; it
does not defer accurate dependency accounting from RFC-BPR-0002.
