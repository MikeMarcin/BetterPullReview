# RFC-BPR-0002 GitHub Host Implementation Architecture

This document specializes the [GitHub provider
contract](github-provider-contract.md) for the RFC-BPR-0002 implementation.
The governing rule remains:

> **The core decides. The host performs. Qt presents.**

RFC-BPR-0002 adds a read-only GitHub.com and GitHub Enterprise Server 3.18
vertical slice. It does not add a second source of shared truth, a GitHub
mutation path, or a widget-to-provider shortcut.

## Selected boundary

```text
Qt presentation adapter
    host/profile controls, PR locator input, QActions, metadata,
    conversation model, errors, focus, accessibility
        |
        | typed semantic intent
        v
application host
    bounded observations, provider session, staged normalized snapshots,
    capability/cache authority, cancellation, lifecycle
        |
        | deterministic transition
        v
functional core
    active opaque identity, command legality, request/generation identity,
    completion acceptance, semantic deltas and diagnostics
        |
        | typed semantic request
        v
application host
        |
        | narrow github_provider_port
        v
GitHub adapter
    explicit profiles, REST/GraphQL request contracts, normalization,
    opt-in CLI read boundary, native HTTPS, response limits and redaction
        |
        +-- gh/QProcess read integration in progress on macOS
        |
        +-- production QNetworkAccessManager plus a platform credential vault
```

Provider, transport, process, network, vault, JSON, and Qt types do not enter
the functional core. `QProcess` and `QNetworkAccessManager` are permitted only
inside their designated external adapters. Qt Widgets presentation code
receives application projections and has no provider capability.

## Authority allocation

| State | Authority | Core representation | Qt representation |
|---|---|---|---|
| Host profile configuration | Application host profile registry | Opaque `host_profile_id` | Selectable non-secret profile facts |
| Authenticated account | Selected provider session | Opaque host/account identity and generation | Visible hostname and account login |
| Credentials | Native platform vault in milestone one; CLI-owned storage for explicit CLI read opt-in | None | None |
| Capability detail and evidence | Host capability cache | Compact tri-state capability set and revision | Supported, unsupported, or unknown presentation |
| Shared PR and conversation state | GitHub, observed through the provider session | Stable identity, accepted snapshot identity, bounded summary | Immutable metadata and conversation projections |
| Normalized response cache | Application host | Snapshot identity only | Incremental rows/cards derived from accepted snapshots |
| Command legality and stale-result policy | Functional core | Interaction state and typed transition policy | `QAction` projections |
| Focus, input text, geometry, and row indexes | Qt presentation adapter | None unless semantically required | Native Qt state |

An accepted normalized snapshot is a cache of a GitHub observation. It is
never a competing shared authority. Refresh replaces or repairs it from
GitHub; local disagreement cannot overwrite GitHub facts.

## Identity model

The functional core uses opaque, strongly typed identities:

```text
host_profile_id
git_host_id
github_account_id
repository_id
pull_request_id
provider_snapshot_id
capability_revision
request_id
application_session_id
request_generation
```

Host profile configuration contains hostname, product kind, REST and GraphQL
roots, authentication transport, trust/proxy mode, REST API version, and a
profile revision. Those operational facts remain in the host and provider
adapter. The core sees only the profile identity and normalized observations
required for deterministic policy.

Session establishment occurs before the provider has proven the account
identity, so it uses:

```cpp
struct github_session_request_context
{
    request_id Request;
    application_session_id ApplicationSession;
    host_profile_id Profile;
    request_generation Generation;
};
```

A pull-request request carries every identity already known at submission:

```cpp
struct pull_request_request_context
{
    request_id Request;
    application_session_id ApplicationSession;
    host_profile_id Profile;
    git_host_id Host;
    github_account_id Account;
    repository_locator Repository;
    pull_request_number Number;
    request_generation Generation;
};
```

The completion adds canonical repository, pull-request, and snapshot
identities. Acceptance compares the entire applicable context, not only the
request number. Host/profile/account switching increments the generation,
clears active shared projections, and makes every earlier completion stale in
constant time.

## Validated host authority

A host-profile candidate becomes usable only through the GitHub adapter's
validated-profile type. The candidate must have trusted `explicit_user`,
`builtin`, or `managed` provenance, nonzero opaque profile/host identity and revision,
system proxy/trust modes, a selected authentication transport, and one of the
two enum-backed server contracts. Pull-request/provider content and redirects
are never profile provenance.

The initial contracts admit only canonical lowercase ASCII FQDNs, with DNS
label and total-length bounds and no IP literal, custom port, trailing dot,
userinfo, IDNA alias, URL syntax, query, or fragment. Their roots are exact:

| Contract | Hostname | REST root | GraphQL root |
|---|---|---|---|
| GitHub.com | `github.com` | `https://api.github.com` | `https://api.github.com/graphql` |
| GHES 3.18 | configured FQDN | `https://HOST/api/v3` | `https://HOST/api/graphql` |

Both select REST version `2022-11-28`. Product kind, routes, roots, and
version are derived from the validated contract rather than accepted again
as request strings. A redirect is only a target-validation input: it cannot
create or mutate a profile. GraphQL redirects are rejected; a REST redirect
may proceed only when it remains query-free HTTPS, on the exact REST
authority, and inside the selected API namespace.

Profiles contain no login, account, capability, or credential material.
Authenticated account identity is a later `/user` observation, and
capabilities are separately observed and cached facts.

## Capability model

Every probed capability has three states:

```text
Unknown
Supported
Unsupported
```

`Unknown` means the selected session has not produced sufficient evidence.
It is not treated as supported, and it is not mislabeled as unsupported.
RFC-BPR-0002 records at least:

- review-thread reads;
- viewed-file mutation support;
- resolve-thread support;
- unresolve-thread support.

The latter mutation capabilities are observed for future command legality but
are not exposed as executable mutations in RFC-BPR-0002. A capability snapshot
is keyed by profile revision, host, authenticated account, product/server
facts, and probe-contract revision. The core receives the compact tri-state
set plus `capability_revision`; the host retains detailed probe evidence.

The adapter sends a fixed plan of nine serial, bounded GraphQL introspection
queries to the exact validated GraphQL root. Each limited collection field
expands no more than twice per document, satisfying the GitHub repetition limit
observed in [[BPR-0559]]. Exact canonical request and completion correlation
include the part index. Every response is validated, with a 1 MiB aggregate
body ceiling and 50,000-value aggregate bound; only the complete merged result
can establish capabilities. Cancellation, invalid metadata, errors, and rate
exhaustion stop further parts. A later GHES response omitting the whole rate
tuple does not discard an earlier observation. This adds connection round trips;
[[BPR-0604]] defers evaluation of schema-based alternatives.

The capability wire path uses Glaze throughout: a typed outgoing query record,
a private dynamic introspection tree, and complete-document parsing for error
classification and sanitized diagnostics. The existing type-dependent shape
validators remain authoritative; no JSON representation crosses into core or
application ports. Batch serialization retains explicit null members and sorted
object keys. Missing diagnostic fields remain distinct from explicit null, and
credential reflection removal still runs on decoded text before length checks.
Structural preflight and aggregate limits precede capability promotion.

The plan inspects the mutation root and
the pull-request, changed-file, review-thread, viewed-state enum, and four
mutation input/payload contracts without executing a mutation. Names and type
signatures are case-sensitive and order-independent; duplicate names,
partial aliases, GraphQL errors, malformed wrappers, or a limit breach do not
form evidence.

`Supported` means a complete authenticated schema observation matches the
recorded call contract. It does not assert that the current account may
mutate every target. `Unsupported` is emitted only by a complete observation
that conclusively lacks or incompatibly changes a required fact.
Authentication, permission, network, API-version, rate-limit, certificate,
redirect, timeout, cancellation, malformed, partial, and oversized results
remain explicitly unavailable with all four core states `Unknown`.

Review-thread support is a dependency of resolve and unresolve support.
Viewed-file mutation support requires both mark and unmark contracts.
Per-thread `viewerCanResolve`/`viewerCanUnresolve` and per-file
`DISMISSED`/`UNVIEWED`/`VIEWED` state are normalized target facts, not host
capability overrides. GitHub exposes no equivalent read-only per-file
mutation-permission fact, so mutation authorization remains fail-closed until
RFC-BPR-0008 exercises a designated safe target.

The application cache accepts only four canonical, conclusive recorded or
observed facts. Its exact key is profile, profile revision, host,
authenticated account, server contract, and probe-contract revision. It uses
an injected monotonic logical epoch rather than a clock, generates a new
nonzero capability revision for every successful re-probe, reuses that
revision on a hit, expires at a half-open boundary, never caches unavailable
results, never evicts fresh authority, and exposes exact/profile/identity
invalidation. Capacity and freshness are bounded (defaults 16 and 300 epochs;
ceilings 64 and 900).

## Typed vocabulary

### Intents

```text
activate_host_profile_intent(profile)
open_pull_request_intent(repository locator, PR number)
refresh_pull_request_intent
```

Profile creation or editing is a typed application-host settings operation.
It validates and persists non-secret operational configuration before the core
is offered an opaque profile identity. The core does not validate URLs or
handle credential material.

### Bounded observations

```text
current_github_identity_observation
current_host_capabilities_observation
current_pull_request_summary_observation
```

Observations carry their complete source identity and revision. They expose
facts, not provider, cache, process, network, or vault objects.

### Host requests

```text
establish_github_session_request
fetch_pull_request_request
invalidate_github_identity_request
```

`fetch_pull_request_request` is semantic. It does not contain a URL, REST
route, GraphQL text, `gh` argument, HTTP header, or credential.

### Completion events

```text
github_session_ready_event
github_session_failed_event
pull_request_loaded_event
pull_request_load_failed_event
request_cancelled_event
```

A loaded event carries correlation and normalized summary facts plus a
`provider_snapshot_id`; the full normalized snapshot remains host-owned.

### Deltas

```text
github_identity_delta
capability_delta
accepted_snapshot_delta
pull_request_summary_delta
command_delta
status_delta
diagnostic_delta
```

`accepted_snapshot_delta` is the only authority for committing a staged
provider result to the active host cache. A changed-domain flag by itself is
not sufficient.

## Session, open, and refresh flows

### Establish a session

```text
Qt profile selection
    -> activate_host_profile_intent
    -> core clears the prior active identity, advances generation,
       disables dependent commands, and emits:
           invalidate_github_identity_request
           establish_github_session_request
    -> host cancels/supersedes old work and calls github_provider_port
    -> provider resolves viewer identity and probes capabilities
    -> immutable completion returns to the serialized decision lane
    -> github_session_ready_event
    -> core validates request/application/profile/generation
    -> identity, capability, command, and status deltas
    -> host publishes bounded projections
```

Authentication, permission, trust, network, API-version, rate-limit, malformed
response, and output-limit failures return as typed failure events. Raw
provider text is never a core diagnostic or UI string.

### Open a pull request

```text
Qt Open QAction
    -> open_pull_request_intent(repository locator, PR number)
    -> core validates semantic readiness from bounded identity/capability facts
    -> fetch_pull_request_request with complete correlation context
    -> host calls the selected normalized provider
    -> provider performs REST-first reads and narrow GraphQL thread reads
    -> provider returns an immutable bounded normalized snapshot
    -> host posts the completion to the serialized decision lane
    -> host stages the snapshot by request context
    -> pull_request_loaded_event(snapshot identity and summary)
    -> core either:
         accepts and emits accepted_snapshot_delta, or
         rejects as stale and emits no acceptance delta
    -> host commits and publishes only an explicitly accepted snapshot
```

### Refresh

Refresh submits the active immutable pull-request key under a new request
generation. The old accepted snapshot may remain visibly marked as refreshing,
but an incomplete, failed, canceled, or stale replacement never partially
changes it. A successful accepted snapshot is compared by stable identity and
published as bounded metadata and conversation changes.

## Staged snapshot acceptance

Provider callbacks never mutate an active cache, core state, Qt model, or
widget. The callback captures an immutable normalized result and posts it to
the decision executor.

On the serialized lane the host:

1. stages the result under its exact request context;
2. dispatches the corresponding typed event to the core;
3. inspects the transition for an exact `accepted_snapshot_delta`;
4. commits the matching staged snapshot to the active cache;
5. derives and publishes bounded application projections;
6. discards every unaccepted or superseded staged result.

This order is mandatory. Committing before the core decision would allow a
late result to become an alternate authority even if Qt had not displayed it.
Publishing merely because `changed_domain` contains a broad content bit would
make acceptance ambiguous and is therefore prohibited.

## Accepted pull-request revision handoff

RFC-BPR-0003 consumes accepted pull-request revisions through a dedicated
application boundary. It does not poll `accepted_snapshot()`, subscribe to a
Qt projection, or extend the GitHub host into a Git/cache/catalog controller.
After, and only after, step 4 of staged-snapshot acceptance, the GitHub host
offers one immutable `accepted_pull_request_revision` to an application-owned
sink. The offer is observational: a throwing or rejecting sink is contained
and cannot roll back the GitHub core transition, cache commit, or Qt
publication. The epoch host stages and decides its own work independently.

The handoff contains only bounded, provider-free values:

- application session identity, exact host profile identity and GitHub profile
  revision, Git host identity, canonical hostname, server contract, and GitHub
  identity generation;
- authenticated numeric account identity and the exact scoped viewer actor
  identity;
- canonical repository identity and validated repository locator,
  pull-request identity and number, pull-request generation, and provider
  snapshot identity;
- algorithm-tagged exact base and head Git object identities and an opaque
  base-branch identity derived from the accepted canonical repository/branch
  facts, never from display text or an untrusted ref name;
- the one bounded newest-current-viewer review candidate, including exact
  review/actor identity, typed provider order, state, and commit identity; and
- an opaque, noncopyable, revocable native smart-HTTPS lease when the accepted
  profile uses native HTTPS.

An object identity is an algorithm tag plus exactly a nonzero 20-byte SHA-1 or
32-byte SHA-256 binary digest. Provider input accepts only the corresponding
40- or 64-character hexadecimal form and decodes it to bytes; complete
lowercase hexadecimal text is a presentation projection only. Untagged text,
all-zero digests, and a mix with the repository object format are invalid.
The base-branch identity is a versioned fixed digest of repository identity
and the separately validated full `refs/heads/...` value; raw branch text
never becomes core identity or a Git argument. The full normalized GitHub
snapshot, provider records, URLs, headers, credential objects, and transport
types remain owned by the GitHub host and adapter.

The corresponding `pull_request_authority_invalidated` notification is sent
when application session, profile/revision, host, authenticated account,
repository, pull request, or GitHub identity generation changes. The host
revokes the smart-HTTPS lease, sends invalidation, and only then clears the
old accepted GitHub authority and its identity-sensitive projection. Every
epoch request and completion retains the complete handed-off context, so the
epoch host can cancel work and reject a late terminal without consulting
mutable GitHub host state. Refreshing the same authority may retain the last
accepted epoch projection while new work is pending; an authority switch
cannot.

## Normalized provider boundary

The application-facing `github_provider_port` accepts semantic session and
fetch requests and completes with normalized records or typed failures. Its
read model includes bounded forms of:

- viewer identity;
- repository identity;
- pull-request identity and metadata;
- base and head commit identities;
- reviews;
- review threads and comments;
- issue comments;
- files, commits, and checks required by the provider contract;
- capability and rate-limit observations.

Provider-library and transport types stop at the adapter:

```text
not allowed inward:
    QString / QJsonValue / QJsonDocument
    QNetworkRequest / QNetworkReply
    QProcess / process exit records
    raw REST or GraphQL response records
    Authorization headers or tokens

allowed inward:
    standard/domain C++ normalized records
    stable identities and revisions
    bounded plain text
    typed failure categories
    redacted metadata-only operational trace facts
```

Normalization validates count, byte, nesting, page, string, and allocation
limits before a snapshot can be staged. Failure is atomic: a rejected response
does not partially update the accepted snapshot.

Provider failure mapping is total and intentionally bounded. A table-driven
application-host contract enumerates all 15 `github_provider_failure` values,
drives each through a pull-request completion, and requires the corresponding
typed core reason with no accepted host/core snapshot, projection payload, or
provider-owned detail. Process-startup and generic-unavailable failures both
map to the safe network reason; retry policy remains separately typed and
bounded.

The production provider, not the recorded one-page facade, owns continuation
exhaustion. REST pages follow only a validated `Link` continuation on the
selected authority; GraphQL connections advance only through nonempty,
changing `pageInfo.endCursor` values while `hasNextPage` is true. Page count,
per-page bytes and records, aggregate bytes and records, cursor size, expected
GitHub totals, repository identity, pull-request identity, and expected head
OID are checked before one snapshot is returned. A repeated or missing cursor,
unknown completeness, truncated count, authority drift, or failed page
rejects the complete read.

The REST `ListReviews` contract is additionally order-sensitive. GitHub.com
and GHES 3.18 document that reviews are returned in chronological order. The
provider preserves the exhausted page sequence and attaches a bounded,
one-based chronology ordinal to each normalized review; it does not sort by
login, database identifier, state, commit, or locally observed time.
`submitted_at` is parsed when present but is not used to repair, deduplicate,
or reorder provider chronology, and a pending review may omit it.

Current-viewer association requires exact actor/node identity under the
accepted host/account authority, never login text. `My last reviewed` selects
the highest-order review by that actor whose state is not pending and whose
exact algorithm-tagged commit identity is present. A dismissed review is
still a submitted review. Once selected, that exact candidate remains
authoritative even when its object is not retained; the alias is then
explicitly unavailable and never falls back to an older resident review,
substitutes the current head, or guesses from a timestamp. Provider chronology
is an observation; deterministic alias legality and selection remain core
policy.

Rate-limit observations are resource-scoped. Core REST, Search, and GraphQL
may update only their corresponding `core`, `search`, and `graphql` buckets.
GitHub.com requires the applicable exact four-header tuple; GHES may omit an
entire tuple, but a partial, duplicate, malformed, unknown, or wrong-resource
tuple is invalid. The provider and host merge independent buckets across
pages and retries. A primary, secondary, or structured GraphQL `RATE_LIMITED`
terminal retains its exact causal record; a bounded `Retry-After` remains a
separate scheduling observation. Raw provider messages never cross inward.

An authenticated pull-request refresh failure now retains its bounded diagnostic
in the connected server card's **Details → Last problem** disclosure, including
the distinct primary/secondary wording and reported retry interval. The account
remains connected. BPR-0968's large-review course proves that presentation and
explicit recovery, but also observes that another manual Refresh currently
reaches the provider before `Retry-After` expires. Window-read admission has no
shared account/resource cooldown. The standalone card retains that scheduling
and cross-window cancellation/authority work; this display does not promise an
enforced delay or automatic retry.

### Personal discovery reads

[[BPR-0988]] (RFC-BPR-0026) adds a second, independent read to the same
provider: `personal_discovery_port::discover` lists the viewer's open pull
requests of one saved repository without reading any of them. It is two
operations outside the recorded snapshot manifest:

- `SearchPersonalPullRequests`: `GET {rest}/search/issues` with
  `q=is:pr is:open author:LOGIN repo:OWNER/NAME` or
  `review-requested:LOGIN`, the qualifiers the GitHub.com and GHES 3.18 search
  contracts both document. The login is the accepted viewer's, admitted only
  as letters, digits, hyphen and underscore, so it cannot become another
  qualifier. One page of at most 100 items is read and a continuation is
  never followed: `incomplete_results`, a total above the page, or a `next`
  link makes the answer partial.
- `ListRequestedReviewers`: `GET {rest}/repos/OWNER/NAME/pulls/N/requested_reviewers`
  for at most 30 rows of one answer. An authored row takes its awaited
  reviewer count from it; a requested row is kept only when the users it
  lists include the viewer, so a team-only request is not discovered.

The search's wire parent is a `core::repository_request_context`: the same
application session, profile, host, account and identity generation a pull
request context carries, scoped to one repository and naming no pull request.
Wire validation admits that parent for the personal search of exactly its
repository and for nothing else, and refuses the personal search under any
other parent; the requested-reviewers read runs under its pull request's
context. Native HTTPS correlates both against the exact account credential as
it does a pull request read, and an anonymous authority refuses both as
identity operations. The search bills the `search` rate resource and the
detail read `core`. Any failed request fails the whole answer, a search whose
repository qualifier GitHub refuses (422) is reported as lost access, and
nothing in an answer is a snapshot, a file, a commit, a thread or a diff.

## Transport ownership

The provider adapter validates the explicit host profile. Native profiles
construct native HTTPS. Unsupported CLI compositions return `cli_unavailable`;
the macOS opt-in read composition is described below.

### GitHub CLI broker

RFC-BPR-0015's amended first read milestone has a qualified macOS fixture
path. Native HTTPS remains the default. The tests-disabled macOS composition
has explicit
CLI read opt-in on the selected GitHub.com or GHES host, with a Connect warning,
observed `/user` identity, pre/post-request account checks and a read-only lease.
It invokes the configured `gh` directly with bounded arguments, input, output
and time; account discovery and API use require `gh` 2.96 or later
(including later majors)
because the account inventory uses `gh auth status --json hosts`
(every account on each host; `--active` only for the default choice).
Older versions are refused before account status runs and the UI requires an
upgrade. BetterPullReview never passes `--show-token`.
This is a version check, not proof of an official executable's provenance.
The r2 trust-boundary amendment permits a bounded exact-repository `gh` Git
operation for objects; `gh` may internally launch Git and its shell credential
helper. Every macOS `gh` launch must run under OS-enforced file-write denial,
with a narrowly owned temporary object-store exception only for the bounded
clone; an unavailable boundary refuses the CLI profile. This covers `gh`'s
possible first-use global configuration migration, which a post-request check
cannot prevent. BetterPullReview must not read the helper's token exchange or
change global authentication. Full Git comparison requires `gh` 2.96 or later
(RFC-BPR-0015's account and API floor) and Git 2.49 or later
(`clone --revision`), not one exact vendor or banner. Before any credentialed
`gh` use, bounded write-denied local `gh api --help`, `gh repo clone --help`
and `gh auth git-credential --help` probes must show the flags and usage the
app invokes, and `git clone -h` must show the required clone options. A failed
check and an unsupported tool reach the comparison as distinct typed reasons.
Capability admission retains actual clone and exact-object validation. Native
qualification evidence remains installation-specific. The 32 MiB per-file
ceiling and supervised 96 MiB/128-entry temporary-store abort threshold
remain; the aggregate threshold is not a hard disk quota. Local refs reflect the accepted
API snapshot, not fresh remote advertisement. A tests-disabled macOS fixture
now renders a full PR diff through the CLI profile; an isolated build with
the process adapter removed from composition visibly refuses the same healthy
fixture. A macOS native read-only probe now verifies one official `gh` HTTPS clone of a
small owner-controlled private repository through `gh auth git-credential`;
`gh` reports a keyring token source and a matching Keychain item exists.
The probe does not bind the precise Security API calls, credential generation,
or concurrent account selection. The private repository name and objects
were not retained. On macOS, the opted-in CLI profile now routes whole-review and
right-side inline-comment publication through the existing mutation admission,
a separate exact-host/account/PR/operation confirmation for each ordinary write, and
server reconciliation. The configured gh account may still change between
checks; an ambiguous attempted write is not posted twice. Fixture actions
exercise this path; no live PR was written. Windows and Linux remain
unavailable. Mac qualification is recorded by BPR-0787. RFC-BPR-0014 is now
Auto-Verified, and RFC-BPR-0015 became Auto-Verified on 2026-09-23 through
BPR-0788. The product owner approved the pending review of the CLI
authority and protocol changes made after that qualification. That
approval is an owner decision, not an independent reviewer's report.

Tour Publish is a bounded grouped-confirmation extension: the preview binds one
immutable plan to the exact actor, head, sources, prose and allowed returned-link
slots. Each journaled write consumes its own one-use child grant. Stop or an
identity/draft change revokes remaining authority; restart reconciles read-only
and needs an explicit preview and Resume. Compatibility comments are explicitly
non-atomic, and an optional description edit is a separately named operation.
These Tour operations remain unavailable on an unqualified host/transport.

BetterPullReview does not extract or copy CLI tokens or change global CLI
authentication. Opt-in trusts the machine's configured executable, configuration,
socket routing, CA and redirect behavior, active account and helper processes.
Pre/post observations can detect some account changes but cannot bind a credential
generation or prevent a concurrent account switch. Executable/configuration/CA
replacement and process escape remain uncontained risks. Read opt-in grants no
publishing authority. RFC-BPR-0013 keeps a 250-record limit for commits returned by the pull-request
endpoint. A larger reported total is accepted only when that endpoint
returns exactly 250 records ending at the accepted head. The UI names the
reported total and returned count separately; this does not establish that
any particular private pull request can render.

#### Historical RFC-BPR-0002 boundary and evidence

The following records the original zero-launch milestone and its strict trust
contract. The RFC-BPR-0015 amendment supersedes that enablement prohibition;
the recorded risks remain unresolved, not passing containment evidence.

Native HTTPS is the sole milestone-one production authentication transport.
CLI profiles remain visible, but the public provider returns fixed typed
`cli_unavailable` before transport construction, CLI
executable/configuration discovery, or a process-port call. The broker uses a
canonical absolute `gh` executable through a designated `QProcess` runner
only as retained, tested scaffolding for a possible future transport. It
invokes argument arrays without a shell, supplies request bodies through
standard input, disables prompting and
incidental output, uses a canonical directory currently outside repository
ancestry, bounds all streams and duration, and redacts failures. It neither
asks for nor extracts a token.

Arguments come from an operation-specific allowlist. The child environment is
constructed from an explicit minimal allowlist rather than inherited and then
scrubbed. It contains only the platform facts required for the CLI's own
credential store and the selected trust/proxy policy plus forced noninteractive
settings. Token, repository/host override, debug, config-directory override,
pager, editor/browser, color, TTY, update, telemetry, spinner, and prompt
influences are absent without reading or logging their values. Contract tests
inject a previously unknown canary environment name/value and prove that it is
not inherited.

The configured hostname is explicit. A broker result may be normalized only
when the selected adapter can establish that the request stayed within its
approved authority policy, used the correlated credential identity, and was
not rerouted by ambient configuration. A redirect-requiring or otherwise
unverifiable contract, cross-authority redirect, certificate failure,
uncontrolled persisted transport setting, or unbound active-account
selection fails closed. A configured hostname is not by itself authority,
identity, redirect, or certificate evidence. If the installed CLI contract
cannot prove these properties, broker mode is unavailable.

BPR-0025 implements that boundary as two adapter-private, standard-C++ ports.
The wire port retains the complete parent request, profile revision, host,
operation, and page correlation on every success, typed failure, or
cancellation terminal. The lower process port receives only one validated
absolute executable, an element-wise argument vector, a freshly constructed
environment, a canonical working directory currently outside any worktree or
bare-repository ancestry, bounded standard input, and fixed stream/time/reap
limits. The concrete retained runner implementation is the only GitHub source
that uses `QProcess`; it returns bounded standard output only on a clean
success and otherwise discards every partial stream and returns a fixed
failure category. It is tested scaffolding and is not composed into the
milestone-one public executable. This directory validation does not prove
owner-controlled permissions or bind the directory identity through launch.

The runner owns one serialized worker and creates, uses, and destroys each
`QProcess` there. An absolute deadline covers queueing and is checked again
immediately before launch. Allocation-free shape validation and
pre-cancellation precede work-item allocation and request copying. On Unix the
runner requests closed inherited descriptors, disabled core dumps, reset
signal handlers, and `CreateNewSession`; the child is the new session's
initial process-group leader. A compiled child directly observes those four
facts. On Windows every child inherits only its three standard handles,
through the platform target's launch restriction that the Git adapter's
unguarded Git also takes; a launch whose handle list cannot be built is
refused by process creation itself, so nothing starts and the run fails as
`start_failure`, and a child created without the restriction all the same is
refused after the start (BPR-1114). A compiled Windows case holds a C runtime
stream on a file while a child starts and opens that file with no sharing
while the child runs; another shows through a job's process accounting that a
refused launch creates no process. Compiled lifecycle cases prove cleanup on clean exit, failure, timeout,
and cancellation signals descendants that remain in that initial process
group and reaps the direct child; a separate escape case proves a descendant
that moves to another group can survive before the fixture removes it. A
compiled seam also proves completion-watchdog quarantine, zero launch while
quarantined, and recovery. Source and hostile architecture gates enforce
shutdown cleanup and process-group resolution and separately require an actual
bounded-reap failure to return fixed `reap_failure`, quarantine the slot, and
retain the live object for continued reaping on the same worker rather than
invoking Qt's destructor wait or creating another reaper. The compiled runtime
suite does not force that live-process failed-reap path.

Public reads use a fixed buffer and request no more than the remaining
accepted bound plus one byte. That bounds retained and adapter-accepted bytes,
not Qt's private in-flight pipe allocation, for which Qt exposes no cap. Raw
macOS inspection observes the requested environment plus only Darwin's
deterministic `__CF_USER_TEXT_ENCODING` addition. The Linux raw-runner fixture
requests its locale explicitly; that is not an extra production broker
variable. Windows Job Object descendant containment, private pipe bounds, and
target-native CLI runner behavior are deferred to RFC-BPR-0015 and are not
inferred from the Unix evidence. BPR-0036 retains the Windows native-HTTPS,
vault, and public-path checks.

The invocation compiler revalidates the complete canonical wire request. It
emits only `gh api`, one translated relative endpoint, explicit
`--hostname`/`--method` pairs, canonical noncredential headers, and
`--input -` for the selected GraphQL `POST`. GitHub.com REST drops its leading
slash, GHES REST drops the already-validated `/api/v3/` prefix, and both
GraphQL contracts become the CLI's literal `graphql` endpoint. Absolute URLs,
repository placeholders, file expansion, caller-supplied flags, status or
debug output, response caching, pagination, templating, fields, previews, and
credential-bearing arguments are not representable.

The current direct audit used the official macOS arm64 `gh` 2.96.0 release
dated 2026-07-02. Its downloaded ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`,
and its extracted binary SHA-256 was
`b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`.
An owner-private copy produced one fixed bad-certificate TLS alert and no HTTP
request for a self-signed loopback certificate. A controlled cross-authority
`302` received the exact 47-byte request body and synthetic authorization at
its selected origin, then reached the distinct sink without either.
Equivalent `307` and `308` POST probes reached their selected origins but did
not produce an observed sink request; that absence remains unproven rather
than accepted as redirect rejection. The installed `gh` 2.95.0 binary,
SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
was rerun with the same material outcomes. Each private executable digest was
checked again after its probes.

The observed `302` is sufficient to falsify exact-authority enforcement. The
CLI's ordinary success output provides neither a stable redirect chain nor
the effective final authority, and the available debug/status surfaces are
outside the broker allowlist. The direct loopback probe uses a full URL only
to select a disposable local fixture from an owner-private canonical
non-repository directory.
Because `gh` rejects an ephemeral `host:port` as `--hostname`, the probe keeps
the synthetic `github.com` startup hostname and never treats the loopback run
as exact production-authority proof. Source inspection confirms that relative
production endpoints use the same default Go redirect client. A
verification-only owner-private `GH_CONFIG_DIR` holds the random synthetic
token solely to pass the CLI startup authentication gate. That override is
absent from production.

Omitting that override does not eliminate ambient configuration.
`http_unix_socket` is an active CLI transport setting, not a telemetry-only
fact: for both selected binaries, the isolated probe loaded the setting and
the exact `gh api` invocation made one authenticated HTTP request through an
AF_UNIX sentinel with the expected synthetic Authorization value. The
production broker would need the selected platform home or application-data
paths for CLI account state. Canonical `SSL_CERT_FILE`/`SSL_CERT_DIR` paths
likewise do not bind selected trust-material identity or contents through the
CLI's later open.

The current `gh api` command exposes no per-invocation `--user`.
`gh auth token --user` prints the token and is prohibited;
`gh auth switch --user` mutates persisted active-account state and is racy
with BetterPullReview's correlated identity generation. Current selected
source resolves `ActiveToken` per round trip, falls back from a keyring error
to a blank-host token, and uses a macOS keyring path that launches
`/usr/bin/security`.

Per-account pinning ([[BPR-1373]], RFC-BPR-0015's 2026-10-09 owner amendment)
selects an account with neither command. A CLI profile chosen for one login
runs each `gh` call with `GH_CONFIG_DIR` naming a private directory that the
app writes for that call alone and removes afterwards: owner-only, read-only,
holding `config.yml` and a `hosts.yml` that names only that host and login.
`gh` reads that login's keychain token itself, so no token passes through
BetterPullReview, and no `GH_TOKEN`-family variable is ever set. When the
login has no keychain entry, `gh` falls back silently to the blank-host
(active) token. The [[BPR-1370]] viewer check, held to the profile's host,
login and first-observed account ID, is therefore the proof: any other viewer
refuses the read and the lease, and the profile shows the account signed out
in `gh`. This binds the account by observation, not by credential generation.
The window between `gh`'s keychain read and its request remains.

Consequently the retained CLI transport exposes eight typed reasons:
milestone-one product-scope deferral, executable-image identity,
cross-authority redirect behavior, ambient configuration, credential/account
identity, working-directory identity, descendant-process containment, and
numeric PID/process-group identity reuse. It returns `cli_unavailable` before
starting a child; the public composition rejects CLI earlier and makes zero
process launches. A fake process cannot create trust, and native HTTPS
evidence cannot be borrowed. RFC-BPR-0015 owns any positive CLI enablement and
must bind or eliminate every blocker without a privileged helper, weakened
security, or GitHub Actions.

### Native HTTPS

BPR-0026 implements the adapter-private `github_wire_transport_port` with one
dedicated `QThread`, event loop, and `QNetworkAccessManager` per transport
instance. The manager and every request/reply object stay on that worker.
`perform` admits one operation at a time, revalidates the complete correlation
and canonical wire request before network work, and returns only the original
correlation plus a bounded success, typed error, or cancellation. BPR-0027
adds pagination, normalization, host staging, and the application-facing
provider path. BPR-0029 composes that provider and transport in the public
executable for explicit GitHub.com and GHES profiles. The recorded public
fixtures substitute their own transport factory behind that composition; they
exercise the surrounding QAction/provider/host/core/Qt path, not a real native
network request.

The production transport asks `QNetworkProxyFactory::systemProxyForQuery` for
each initial or redirected URL. It accepts at most 16 returned candidates,
retains supported direct, HTTP, and SOCKS5 candidates in system order, and
tries the next candidate only after a retryable connection or proxy failure.
Proxy-authentication failure is retryable to a later system candidate; origin
authentication, TLS, HTTP, malformed-response, and policy failures are not.
Each attempt clears manager-local access and connection caches and selects
only that candidate; no process-global proxy is mutated. On generic Unix,
Qt's system policy may use libproxy/desktop sources or lowercase
`http_proxy`, `https_proxy`, and `no_proxy`. Production code does not call
`getenv`, but “system proxy” is not a claim of environment independence.

The profile's validated authority governs the origin request. The underlying
connection may instead target one of the bounded HTTP or SOCKS5 proxy
endpoints returned by Qt system policy. Those proxy endpoints are routing
intermediaries rather than validated GitHub origins, and a proxy challenge is
never answered with the GitHub credential.

System/PAC proxy discovery is synchronous inside Qt. System-root enumeration
and request configuration are also synchronous. The absolute 30-second
deadline is checked before and after those calls and bounds subsequent Qt
transfer/event-loop work, but it cannot preempt a blocked discovery call.
Prompt cancellation sets a stop flag and queues an abort on the worker, with
a five-millisecond in-loop poll as a backstop; completion can still be delayed
until synchronous discovery returns. Shutdown can wait for the same worker.

Every hop receives a fresh request with manual redirect policy, no cookie,
cache, or authentication reuse, HTTP/2 disabled, `Accept-Encoding: identity`,
and HTTP/1.1 ALPN. TLS configuration starts from an explicit copy of the
operating-system CA set, removes local identity, session-ticket, backend, and
pre-shared-key state, selects `VerifyPeer`, and permits TLS 1.2 or later.
Certificate errors and pre-shared-key challenges abort; there is no
certificate-error bypass or process-global TLS mutation. A terminal requires
the `QNetworkReply::encrypted` completed-initial-handshake signal, a non-null
peer certificate, no accepted `sslErrors`, the configured peer-verification
and TLS-1.2-or-later policy, no HTTP/2 use, and the exact requested reply URL.
Qt's scheme-associated
`ConnectionEncryptedAttribute` is an additional terminal consistency check,
not by itself proof of a negotiated encrypted session. The meaningful
handshake/trust evidence is the completed-handshake signal and peer
certificate under the enforced verification policy, together with the
trusted/untrusted fixture outcomes. Qt's public reply surface used here does
not directly record the negotiated TLS version, so the implementation does
not claim that stronger fact.

Qt automatic redirects are disabled. The transport permits at most three
manual redirects, only for REST `GET`, only to a canonical query-free HTTPS
path on the exact validated REST authority and within its API namespace.
GraphQL redirects, cross-authority targets, loops, malformed targets, and a
fourth redirect return typed `redirect`. A newly validated URL, proxy
candidate list, TLS configuration, request, and Authorization header are
created for each accepted hop.

Adapter-visible bounds are fixed: 256 KiB per request body, 4 MiB per response
body, 64 response headers, 64 KiB of aggregate response-header text, 8 KiB per
header field, 16 system proxy candidates, three redirects, and one
30-second absolute operation deadline. Synchronous discovery may extend
wall-clock completion because it cannot be preempted; the request uses only
the remaining deadline after discovery returns. Reads use fixed 16 KiB chunks
and request no more than the remaining response limit plus one byte.
Successful responses must be HTTP 200 JSON with no content encoding, a
consistent content length, and bounded pagination metadata. Authentication,
permission, network, API-version, rate-limit, certificate, redirect, timeout,
cancellation, malformed-response, response-limit, and unavailable outcomes
remain typed and redact remote detail.

Capability-discovery refusals additionally carry a finite source-defined category
through the provider and exact accepted host terminal. The category distinguishes
transport metadata, response envelopes and schema normalization, provider
consistency and local host/cache failures without carrying remote values or
text. Deferred credential cleanup preserves the category with its original
request; stale replacements cannot inherit it. Qt renders a fixed sentence in
the existing copyable diagnostic. Invalid category values display an explicit
unavailable-detail fallback and do not change failure or retry policy.

For non-rate GraphQL errors, the parser additionally distinguishes an empty
or non-array errors field, malformed entries, and sixteen exact allowlisted
`type`/`extensions.type`/`extensions.code` tokens. Qt reports the recognized
token through fixed source-defined wording; this is a report of the code,
not an inferred cause or a promise about GitHub's error vocabulary. Every
entry and supplied marker must agree before a recognized code is selected.
Unknown or conflicting markers stay generic or mixed/unclassified. Remote
messages, paths, locations and unrecognized strings never enter presentation.
This refinement runs only after the existing non-rate refusal; rate-limit
precedence, schema acceptance and session authority are unchanged.

Those are adapter acceptance and retention bounds, not exact bounds on Qt's
private HTTP parser, socket buffers, or other in-flight allocations. Qt can
parse or retain bytes before exposing metadata through `QNetworkReply`, and it
may normalize or collapse duplicate headers, notably `Location`, before raw
header pairs and redirect attributes become visible. The adapter enforces the
metadata Qt exposes, not an exact count of on-wire Location fields. Qt also
does not provide a wipe API for the copies of an Authorization header held by
`QNetworkRequest`, `QNetworkAccessManager`, TLS, or socket internals. The
transport overwrites its explicit mutable Authorization buffer and the
credential owner overwrites its storage, but exact erasure of Qt-owned copies
is not claimed.

Bootstrap material is bound to one exact session context and may authorize
only one `GetViewerIdentity` operation; once claimed, it is destroyed after
that terminal path and cannot be replayed. An account credential must match
the exact profile, profile revision, host identity, canonical hostname,
account, and server contract, and each request must match its application
session and identity generation. Mismatches fail before network execution.

The target-native test seam exists only when `BUILD_TESTING=ON`. It keeps
the production request, TLS, redirect, cancellation, response, and
credential-binding path but substitutes an ordered loopback proxy list and
may add one disposable fixture CA to the system roots. The 15-case
TLS-over-CONNECT fixture covers success, ordered proxy fallback, accepted
same-authority REST redirect, rejected cross-authority and GraphQL redirects,
untrusted certificate, a response-body limit, one 8-KiB-plus-one
response-header-field limit, in-flight and pre-start cancellation, timeout,
HTTP 401/403/429 classification, and content-encoding rejection. There is no
separate 65-field or aggregate-header-only loopback scenario; those adapter
checks are source/contract evidence. Runtime facts separately report whether
Qt supports TLS, its active TLS backend category, the first raw system-proxy
candidate category, and raw candidate count; they do not contain credentials,
proxy addresses, CA contents, or remote data, and they do not prove every
candidate is reachable.

On the directly exercised macOS arm64 development host, the complete 15-case
fixture passed. Runtime observation reported Qt Network 6.11.1 with the
OpenSSL TLS backend, TLS supported, and one direct system-proxy candidate.
A separate `BUILD_TESTING=OFF` build/dependency audit found no test-only
factory symbols, retained the adapter's direct Security and CoreFoundation
dependencies, and observed the current ad-hoc linker signature. These are
development build and runtime facts, not package signing or distribution
certification.

Direct Linux evidence used an Ubuntu 26.04 x86_64 full-system QEMU guest with
Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2. The guest inputs were frozen to
SHA-256
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`
for `native_https_transport.cpp` and
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`
for the Python harness. The contract and 15-case fixture passed 2/2 in
0.12/11.29 seconds, and the two architecture gates passed 2/2 in
5.00/144.35 seconds. The final hostile suite covered all 92 mutations.
The uninstrumented focused suite passed repeatedly. Runtime
observation reported
`native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`.
The ELF directly requires Qt6Network and Qt6Core. A controlled trace of only
the trusted-success case under `strace` observed the Qt OpenSSL backend plugin,
`libssl.so.3`, and `libcrypto.so.3` being opened. A full fixture-suite
`strace` is deliberately not evidence because tracing perturbs cancellation
timing; behavioral evidence comes from the uninstrumented passes, while the
single-success trace is dependency evidence only. Tracked and on-disk scans
found no GitHub Actions workflow file, and the source/build configuration
selected no GitHub Actions integration.

Windows 10/11 x64 execution and dependency inspection remain BPR-0036 work;
that platform-specific evidence is nonblocking for the other implementation,
test, documentation, security-review, simplification, and later-RFC work. It
blocks only affected Windows claims and final RFC-BPR-0002 Auto-Verified
closeout.

### RFC-BPR-0003 native Git smart HTTPS capability

The epoch path uses a separate application capability and wire
implementation. It is not `github_wire_transport_port`, does not expose its
4-MiB JSON body contract, and does not make installed Git an authentication
transport. An opaque smart-HTTPS lease is issued only from an accepted native
HTTPS session and is bound to the exact application session, profile and
profile revision, host identity, canonical hostname and server contract,
authenticated account, repository identity, and GitHub identity generation.
CLI profiles receive no native smart-HTTPS Git-object lease; opted-in macOS
CLI reads use a separate supervised gh Git-object bridge, and publication
uses a separately confirmed mutation lease. An unavailable native session
receives no lease.

The lease exposes two semantic operations for the one accepted repository:

```text
discover_upload_pack
    GET /OWNER/REPOSITORY.git/info/refs?service=git-upload-pack

upload_pack
    POST /OWNER/REPOSITORY.git/git-upload-pack
```

There is no general URL, method, header, body, redirect, or credential API.
The repository path is compiled from the accepted server-canonical identity.
The discovery query is the original Git smart-HTTP protocol query, not a
redirect target. Automatic redirects are disabled and every redirect,
including same-authority and query-bearing redirects, fails closed. This is
stricter than, and does not relax, RFC-BPR-0002's query-free REST redirect
contract. Cookies, HTTP cache, content decoding, credential reuse outside the
bound lease, and response logging are disabled.

The capability reuses the selected native adapter's credential, operating
system trust store, bounded system-proxy policy, exact authority validation,
TLS policy, and redacted terminal categories without exposing any credential
or authorization header to its caller. Revocation on authority invalidation
or shutdown is terminal for new work, cancels in-flight work, and causes every
late completion to be rejected by its full context and generation. A normally
accepted submission produces exactly one terminal.

Smart-protocol control and fetch rounds use separate bounds and retention
shapes. Discovery and `ls-refs` are read in fixed chunks but retained only as
complete replies within a 64-KiB ceiling before parsing. Fetch reads at most
16 KiB at a time and forwards each chunk directly to the incremental
pkt-line/sideband consumer; it is not accumulated under either the control
reply or JSON response-body implementation. The production smart-operation
deadline is 15 minutes, and a 30-second no-progress idle deadline is reset by
admitted upload or download progress. Content type, service advertisement,
pkt-line framing, object format, advertised exact base/head identities, and
final pack/connectivity are independently validated. An app-owned minimal
protocol-v2 client emits only the exact validated base and fixed
pull-request-head `ls-refs` prefixes and the deduplicated accepted base/head
`want` set under a closed request grammar. Known optional server capabilities
may be advertised and ignored, but they are never requested, echoed, or
exercised. Any bounded `have` comes only from catalog-visible exact objects in
the same repository cache scope. It, not installed Git, owns protocol
negotiation and network rounds.

The smart origin is distinct from the API origin for GitHub.com:
`github.com` receives smart Git while `api.github.com` remains the REST and
GraphQL authority. GHES uses its exact validated enterprise hostname for
smart Git. Commit `d903290` implements the bounded application protocol
client, `edd762e` implements the native smart-HTTPS lease and wire path, and
`2801241` implements the authority-partitioned bare-cache/retained-ref
component that consumes their validated pack. Its direct cache contract
passed 1/1 in 162.58 seconds; the adjacent protocol/native/process/
architecture replay passed 7/7 in 92.95 seconds. These remain component
contracts. BPR-0044 and BPR-0045 supply durable operation state and
orchestration, and BPR-0047 supplies selection through the public
BetterPullReview composition.

The fixed `index-pack` import child receives only the bounded pack stream on
standard input and a validated app-owned bare-repository path. It receives no
protocol advertisement, URL, remote ref, proxy, trust, cookie, certificate,
Authorization, credential-helper, or provider authority and performs no
network operation. Separate fixed ref operations receive only generated
private BetterPullReview refs and exact expected object values.

Native credentials are consumed inside the credential-bearing transport.
They never enter process arguments, environment, logs, diagnostics,
persistence, core events, application projections, or a wrong-authority wire
request. The transport necessarily places Authorization in transient
`QNetworkRequest`/network-backend storage; those Qt-held copies cannot be
precisely wiped. Locked, unavailable, access-denied, corrupt, and
credential-deletion failure states are typed failures. They never cause
plaintext, settings, environment, or SQLite fallback storage.

The portable application contract keys every native credential by profile,
profile revision, host identity, canonical hostname, authenticated account,
and server contract. Every store, retrieve, delete, cancellation, and terminal
result also carries the request, application session, and identity generation.
Login text, repository content, redirects, capability revisions, and
caller-provided provider facts cannot form credential authority.

Credential material is a bounded nonempty byte owner rather than a string.
It is noncopyable, move-safe, exposes only a read-only byte span, overwrites
owned storage before release, and has no comparison, hashing, formatting,
streaming, or serialization surface. Store takes ownership immediately and
acts as an atomic upsert. Retrieve distinguishes material from absence;
delete distinguishes deletion from prior absence; every operation has typed
failure and cancellation alternatives. A winning cancellation suppresses
secret-bearing success and destroys transient material, and teardown does the
same for pending work.

The contract does not itself discover an account or prove a native vault.
Production enrollment must bind a validated profile to the authenticated
account observed through `/user`, then hand the move-only material directly
to the selected platform adapter. Its caller remains responsible for
overwriting any mutable input buffer used to construct the owned material.
Fake-vault tests prove portable replacement, isolation, result, and
cancellation semantics only; BPR-0022, BPR-0023, and BPR-0024 retain the
target-native lifecycle gates.

### macOS Keychain selection

BPR-0022's macOS adapter is an explicit file-based Keychain
implementation. It uses the modern `SecItem` operations with a generic
password item, but deliberately does not set
`kSecUseDataProtectionKeychain`, `kSecAttrSynchronizable`, an access group, or
an accessibility class. This is a scoped choice for the current ad hoc
linker-signed, unbundled development executable, which has no team identity,
bound `Info.plist`, or provisioning profile. It is not a claim that a future
signed application bundle should remain on the legacy implementation.

File-based Keychain searches normally cover every keychain in the process
search list even though additions go to the default keychain. The adapter
therefore resolves the exact default `SecKeychainRef`, uses it as
`kSecUseKeychain` for additions, and supplies a singleton
`kSecMatchSearchList` for retrieval, update, and deletion. A matching
service/account item in another keychain cannot be retrieved, replaced, or
deleted by this adapter.

The production service is fixed as
`org.betterpullreview.github-native-credential.file-keychain.v2`; native
tests use the separate
`org.betterpullreview.github-native-credential.file-keychain.integration-test.v2`
service and cannot touch production items. The account attribute is 574
lowercase hexadecimal characters: four fixed-width 64-bit fields for profile,
profile revision, host, and account; an explicit server-contract discriminator
(`01` for GitHub.com and `02` for GHES 3.18); one byte of hostname length; and
253 fixed hostname-byte slots encoded as canonical ASCII bytes or `00`
padding. The discriminators do not depend on C++ enum ordinals. The key
contains no login, API URL, repository, redirect, provider response, or
credential material.
Every query matches the generic-password class, exact service, exact account,
and exact default keychain and requests failure instead of authentication UI.

Store is an in-place upsert: add first, update only on duplicate, and retry add
once only if that update reports absence. It never deletes an old item before
replacement. Retrieve asks for one data result; delete reports deletion or
prior absence distinctly. Native status values are mapped to the portable
locked, unavailable, access-denied, corrupt, operation-failed,
allocation-failure, or invalid-request categories without retaining a raw
status number or operating-system error string.

The adapter queues exact portable operation contexts through an injected host
effect executor. Synchronous `SecItem` work never runs on the serialized
decision executor or a GUI lane. Cancellation can win only before native
execution starts; once a synchronous `SecItem` operation begins, its terminal
result wins. A terminal produced after native execution arrives on the effect
lane. Immediate rejection and explicit or destructor cancellation may complete
synchronously on the originating lane. Every callback runs outside adapter
locks, and a consuming application host must marshal every typed completion
onto the serialized decision lane. Destruction atomically claims and cancels
all still-queued work and destroys any owned store or retrieved material.

There is no runtime probe, migration, or fallback between the file-based and
data-protection Keychains. A future data-protection selection requires a
signed app-like bundle, explicit entitlement/access-group policy, migration
and rollback behavior, and target-native package evidence under
RFC-BPR-0014. Until that work is complete, the two stores must never be
silently mixed.

### Windows Credential Manager selection

BPR-0023's Windows adapter selects the wide-character Win32 Credential
Manager API and one `CRED_TYPE_GENERIC` credential with
`CRED_PERSIST_LOCAL_MACHINE`. The production target prefix is fixed as
`org.betterpullreview.github-native-credential.windows-credential-manager.v2/`;
native tests use the disjoint
`org.betterpullreview.github-native-credential.windows-credential-manager.integration-test.v2/`
prefix. The same 574-character fixed-width lowercase hexadecimal authority
encoding used by the macOS adapter follows the prefix. Production and test
targets are therefore 650 and 667 UTF-16 code units respectively.

The target prefix is an application namespace, not an operating-system access
control list. Generic credentials are available to another process running as
the same user if that process knows the target. `CRED_PERSIST_LOCAL_MACHINE`
means that the credential remains available to later logon sessions of that
same user on the same computer. It does not make the credential machine-wide,
hardware-backed, application-isolated, or roaming.

The portable 2,560-byte limit exactly matches the documented Windows 10/11
generic-credential bound of `5 * 512` bytes. The adapter names that supported
platform contract directly because some MinGW headers retain the obsolete
512-byte macro. It does not reduce the portable contract according to the
compiler header. Returned native blobs are accepted and overwritten before
`CredFree` only when both their pointer and their extent satisfy the stable
bound; malformed or oversized native results fail as corrupt without
dereferencing an untrusted extent.

Store constructs one zero-initialized `CREDENTIALW` and makes one
`CredWriteW(..., 0)` create-or-replace call. It never deletes the old
credential before replacement. Retrieve and delete use exact-target
`CredReadW` and `CredDeleteW` calls, and native ownership is released only
through `CredFree`. `GetLastError` is captured immediately after a failed
call. Documented absence, unavailable-session, denial, corrupt-data,
allocation, and invalid-request statuses map to portable outcomes; all
unselected statuses become `operation_failed`. Credential Manager documents
no read/write/delete status that honestly means a locked vault, so this
adapter does not invent a native `locked` mapping.

Synchronous Credential Manager calls use the host effect executor with the
same queued-work, cancellation, callback, and atomic teardown rules as the
macOS adapter. The source has no Credential UI, enumeration, Credential
Locker, DPAPI, registry, file, settings, SQLite, environment, process,
network, logging, or alternate-vault path.

The adapter, fake-backed contract suite, native fixture, hostile source
boundaries, and x64 PE cross-compile are prepared. None is a substitute for
running the checked-in `windows-dev` build and disposable lifecycle on a
native Windows x64 environment. BPR-0023 closes the implementation slice;
BPR-0036 keeps the native claim open until that target-native build,
execution, import inspection, and exact cleanup pass.

### Linux Secret Service selection

BPR-0024's Linux adapter is selected only when `CMAKE_SYSTEM_NAME` is
`Linux`. That target requires and links the stable
`libsecret-1>=0.20.0` interface; the Qt-free target graph does not discover
or link libsecret.

The production schema is fixed as
`org.betterpullreview.github-native-credential.secret-service.v2`; native
tests use the disjoint
`org.betterpullreview.github-native-credential.secret-service.integration-test.v2`
schema. Each schema declares only one string attribute, `authority`, whose
value is the same 574-character fixed-width lowercase hexadecimal encoding
used by the other native adapters. A matching item must have exactly that
authority and the `xdg:schema` value added by libsecret. Multiple matches,
additional attributes, a schema mismatch, or an invalid value is corrupt.

The v1 and v2 namespaces are deliberately disjoint, and the adapters do not
read, enumerate, migrate, or fall back to v1. The pre-release v1 production
vaults were never composed into the public executable. Every directly
executed v1 integration fixture completed exact cleanup; the Windows v1
fixture remained preparatory and created no target-native item. This
intentional format break prevents an old hostname-free key from aliasing a
hostname-bound authority.

The adapter opens a derived `SecretService` with the literal
`org.freedesktop.secrets` argument and `SECRET_SERVICE_NONE`, then checks
`g_dbus_proxy_get_name()` for that exact value before establishing a Secret
Service session or resolving a collection or item. Only a canonical proxy may
continue through `secret_service_ensure_session_sync()`. This second check is
mandatory because the selected libsecret can be redirected at the dependency
layer. A redirected proxy fails as typed `unavailable`; production code never
reads, copies, changes, or removes a service-routing environment variable.

Only the default collection alias is admitted. The adapter does not unlock
the collection, create a collection, search an alternate vault, or silently
start an interactive authentication flow. Its derived `SecretService`
overrides synchronous, asynchronous, and finish prompt vfuncs. If libsecret
hands it a prompt, the adapter issues exactly one `Dismiss` D-Bus call with
no auto-start and a positive one-second bound, then returns a locked failure.
It never calls `Prompt`.

Store creates a binary `SecretValue` with
`application/octet-stream` and uses one
`SECRET_ITEM_CREATE_REPLACE` operation in the default collection. Retrieve
searches all exact-schema matches without loading or unlocking them, requires
zero or one exact item, checks its lock state, and then loads that item
explicitly. It accepts only `application/octet-stream` or exact `text/plain`.
The latter is narrowly admitted because gnome-keyring 50 through libsecret
0.21.7 was directly observed changing the content-type metadata to
`text/plain` while retaining arbitrary opaque bytes, including a complete
2,560-byte replacement. Delete acts on the same one exact item.

Every fixed string in that path — both schema identifiers, the `authority`
attribute name, the two item labels, both accepted content types, and the
`org.freedesktop.secrets` bus name — is a `std::string_view` handed to a
libsecret entry point that reads to a terminator it is not given a length
for. None of them reaches such a call through `data()`, including the GLib
hash-table key and lookups whose tables libsecret then consumes. They go
through `c_str`, a
`consteval` function that inspects the byte at `size()`, so a view that is not
null-terminated — a `substr`, a `{pointer, length}` pair, a value known only
at run time — fails to compile at the call site instead of reading past its
end inside a call that carries a credential. The item label is selected by a
function that is total over the two schema scopes, so there is no empty view
whose `data()` is a null pointer to hand to `secret_item_create_sync`.
`cmake/VerifyArchitectureBoundaries.cmake` pins the guard itself as well as
the call sites, so replacing it with an unchecked `data()` fails
`architecture_boundaries`.

Secret Service, D-Bus, and I/O statuses map conservatively to the portable
locked, unavailable, access-denied, corrupt, invalid-request, allocation, or
operation-failed categories. No raw status text crosses inward. Synchronous
libsecret work is serialized by the backend and dispatched through the
injected host effect executor. Queuing, cancellation, completion, callback,
and teardown follow the same portable ownership rules as the other native
adapters. There is no prompt display, collection creation, unlock, alternate
vault, plaintext, settings, SQLite, environment, file, process, network,
logging, or UI fallback.

Direct Linux vault evidence passed in an Ubuntu 26.04 x86_64 full-system QEMU
VM with an emulated CPU, Linux 7.0, Qt 6.10.2, libsecret 0.21.7, and
gnome-keyring 50. A dedicated real user ran the fake contract and the
resource-locked setup/lifecycle/cleanup fixture under Xvfb plus a desktop
D-Bus Secret Service session. The four focused CTest cases passed, including
73-byte storage, 2,560-byte replacement, retrieval, deletion, repeated
absence, and canary probes over arguments, environment, fixed output,
executable, source, and isolated scratch. A second lifecycle round-tripped
opaque values containing NUL, high-bit, and invalid UTF-8 bytes at the same
73-byte and 2,560-byte sizes. Checked-in setup and cleanup proved the exact
fixture authority absent; a separate controlled D-Bus audit proved the entire
integration schema absent. Other controlled runs proved hostile
`SECRET_SERVICE_BUS_NAME` rejection, a truly locked default collection, and
no-session-bus unavailability. A live hostile D-Bus owner observed only
proxy-property inspection and no `OpenSession`, collection, or
credential-item method before rejection. Fake-backed effects and direct
native-status classification separately cover access-denied and failed-delete
terminals without claiming those as desktop-policy fixtures.

That full-system VM result proves the selected Linux vault behavior against a
real Linux kernel and user-space API stack, but it is not bare-metal evidence.
The vault fixture itself does not verify HTTPS. BPR-0026 separately verified
the Linux internal native HTTPS wire path in the same class of full-system
Ubuntu 26.04 QEMU environment: all 15 TLS/proxy/redirect/limit/cancellation
scenarios passed, runtime selected the OpenSSL backend and one direct proxy
candidate, and ELF plus a controlled single-success trace selected Qt6Network,
Qt6Core, the Qt OpenSSL backend plugin, `libssl.so.3`, and `libcrypto.so.3`.
That separate BPR-0026 evidence is not by itself composed-provider evidence.
BPR-0029 now supplies the recorded public executable trace on macOS and in an
Ubuntu 26.04 x86_64 full-system QEMU guest. Its substituted fixture transport
does not replace the direct internal native-wire evidence. The current Linux
v2 run passed the
Secret Service setup/lifecycle/cleanup cases 3/3, independent exact cleanup
and hostile/unavailable/locked cases, both exact-host rendered public
fixtures, fresh 81/81 tests-on and 36/36 tests-off Release build targets,
fixture-seam absence, and dependency/runtime/ELF audits. Windows target-native
evidence remains in BPR-0036. Mock transports, cross-compilation, and
vault-only evidence cannot substitute for a retained platform claim.

### RFC-BPR-0016 device authorization

A second credential-acquisition path over this same native HTTPS transport.
It adds no transport rule and relaxes none, and manual token entry remains
available on every profile, including one the device grant already enrolled.

#### The registered identity and its permissions

The shipped artifact carries one public GitHub App client identifier, recorded
as the distributable production identity by [[BPR-0318]] and admitted by
`scripts/verify_rfc0016_production_identity.py`. No client secret exists in
the application; the registration is constrained to a public client, which is
what makes that possible. A second, private, owner-only identity exists for
[[BPR-0276]]'s recorded capture and must never be shipped as the production
one — the release scan requires the recorded distributable client to be
present and the verification client to be absent.

The requested permission set is the minimum the shipped operation catalog
justifies, and it is compiled in rather than operator-editable, because an
editable scope set reintroduces the decision this path exists to remove:

| Authority | Permission | Why the catalog needs it |
|---|---|---|
| Repository metadata | Metadata, read-only | Repository identity and metadata `GET` |
| Git object access | Contents, read-only | Native smart-HTTP discovery and `git-upload-pack` |
| Check state | Checks, read-only | Commit check-runs `GET` |
| Pull-request observation and publication | Pull requests, read and write | PR, files, commits, reviews, comments, and GraphQL reads, plus the single review-comment `POST` |

No Issues, Actions, Workflows, Commit statuses, Administration, organization,
enterprise, or account permission and no OAuth scope is justified by the
current catalog. A new route that needs a wider real token reopens the
attended permission decision rather than widening this table.

The device flow yields a GitHub App user access token, whose effective
repository access is the intersection of the user's access and the app's
installation selection and permissions, as described in
[GitHub's user-token documentation](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-a-user-access-token-for-a-github-app).
Successful `/user` authentication establishes the account but not access to
every private repository that account can open on GitHub.com. An exact
`GetRepository` permission refusal therefore cannot be resolved merely by
matching the browser and product account names. The manual-token route
remains first-class under the product's no-required-admin-install contract;
the app must keep a device-grant limitation explicit without treating it as
the user's lack of website access.
The bounded diagnostic may retain the numeric HTTP status and a fixed source
category (`http_response`, `network_reply`, or `native_transport`) when a
provider read fails. These facts distinguish a GitHub repository refusal
from a local transport error without persisting a response body, header,
credential or repository content; they do not identify the exact installation
policy behind an HTTP `404`.

#### Exact endpoints and route admission

Two routes on `github.com`, both `POST` with a form body:

| Endpoint | Route |
|---|---|
| Device code | `/login/device/code` |
| Token exchange and refresh | `/login/oauth/access_token` |

URL construction admits the `github_com_2022_11_28` server contract, an empty
query, and exactly those two paths; it re-encodes the address and requires a
byte-identical round trip, no port, no userinfo, and no fragment. Anything
else yields no request at all rather than a rejected one. Responses are
bounded at 64 KiB and parsed into one typed completion, and a
`verification_uri` outside the expected authority is refused at the wire
before it can reach a host ([[BPR-0278]],
`github_device_authorization_contract`,
`native_https_device_authorization_contract`).

The device contract uses Glaze to validate the complete response and index its
flat envelope as non-owning raw JSON views. Required field inventories are
checked before strings and durations are decoded into typed grant/error values;
views never leave the synchronous parser or outlive the caller's response.
Durations use exact positive uint32 parsing with the existing int32 maximum,
so fractional JSON spellings cannot round into accepted lifetimes. The prior
device-envelope last-member-wins policy, including escaped equivalent keys,
remains distinct from the mutation contract's duplicate-key rejection. Unknown
values still require valid JSON and Unicode, and failures remain fixed enums.
Form-encoded outbound bytes, URI authority checks and credential ownership are
unchanged by the Glaze migration.

#### Polling discipline

The host honors the server's interval within a floor of 5 s and a ceiling of
300 s, applies at least the RFC 8628 five-second increase when the service
answers `slow_down` and the larger interval the recorded GitHub contract
names when it names one, and stops at a total bound of 900 s or 256 polls
whichever comes first. Grant validity is capped at 3,600 s. Three consecutive
transport failures inside one poll window end the flow; a refusal of the
server's identity or of the response shape ends it immediately and is not
counted against that budget. Polling is cancellable from the UI and at
shutdown, and cancellation retains no device code ([[BPR-0279]],
`application_device_authorization_host`).

#### Refresh

The selected registration is a GitHub App, so access and refresh material have
bounded lifetimes. Renewal is armed 300 seconds before access expiry, or at the
midpoint of a lifetime shorter than twice that lead. Active deadlines are
monotonic. Transient transport failures retry at most the configured failure
bound, with waits no longer than five seconds and never beyond the remaining
access lifetime. Terminal failure reports `reenrollment_required`; it cannot
retry forever ([[BPR-0279]], [[BPR-0541]],
`application_device_authorization_refresh`).

On enrollment, the GitHub host records `device_grant` provenance and the device
host binds the accepted credential authority before storing the refresh slot.
A pasted token remains `manual` and cannot renew from an orphan refresh slot
([[BPR-0540]]). A settled device pair supplies exact profile/revision/host/
hostname/account/server authority and issuance/access-expiry/refresh-expiry
facts to `device_binding_persistence`. Its bounded serialized effect lane
writes `state/v1/device-bindings.sqlite3` through the lazy SQLite adapter. This
store contains no token bytes. Schema, record and capacity validation refuse
unknown or malformed data.

Only a verdict on the file latches the store. The store reads the admitted file
inside one transaction, so a verdict comes from one snapshot. A file judged
corrupt refuses every later call for the life of the store. Three failures are
not verdicts, and none of them latches: cancellation, a lock held by another
connection, and a failed configuration or migration. Each closes the
connection and reports `cancelled` or `unavailable`, and the next call admits,
opens and judges the file again ([[BPR-0884]]).

A read that SQLite fails is judged by SQLite's result, at every read, not only
the first ([[BPR-1111]]). SQLite's own finding that the file is corrupt or not
a database is a verdict. So are the read scope's step ceiling and length
limit, which only hostile content reaches. Every other result is `unavailable`:
I/O, memory, the WAL-index locking protocol, recovery and contention.
`PRAGMA quick_check` reports a page it could not read as one of its findings
rather than failing, so a finding that names such a read is judged by its
result code as well. Every read the judgement makes is inside its snapshot. The
row read that followed the snapshot's end is gone, and a migration reads back
what it wrote inside its own write transaction.

Every metadata request the lane admits has exactly one terminal. A request
whose effect cannot be posted reports `unavailable`. If the decision lane
refuses that terminal too, a new request is withdrawn and the call throws. A
queued request instead gets its terminal in the lane turn that finished the
request before it.

The GitHub host counts each binding request, and some set a pending flag,
before it calls the lane. So it treats a withdrawal as the lane's
`unavailable` terminal, never as a success, and does not pass the throw on
([[BPR-1048]]). This holds for the settled-binding store and the tombstone
behind Sign out, replacement and cleanup. The rotation barrier calls no lane
since [[BPR-1116]]. Inside a host turn the failure runs
when the turn ends, where the lane's own terminal would have found it. That
matters for a replacement: its session is recorded after the invalidation
that asked for the tombstone, so a withdrawn tombstone fails the replacement
and leaves both vault slots alone, as a refused one does. Outside a turn the
failure runs at once. The count and the flag are undone, so later restores are
admitted and the host can become quiescent. The startup load already failed its restore
as `unavailable` at the call.

A turn that ends in an exception fails the requests it queued before the
exception leaves it ([[BPR-1112]]). No later turn is needed, and no second
`shutdown()`: after `Closing` almost nothing starts one. A throwing terminal
goes no further on either path. `start_delete` builds its terminal handler
before it changes any state, so an allocation that fails there reaches the
caller with nothing changed, counted or pending.

Startup loads only the compiled native GitHub.com device profile after both
hosts exist. `resume_profile` validates its current profile contract, retrieves
the exact vault item and checks authenticated `/user` identity before accepting
the session. Missing or mismatched access material cannot restore a device
session, and issuance/expiry facts that are backward or expired cannot extend
validity. The refresh-slot probe then decides whether the session renews
itself. A restored session whose refresh slot is missing stays signed in on
its access token, is not renewed, and its card says it does not renew itself
([[BPR-1116]]). Once that token's lifetime has gone, the next launch's renewal
meets the missing material as a refusal and keeps the grant. `resume_refresh` schedules from
the remaining lifetime instead of exchanging a token immediately.

A launch that cannot restore the remembered grant keeps it ([[BPR-0644]]).
This is the owner's rule of 2026-09-24: a remembered grant is removed only
when the person removes it (Sign out) or a new sign-in replaces it. A locked,
refused, missing or unreadable access slot, a network failure, and a `/user`
that refuses the token each fail the restored session visibly with their own
reason and erase nothing. The row stays live and the next launch tries again.
An access-expired binding renews through the device host. That launch renewal
writes no tombstone before its first attempt, so a renewal that is offline,
refused, or quit while it retries leaves the row live. A refresh-expired
binding is held and failed at once, asking for signing in again, without
spending anything. The failure's diagnostic carries `GrantKept`, and the
server card offers Sign out beside Begin with a sentence saying how the grant
ends. Material that signs in as an account the binding never named is kept
too, by the owner's decision of 2026-09-24. There are two paths to it: a
rotated grant whose `/user` differs, which the host's bootstrap check catches,
and a restored access slot that answers for another account, which the
provider refuses as an `authentication` failure carrying `AccountMismatch`.
A renewal's rotated access token for the other account is never stored. The failure's diagnostic
carries `AccountMismatch` beside `GrantKept`, and the Accounts page shows it as
the card's error: the remembered sign-in signs in to a different account than
the one it was saved for, with Sign in to replace it and Sign out to forget
it. Only the profile-mismatch erasures, where the compiled profile changed
under a binding, still remove material on their own.

The same rule holds while the application runs ([[BPR-1116]]). An accepted
session's renewal that gives up, whether out of network attempts, unable to
reach the credential store, or refused, keeps the grant. The host resumes the
profile from its own remembered binding: the invalidation that goes with a
resume deletes nothing. The resumed session then fails at once with the
renewal's failure, so it carries `GrantKept` and the card offers Sign out
beside Sign in. The device host's renewal chain is released, as a launch
renewal's is. A grant whose binding was never stored is not claimed as kept,
and the orderly quit still removes it, as it removes any grant kept for one
session only.

Vault slots and SQLite do not form one transaction. Before replacement the
existing binding is durably tombstoned. A renewal's barrier is the exception,
at launch ([[BPR-0644]]) and in process ([[BPR-1116]]). It writes nothing, and
the row stays live until the rotated grant's binding replaces it, so a crash
after the spend leaves the next launch to spend whatever the refresh slot
holds. An in-process rotation that succeeds enrolls its rotated access token,
and that enrollment is a replacement: it tombstones the row and deletes the
old access slot, keeping the refresh slot it just filled. A process that ends
between that tombstone and the rotated grant's accepted binding leaves the row
tombstoned. The window runs from that tombstone through the rotated session's
vault store, `/user` and capability checks to its binding store. A new resumable record is
written only after both vault slots and the accepted session settle. A newly
issued refresh token cannot inherit a missing expiry from its predecessor;
only an explicitly retained refresh slot preserves its existing expiry. Stale
completions cannot make an invalidated binding resumable.

The production `retain_accepted_device_grants` shutdown policy retains only a
durably settled device pair. Manual sessions and incomplete grants retain exact
cleanup. Sign out and manual replacement invalidate the binding, then issue
access deletion followed by refresh deletion under the same authority. Each
delete starts from its predecessor's terminal because native vault operations
share an exact context ([[BPR-0545]]). Same-authority rotation preserves the
pair it just renewed; a changed or failed replacement releases the prior pair
([[BPR-0547]], [[BPR-0549]]). Metadata work, refusals, callbacks and cleanup
remain counted through quiescence; failures produce diagnostics rather than a
successful cleanup claim.

`application_device_binding_restart` reconstructs application hosts over a
reopened database, observes `REFRESH_RESUMES:1`, defers one rotation, clicks the
ordinary Sign out control, and proves a third startup cannot resume. It also
checks manual replacement and non-retention. Only a test-local in-memory vault
survives those application lifetimes. `sqlite_device_binding_storage` separately
uses child processes on macOS to prove metadata reopening; neither test claims
native-vault continuity across OS processes. The older two-rotation and orphan
courses remain regression evidence under their explicit session-only fixture
policy, including their expected final refresh-slot erasure.

#### The vault join point

Success enters the existing profile-enrollment path at exactly the point the
manual prompt enters it today. The access token becomes
`github_credential_material` and is enrolled through `enroll_profile`; from
that moment it is indistinguishable to the rest of the system from a token the
operator pasted, and vault storage, identity observation, and capability
probing are unchanged. There is no second store, no settings copy, and no
SQLite copy ([[BPR-0281]], [[BPR-0282]],
`application_device_authorization_registered_course`).

#### GHES and unregistered profiles

The device path is composed against the first configured profile, and the only
transport that can be built for it is the GitHub.com one: route admission
refuses every other server contract. A build whose first profile is GitHub
Enterprise Server therefore composes the refusing transport, the capability
stays unregistered, and selecting device sign-in answers `device_flow_unavailable`
with zero requests — structurally, before a request is constructed, rather than
as a count that happens to be zero. Adding a GHES profile beside GitHub.com
does not change the GitHub.com device path, and since BPR-0535 it cannot
borrow it either: the composed capability names the profile it was built
for, and the host refuses a begin for any other profile before it touches the
transport, the vault, or enrollment — on the mounted panel's route and on the
public-preview sign-in route alike. The independent review of the closeout
head had shown that without that check a reviewer who picked a GHES profile
and pressed "Get a code" started the GitHub.com flow with the GitHub.com
client and, on authorization, enrolled a GitHub.com token under the GHES
profile; `application_device_authorization_ghes_refusal` now drives that
exact press and proves the refusal with zero requests.

An enterprise instance would need an application registered on that instance,
which is an administration action RFC-BPR-0016 does not own. Until one exists,
GHES keeps the manual token path. The ledger's end-to-end GHES claim row is
still open: no current selector drives the device probe against a GHES front
profile, and closing it is [[BPR-0285]]'s.

## Read-only enforcement

The RFC-BPR-0002 composition exposes only narrow provider-read ports. Its
request inventory and outbound admission checks require:

- recorded methods must parse as exact uppercase `GET` or `POST`, with no
  fallback for any other value;
- REST catalog operations must use typed `GET`;
- GraphQL `POST` bodies and the capability-probe document share one bounded
  full-document scanner and must contain exactly one explicit `query` plus
  bounded fragments;
- every top-level `mutation` or `subscription`, including one after a valid
  query, is rejected, as are multiple operations, malformed scanned
  structure or token framing, and byte, nesting, or definition-limit
  exhaustion;
- comments, strings, block strings, field names, operation names, endpoint
  names, and fixture labels are never treated as operation-type evidence.

The scanner is a bounded structural classifier, not a complete GraphQL grammar
parser. The outbound read contract also requires exact equality with the
canonical compiled request body before an operation can execute. Every
legacy/current GraphQL shape has a direct case where the scanner approves an
added read selection but canonical equality rejects the changed body.

GraphQL read requests use typed Glaze query/variable records. Member order,
integer formatting and omitted cursor/thread fields preserve canonical request
bytes for legacy, initial and continuation queries. Outbound validation uses
Glaze after the structural byte/depth/duplicate preflight, validates the entire
JSON envelope, then applies the operation scanner before variable-type checks.
Raw variable views stay inside that synchronous validation call. Exact canonical
body equality remains the final admission requirement.

Strict fixture-response validation uses Glaze indexes of borrowed JSON values
so field inventories distinguish missing members from explicit null and retain
the original numeric spelling. Bounded structural preflight and whole-document
JSON validation run before field checks. Scalar roots, non-finite number tokens,
malformed Unicode and trailing documents remain malformed responses. Identities
decode directly to uint64, while PR numbers and lines retain their narrower
bounds; decoded strings determine path equality. The pre-existing incomplete-page
classification precedes shape checks and never supplies an identity.

Recorded read normalization extracts Glaze wire records after strict fixture
validation has established field inventories, nullable relationships and page
completeness. Identities and counts decode directly to integer fields; lines
and nullable actors remain explicit optionals. The normalizer then checks
repository/viewer/head identity, groups REST replies, and validates the complete
application snapshot before returning it. No parser or wire record crosses that
boundary. Missing REST comment coordinates retain the invalid-zero refusal;
explicit null coordinates remain absent. Comment position is used only for its
nullness, preserving the distinction between a missing position and explicit
null without interpreting another provider value as a coordinate.

Production paging also uses Glaze with exact integer decoding. Each parsed page
owns an immutable response body; indexed JSON children share that ownership,
including copied GraphQL connection nodes. No raw view borrows a completed
transport operation. Structural bounds and whole-document validation precede
shape and scalar checks, including unknown response values. Required members,
explicit nulls, exact resource-scoped rate observations, REST authority and
GraphQL cursor/count checks retain their existing admission rules. Required
pull-request collections exhaust before the complete typed snapshot validates
and returns; a malformed continuation never exposes an earlier partial
snapshot. Repository-wide open-PR search is ancillary to opening one exact PR.
When a well-formed search page declares more results than the configured search
limit, its received bounded rows still pass the existing semantic and
relationship checks, and the page must declare a continuation consistent with
its total. The accepted snapshot then marks search as `result_limit` and keeps
zero search rows; it never presents a truncated search as complete. Malformed
search pages, inconsistent pagination and other read failures still refuse the
whole snapshot.

Mutation capability probing may observe schema support without executing the
mutation. Executable GitHub mutations remain RFC-BPR-0008.

## Cache and invalidation rules

Every identity-sensitive cache key includes:

```text
profile revision
+ host identity
+ authenticated account identity
+ repository identity or unresolved locator
+ pull-request identity/number
+ capability or provider-contract revision
```

Selecting a different profile or resolving a different authenticated account:

- increments the core identity generation;
- disables old-identity commands immediately;
- cancels or suppresses pending provider work;
- discards staged results;
- removes active metadata and conversation projections;
- invalidates capability, normalized response, rate-limit, and retry state for
  the old active session;
- prevents reuse solely because repository text or PR number matches.

Shared records are refreshed from GitHub. A cache hit can satisfy a bounded
observation only when its entire authority key and freshness policy match.

Milestone one does not share a physical object database across accounts or
profile revisions. The exact profile/revision, host, authenticated account,
canonical repository, and object-format partition receives a
catalog-generated random 256-bit `repository_cache_scope_id`. Each pull
request within it separately receives a random 256-bit
`pull_request_authority_scope_id`; that scope qualifies retained refs, epoch
ordinals, aliases, catalog queries, paging, selection, and deletion. Both are
persistent lowercase 64-hex opaque values, nonreversible, never supplied by a
caller, and collision-checked before use.

Every use requires a current accepted authenticated-account authority. There
is no offline or restart visibility merely because a bare repository or
catalog row exists, and a caller cannot enumerate scopes or fetch an object
by arbitrary OID. Profile/account invalidation revokes active visibility. A
catalog/ref authority mismatch, object-format mismatch, or stale generation
fails closed. Provider clone URLs and raw `HeadRef` text are never cache,
path, ref, or transport authority.

The window's accepted PR snapshot is held in memory. A failed same-target
refresh retains that observation and its displayed source; the workbench says
that previously loaded review data has unknown freshness. A successful new
observation clears this notice. Recent titles/destinations and exact reading
positions persist separately from the snapshot, Git objects and epoch catalog.
They do not make PR metadata or conversations reopenable offline after restart.
The initial [BPR-0969 inventory](../verification/BPR-0969-offline-inventory-2026-09-25.md)
found profile-private Recent records filtered by host/access mode without an
account key.
The subsequent [Recent account-isolation slice](../verification/BPR-0969-recent-identity-2026-09-25.md)
binds persisted destination/title/reading to the accepted account. Projection
and Open/Resume check that account against the current core identity, including
the sign-out interval before the old accepted model is released. Legacy
unattributed entries cannot acquire a current account merely by loading. This
changes restoration metadata admission, not Git/cache or epoch authority.

BPR-0969 also retains a bounded historical overview in profile-private
`state/v1/review-overviews.sqlite3`. Only a validated provider observation
accepted by the core can schedule a save. A serial host effect performs the
SQLite work; the decision lane counts and drains its completion. Each window
permits one outstanding save, so another accepted read during that save can
leave the previous persisted observation. The cache is optional and never
promises the latest server state.

The exact key includes the complete host-profile authority fingerprint
(including profile revision), host, profile, accepted numeric account, repository
locator and PR number. The record preserves numeric repository/PR IDs and exact
base/head commit IDs. A load is eligible only after a current authenticated read
exhausts its transport retries without an accepted in-memory snapshot. The core
rechecks current session/generation/request/account, exact key and any expected
numeric repository/PR identity before publishing historical metadata. Cancellation,
identity retirement, permission errors, corrupt data and foreign keys cannot
publish it. A locator alone cannot prove current repository ownership or current
head while disconnected; this view grants neither current access nor revision
authority.

The overview says “Cached overview · freshness unknown” and exposes exact retained
base/head IDs. Its optional historical changed-file catalogue preserves current
and previous paths, provider change kinds and additions/deletions from that same
accepted observation. These are GitHub's reported changes, not a recomputed
base-tip/head comparison. The catalogue and overview share one payload, integrity
digest and atomic replacement; no separate key can attach another revision's
files. The core bounds it to 1,024 unique paths and a conservative 128 KiB encoded
budget, with each path at most 4,096 bytes. An excessive catalogue is omitted in
full while the new overview replaces the old one. No truncation or old catalogue
is presented as the new observation.

The version-two payload extension distinguishes unavailable from observed empty.
Original payloads remain readable with the catalogue unavailable; new empty
catalogues explicitly say zero files observed. The SQL table/schema version and
key encoding stay unchanged. An older reader refuses the new trailing payload;
it does not reinterpret it. Unknown extensions and corrupt records fail closed.
Qt presents historical rows in the Overview table and hides unavailable
conversation/progress columns. Source activation uses the separate historical
reader described below; current review state remains unavailable. It creates no accepted snapshot, epoch
handoff, markdown-link or mutation authority. Reopening online obtains a new
server observation, clears cached revision descriptions and restores current
columns and file activation. Ten records share the profile; each payload is
bounded to 704 KiB, the body to 256 KiB, and the database to 2048 pages. Version,
schema, record bounds and SHA-256 integrity are checked before content is returned.
Profile purge includes this store and its SQLite sidecars. This is historical
metadata after current-process authentication; startup with `/user` unreachable
still exposes no authenticated cache. Full offline account admission remains unsupported by the current policy in
[BPR-0969](../issues/60-done/BPR-0969-verify-cached-review-offline-restart-freshness.md).

The subsequent thread-history continuation adds a version-three payload
extension to this same atomic observation. It preserves complete review-thread
collections with thread/comment IDs, ordered reply-to IDs, author login, body,
path, original/current line observations, side, file-level subject, resolution,
outdated and deleted facts. These remain observations made alongside the retained
base/head, not anchors authorized for a current comparison. There is no independent
thread cache key or lookup that can combine different overview revisions.

The optional collection distinguishes unavailable (including older payloads or
an excessive collection) from an observed empty collection. Bounds are 128 threads,
512 comments in total, 64 KiB per body and a conservative 128 KiB aggregate encoded
budget. Duplicate IDs, foreign/forward reply links, invalid anchors, unknown
extensions and corrupt bytes fail closed. The host omits an excessive collection
whole while replacing the overview, never preserving old threads under new
base/head IDs. Existing session/cancellation, key and numeric-identity admission
checks cover the entire record unchanged.

Overview shows a plain-text read-only historical reader with explicit unknown
freshness, prior resolution wording and inert anchor observations. Links and
anchors grant no navigation; the live conversation model, reply/resolve actions,
receipt state, epoch handoff and mutation lease remain unavailable. Missing and
empty collections have different headings. A newly accepted server read removes
all historical text and labels. The
[thread-history evidence](../verification/BPR-0969-thread-history-2026-09-25.md)
records the bounded Mac fixture claim and its limits.

The integrated BPR-0969 outcome adds version-four optional general-comment and
review-summary collections to the same atomic payload. Each collection has at
most 256 unique IDs and a conservative 64 KiB total encoded budget; the complete
collection is omitted when over budget. Reviews retain their observed state and
optional review commit. Old payloads have unavailable collections, whereas a
completed accepted empty endpoint remains observed empty. Text is literal and
review states use historical wording. No summary grants current approval or
permission. The store still holds at most ten records under its 2048-page ceiling.

A cached file gesture enters `cached_review_source_port`, independently of the
live selected-diff port. The window host retains the admitted record and original
failed-open context. Core policy rechecks the current authenticated identity,
account, repository destination, request generation and catalogue membership at
ingress and completion. Opening another review, cancelling, retiring the identity
or shutting down stops the read; stale completions drain without publication.
The source port cannot manufacture an accepted snapshot or epoch handoff.

The epoch host opens the exact existing catalogue with `ExistingOnly`, which
refuses absent, retired, rehydration-required or inactive scopes and cannot
allocate/reactivate them. Its snapshot must match the catalogue binding and the
latest retained epoch's exact base/head and available anchors. The shared profile
Git actor serializes lookups with existing epoch work; a historical read never
replaces a sibling's cache session. Exact base uses the retained previous path
when present; exact head uses the current path. Blob materialization is bounded
to 256 KiB per endpoint. Missing paths differ from empty blobs; missing objects,
binary or oversized data and invalid UTF-8 receive explicit unavailable/unsupported
presentation. No network fetch, catalogue recovery, revision increment, epoch
ingestion or mutation authority results from this path.

The source reader expands within Overview, names the exact base/head and unknown
freshness, and returns to the cached overview without entering the live source
workbench. It reads the base and head themselves, not a recomputed merge-base
comparison. Source for an older cached pair that differs from the latest retained
epoch is conservatively unavailable; there is no fallback to a nearby pair.
Reconnection adopts server truth and clears all historical text. Fully disconnected
startup still requires an independently established account identity; credentials,
Recent rows and cached data cannot supply it under current policy. Extending that
policy would require a separately authorized product decision. The
[integrated evidence](../verification/BPR-0969-cached-review-2026-09-25.md) records
actual restart, missing-pack, mismatched-pair, account-switch and owning-layer
negative controls, with no native/human or Windows/Linux claim.

The production application host constructs and owns an initially empty
capability cache. Composition cannot inject a populated cache, and a caller's
syntactically valid capability facts are not observations. Only the exact
authenticated provider probe can stage an entry; the core's accepted-session
delta precedes cache commit.

Native credential resume accepts only an opaque profile identity. The host-owned
catalog selects the exact profile/revision/host/hostname/account/server authority
established by strict credential-authenticated `/user`. BPR-0541 rehydrates
only device-grant metadata through the validated private store described above;
login text or caller-provided account values cannot select a vault item. The
current profile and authenticated account must match the stored authority.
There is no enumeration, v1 credential fallback, or recovery scan.

Manual sessions are still deleted on orderly quit. Remembered device grants
survive only with a durable settled binding. Exact deletion terminals and
metadata effects remain part of quiescence, and provider/vault/store workers
are drained only afterward. A failed cleanup reports its exact diagnostic and
retains the tombstone for remediation. An interrupted initial grant can leave
unresumable vault material; persistence does not imply broad crash recovery.

## Application projection and Qt rules

The application host publishes an envelope containing semantic core deltas and
optional immutable host-owned read projections. Qt never receives a provider
or credential object.

The public presentation boundary is
`bpr/application/github_projection.hpp`. Its intent callback admits only
profile activation, pull-request open, and pull-request refresh intents; it
cannot construct provider completion events. The update envelope strips core
host requests and accepted-completion internals before delivery. The host
projects accepted session and pull-request records into provider-free value
types before publishing them atomically with the core transition that accepted
them.

Composition supplies Qt with immutable non-secret profile choices derived
from the same validated profile configuration: opaque profile identity,
display label, canonical hostname, product kind, and selected transport.
Profile selection is the only identity-authority input. Login and account
identity remain read-only output of the authenticated `/user` observation and
cannot select a credential or vault binding.

Native-HTTPS activation obtains credential text through a Qt-owned
password-echo input with character and UTF-8 byte bounds. Composition sees
only the move-only sensitive buffer, immediately converts it to the
application credential-material owner, and resets the temporary on every
terminal path. The prompt is presentation behavior; it does not perform a
vault, provider, filesystem, process, or network effect.

The RFC-BPR-0002 presentation exposes:

- active hostname, authenticated account, and selected transport;
- profile selection/configuration controls;
- repository and pull-request locator input;
- Open and Refresh `QAction` state from command projections;
- explicit loading, authentication, permission, network, API-version,
  capability, rate-limit, malformed, oversized, canceled, and stale states;
- normalized PR metadata and read-only conversations.

The BPR-0031 presentation arranges connection and locator controls into
compact responsive rows. At narrow widths the locator actions move below
their inputs and the description/conversation splitter changes from
horizontal to vertical. Host, transport, account, pull-request identity, and
review content remain primary; capability/rate observations and exact commit
identities are reachable through separate Connection Details and Technical
Details disclosures.

Presentation state is explicit: `disconnected`, `connecting`, `empty`,
`loading`, `error`, `cancelled`, `loaded`, `refreshing`, `loaded-error`, and
`loaded-cancelled`. The last three nonterminal/failure variants retain
accepted content. A new-target load does not imply that prior
identity-sensitive content is current, while refresh failure or cancellation
does not discard an accepted projection.

Conversation models map stable GitHub identities to rows. Accepted refreshes
produce insert, remove, move, and data-change deltas for affected records.
They do not call `beginResetModel()` for a bounded comment, capability, status,
or selection change. A custom delegate wraps remote literal text without
elision while leaving native selection and focus painting to the active
style. The first accepted projection may establish an initial expansion;
subsequent accepted refreshes preserve Qt-local expansion and restore the
current selection when its stable identity remains. Presentation owns
wording, focus, accessibility, layout, and safe plain-text rendering.

The normalized presentation contains separate stable identities for reviews,
review threads, nested review comments, and pull-request comments. Qt derives
incremental model operations from those identities; it does not receive
provider records or row indexes from the host. A new pull-request open clears
the prior metadata and conversation projection immediately, while a refresh
keeps the last accepted projection through loading, failure, cancellation, or
stale completion.

Review-thread projection keeps `diffSide`, current `line`, and `originalLine`
as separate facts on the root and copies that anchor identity to its replies.
The provider accepts a current line thread with a known side and current root
line, or preserves a thread with neither nullable root coordinate as an
unanchored line-subject conversation. A current thread with only an original
line is refused; an outdated root may retain only its original line. A reply
may omit both of its own nullable coordinates and uses the root's anchor state.
File threads carry neither. Every GraphQL page repeats the pull request's
`headRefOid`, and the host rejects a snapshot whose REST and GraphQL head OIDs
differ. The selected-comparison composition may therefore place a current
right-side anchor only when its right commit equals that accepted head. It does
not place a left-side pull-request-diff line against the accepted base tip by
itself, because the provider contract does not carry GitHub's merge-base
identity. [[BPR-1212]] v002: the epoch host derives the unique merge base of
GitHub's base OID and the accepted head from the retained objects, never
fetching (`resolve_github_base_numbering`), and compares the file's blob there
with the comparison's left blob. Only when they are the same, or the merge base
is the left commit, is the left side current: base-side threads are placed on
the base pane, a base line takes a new comment, and the mutation host admits
that root as `side: LEFT` on the head's path and commit, with the base
document's line count, no file-comment fallback, and a recovery record that
keeps the side. A sparse review, several or no merge bases, or another blob
leave the left side unplaced and new base-side comments refused with the
reason.

The window inherits the application and operating-system palette, style, and
font. It does not force a palette or style, apply QSS, or install a replacement
application font. Qt application palette, font, style, and theme events refresh
the derived presentation fonts, native icons, delegate geometry, and
responsive layout without changing semantic state. Each published
status/diagnostic presentation update emits at most one combined polite
accessible announcement, and identical announcement text is suppressed.

Direct macOS evidence records system-default Dark startup under QMacStyle and
an earlier safe loaded-window application-local Dark-to-Light plus
13-to-17-point font exercise. The latter proves that the live event path
responds but is not the native global-transition evidence. The completed
owner-authorized native gate started the real application under system Light
and observed the same process follow an operating-system-global change to
Dark. Deterministic offscreen forced-style renders remain layout and scaling
evidence only.

The test-enabled real-application fixture includes the fail-closed native
appearance probe used by that gate. It reports readiness only after a
system-Light Cocoa/QMacStyle startup has loaded the public GitHub.com fixture,
selected `review.cpp` through the shipped Files-row gesture and semantic
workbench host, and received the resulting selected-diff publication through
the retained epoch/cache actor and real diff host. Before focus can begin, that
publication must bind the exact accepted GitHub authority, fixture base/head
commits, file identity, both blobs, normalized default options and digest, and
the rendered semantic selection. Pending host work remains pending within the
existing automated-run budget. A bounded refusal names the exact pending or
failed precondition, and a course that never captures the Light baseline emits
no appearance verdict markers. Budget expiry before the course state exists is
reported as pending startup observation, with helper use derived from the live
invocation counter rather than fabricated from absent probe state. A cut of the
real selection wire must fail even though the fixture-only direct-diff helper
remains compiled; the positive course requires that helper's invocation count
to stay zero.

After that selected diff is exact, the probe selects and collapses a stable
conversation, focuses the comparison, and retains an in-memory initial render.
Qt's color-scheme signal alone is not accepted because an application-local
override can emit it. On macOS the probe also reads the current-user global
appearance domain through a read-only, test-only CoreFoundation seam and
requires an independent Light-to-Dark global transition. Only after the Dark
palette has settled with the same platform, style, selection, focus, collapse
state, and model identity does it retain the second render. The captures must
have the same geometry and device-pixel ratio but different pixels. The probe
then creates two fresh non-aliased PNGs from those exact captures and reloads
both before reporting success; failed probes leave no render artifacts. The
probe observes appearance and never changes the operating-system setting. The
owner-authorized wrapper performed the external system change, required every
marker, and restored the original Dark setting. The global preference key is
evidence plumbing, not production appearance logic. Incomplete arguments, a
non-GitHub.com fixture, aliased or existing paths, offscreen execution, the
wrong startup scheme, a missing signal or global transition, identical pixels,
or state drift discard the claim and fail.
User-selectable System, Light, and Dark profiles remain RFC-BPR-0006 work.

## Composition and lifecycle

The composition root is the only scope that knows all concrete pieces. Its
production profile set is explicit:

- built-in GitHub.com native HTTPS;
- built-in GitHub.com CLI (unavailable in the historical RFC-BPR-0002 course);
- optional GHES 3.18 native HTTPS from an explicitly supplied canonical
  hostname;
- optional GHES 3.18 CLI (unavailable in that historical course).

The CLI assertions in the following recorded course describe RFC-BPR-0002,
not acceptance of the later macOS RFC-BPR-0015 read path.

The provider owns immutable copies of every validated profile and routes by
exact profile identity. GitHub.com profiles have built-in provenance; GHES
profiles have explicit-user provenance. CLI activation reaches the same
semantic host path but returns `cli_unavailable` before the process port,
which keeps the unavailable state visible and produces zero child launches.
The tests-disabled composition contract also rejects CLI runner-factory
symbols in the public application, while its self-test enumerates every
visible CLI profile and requires the bounded unavailable presentation plus
`CLI_UNAVAILABLE_COUNT:1` for the default set and
`CLI_UNAVAILABLE_COUNT:2` with explicit GHES. Public selection performs one
bounded noncredential profile-resolution effect per row from clean start.
When an interactive selection replaces an authenticated native profile, host
identity invalidation first exact-deletes the predecessor credential binding;
the CLI profile itself is neither enrolled nor retrieved.
After the provider receives the CLI viewer request, provider and transport
counter tests independently require zero transport-factory, wire, capability,
CLI-specific effect, and process-port calls. The
`application_public_path_ghes_cli_unavailable` case supplies the direct
two-row public-path evidence. Since [[BPR-0909]] those rows are gone: GitHub
CLI is the discovered Use GitHub CLI entry, and the self-test instead
requires no precomposed CLI row and the entry offered exactly where the read
is (`CLI_ENTRY_ABSENT_OK` / `CLI_ENTRY_OFFERED_OK` with `CLI_ENTRY_CARDS`,
[[BPR-0982]]).

The visible ownership order is:

```text
validated non-secret profile store
provider and vault effect executors
platform credential vault
native network transport
normalized GitHub provider
decision executor
Qt window and models
host-owned credential binding catalog
application host
revocable native Git smart-HTTPS capability
app-owned minimal Git protocol-v2 client
installed-Git local pack/object/ref adapter
app-owned bare-cache and epoch-catalog adapters
dedicated epoch core and application host
```

Provider and vault effects use separate worker executors. The Qt event-loop
decision executor is a required live service through host quiescence. A
successful post is retained exactly once for eventual non-inline execution.
Provider and vault submissions that return normally produce exactly one
terminal; cancellation is advisory and does not suppress it. Composition
keeps every referenced port, projection, decision lane, and binding catalog
alive until the host reports quiescence.

Projection delivery is synchronous but observational. An exception sets a
diagnostic latch and cannot veto a core transition, effect, or cleanup.
Reentrant public ingress is deferred into FIFO generations; only the
generation present at turn entry runs in that turn, preventing projection
feedback from starving queued work. New ingress cannot overtake an older
continuation, and shutdown closes ingress with terminal priority.

Destruction reverses ownership. Public shutdown first closes ingress,
advances or invalidates the active generation, cancels provider work, requires
an exact delete terminal for the retained v2 credential authority, and
discards staged snapshots. Only after host quiescence does composition stop
the provider, close and join both workers, and permit Qt projections,
transports, vault handles, the binding catalog, and the decision lane to be
destroyed. A failed exact credential deletion is surfaced and leaves the
catalog tombstone; it is not silently treated as successful cleanup.

When RFC-BPR-0003 is composed, shutdown first closes epoch ingress, revokes
every smart-HTTPS lease, cancels epoch transport/Git/catalog work, drains the
serialized epoch completion lane, and closes catalog and bare-repository
handles. Only then may GitHub credential deletion, native transport teardown,
and the existing reverse ownership order continue. Revocation and stale-result
rejection do not authorize implicit pruning or deletion of retained history.

Recorded GitHub.com and GHES fixtures replace the native transport factory
behind the same provider port in test-enabled runs. They must use the same
profile selection, provider, host, core, Qt, and composition path as the real
application; a direct normalizer helper or static mockup is not public-path
evidence. These fixtures prove the structural public path and rendered
projection, not a real-network native-HTTPS request. Tests-disabled
composition inspection proves production selects the native transport factory,
and the separate native loopback fixture directly verifies its wire behavior.

Stored replay manifests and device-response projections use private typed Glaze
records. Method parsing, operation order, canonical request bytes, source
descriptors and fault injection remain part of replay admission. Device fixtures
own retained JSON values and supply only synthetic stand-ins for omitted
transient fields. Capture facts serialize owned strings and exact numeric values
with Glaze while retaining the existing capture schema and field names. These
readers and writers remain inside the test-enabled composition boundary.

The fixture transport, fixture vault, fixture command-line choices, native
appearance-probe choices and evidence, and fixture evidence types are compiled
only into test-enabled targets. The public application header is
test-definition-free. A clean
`BUILD_TESTING=OFF` Release configure/build inspects compile commands, the
Ninja graph, symbols, and raw binary bytes to prove that the fixture
translation unit, test macros, paths, arguments, labels, native appearance
markers, and evidence strings are absent.

## Build and dependency allocation

The selected dependency direction is:

```text
better_pull_review_core
    standard/domain C++ only

better_pull_review_application
    core + normalized GitHub port/read models

better_pull_review_adapter_github
    application + Qt Core/Network
    owns provider contracts, QProcess broker, HTTPS, and vault bindings

better_pull_review_adapter_qt
    application-facing contracts + Qt Core/Gui/Widgets
    no provider/process/network/vault dependency

better_pull_review_composition
    wires concrete adapters; owns no hidden service locator
```

The Qt-free preset compiles and links neither effect-backed provider
implementations nor presentation targets and continues to configure, build,
and test without Qt or provider packages. Boundary checks must forbid
provider/process/network/vault types from core, forbid effects from Qt
presentation, and allow effect APIs only in their designated external adapter
sources.

GitHub Actions has zero role in configuring, building, testing, packaging,
releasing, or verifying this feature. Evidence comes from local CMake/CTest,
recorded contracts, real-process inspection, and target-native vault/network
tests.

Build and packaging records still report the actual Qt Network and
platform-vault compile, link, and runtime dependencies selected by the
provider. RFC-BPR-0014 owns delivered-package signing and distribution
certification; it does not defer accurate RFC-BPR-0002 dependency accounting.

The composed agent route consumes only the already accepted provider-free PR
authority and revision. Its late-bound session-catalog port forwards to the
review ledger for that exact hostname/account/repository/PR scope. Core policy
and the reloaded private-context selection are validated before the catalog
atomically admits a session; only then may Git workspace, environment,
executable, or process work begin. The catalog persists revision metadata, not
GitHub credentials, provider objects, private prose, or process results. A
fresh composition over the same data root can therefore present the retained
binding and next monotonic ID without reopening a provider authority channel.

## No-impact and deferred regions

RFC-BPR-0002 has no implementation impact on:

- Git object caching and review epochs;
- diff computation, line mapping, and syntax highlighting;
- the private SQLite review ledger;
- GitHub review/comment/viewed/resolve mutation execution;
- queue, observer, AI, and notification workflows;
- QML, WebEngine, browser, product-owned listener, plugin, telemetry, or
  updater implementations;
- package formats, signing, notarization, or installer behavior.

Git cache and epoch work remains RFC-BPR-0003. GitHub mutations remain
RFC-BPR-0008. Delivered-package certification remains RFC-BPR-0014.

RFC-BPR-0003 therefore integrates through the accepted-revision handoff and
revocable native smart-HTTPS capability defined above while leaving
RFC-BPR-0002's core, JSON provider, and Qt ownership unchanged. Its installed
Git, cache, catalog, epoch host, and epoch presentation remain separate
components.

Within RFC-BPR-0002, the Linux Secret Service vault lifecycle, the inactive
CLI broker's process/invocation boundary, and the native HTTPS internal wire
path on macOS and Linux are directly verified. Positive CLI enablement is
outside that historical milestone scope; its public profile was visibly
unavailable and launched zero processes. RFC-BPR-0015's own ledger records its
later macOS acceptance.
BPR-0028 verifies the safe Qt projection and presentation. BPR-0029
structurally composes the explicit profiles, native HTTPS factory, platform
vault factory, provider, host, core, and Qt path. Its recorded public fixtures
substitute fixture transport/vault state and verify that orderly shutdown
issues the exact delete and leaves no credential in that fixture vault. The
separate target-native vault lifecycle verifies exact native-item cleanup; no
crash/restart recovery claim is made. The pinned BPR-0029 Linux v2 snapshot
passed in an Ubuntu 26.04 x86_64 full-system QEMU guest with exact native
schema cleanup and tests-disabled Release selection/seam evidence. Retained
Windows target-native credential/HTTPS/public-fixture evidence remains BPR-0036
work.
Query-bearing REST redirects remain rejected by the closed BPR-0038 design
decision; any future relaxation requires new direct public-contract evidence
and separately scoped work. The open Windows execution is nonblocking for all
other slices and later-RFC work; it blocks only its affected platform claim
and final RFC-BPR-0002 Auto-Verified closeout.

## Public-preview host lifecycle

`activate_public_preview` enters the core's explicit public-preview mode only
when composition supplies anonymous read authority. The shipped macOS selection
is enabled (BPR-0316); Linux and Windows answer `public_preview_unavailable`.
GHES selection on macOS answers policy refusal. From a fresh signed-out launch, activation/refusal sends zero requests and
touches neither vault nor Authorization material. Replacing an authenticated
session still owes that prior identity its existing credential cleanup.

The host synthesizes an empty viewer and policy-unsupported capability record,
uses the normal provider/normalizer, and projects REST comment groups separately
from unavailable GraphQL review threads. `public_preview_projection.hpp` is the
Qt-facing contract. Mode switches use the existing identity invalidation path:
pending work, accepted content, account-relative associations, queue, epoch,
cache session, and command projections cannot survive under the next authority.
Anonymous smart HTTP carries a revocable public authority instead of an account
credential. The native read and rendered-diff course is recorded in
[the RFC-0017 ledger](../verification/RFC-BPR-0017.md).

Startup metadata loading also owns the cleanup barrier for early Sign out or
replacement sign-in. The core supplies the invalidation request identity;
the host validates the loaded authority against the compiled native profile,
tombstones that binding, and waits for both exact vault deletions before
starting replacement session effects. Sign out followed by quit drains that
cleanup; ordinary quit during loading retains the remembered grant.
An unresolved metadata-load error refuses later session establishment for the
same application lifetime, before profile or vault effects. Restarting the
application retries the load. A restored grant that arrives during a pending
device flow remains the preceding session's binding until that flow validates
its own account.

## Connected CLI destination routing

The account lease and CLI account-observation wrapper forward the configured
transport's sparse commit/tree capability. The epoch host combines that
capability with the accepted review shape to select bounded on-demand objects.
Any review whose reported shape is consistent loads that way, small ones
included: a pull request's counts do not bound its repository's trees, blobs
or ancestors (`object_loading_for`, pinned by
`sparse_capability_applies_independently_of_pull_request_size`). A review
whose shape is missing or inconsistent acquires complete objects.
Neither wrapper can invent support when its underlying transport lacks it.
An observed account mismatch permanently removes the capability.

Exact selected-blob reads traverse the same observed Git lease as discovery,
refs and pack acquisition. [[BPR-1327]] adds `read_exact_blobs` for the files
the reviewer is about to open: one GraphQL read of at most 16 blobs by object
ID (owner, name and IDs as variables, never in the query text). Each answer is
checked against its object ID; a binary, truncated, missing or mismatched one
is answered absent and is read on its own if the reviewer opens its file.

[[BPR-1370]] The same query also selects `viewer { login databaseId }`. GitHub
resolves the viewer from the token that authorized that request, so the answer
names the account that performed this read, not one observed a `gh` call
before or after it. The lease holds that account and login to its bound
account exactly as it holds an observation: a mismatch refuses the read,
revokes the transport and removes the lease's capability for good; an answer
that names no account exactly is refused. Such a read takes no `gh api user`
observation. A single exact read first asks this query for its one blob, at
most 512 KiB of text. Only a blob it cannot carry (binary, truncated, larger,
missing or not verified) or a failed query takes the REST read between the
account observations, one pair in the transport and one in the wrapper, as
before. A cold open of two text sides therefore takes two `gh` processes
instead of ten. What the observations never closed stays open: the active
account may still change between this read and the next request, which makes
its own check. The request retains its complete context, expected blob
identity and byte limit. Cancellation and lease revocation apply before bytes
can return; the underlying transport verifies the blob itself. A standalone
sparse bridge check does not prove this account composition. BPR-0794/0795
regressions cover both wrappers and a blob that must cross their guarded read
path; `gh_cli_git_smart_transport` composes the provider's lease over real
fixture `gh` processes and counts them.

The setup profile observation uses the host's accepted authenticated session,
including CLI leases that intentionally have no native credential-vault key.
Only an accepted CLI session participates in pasted-link resolution; account
inventory does not grant authentication. The core prefers an authenticated
exact-host profile over an inactive native profile, then retains the existing
native sign-in or anonymous-preview fallback. Opening checks the accepted
session's profile and host against the configured destination before dispatching
the window read. Publication admission is unchanged.
