# Fixture Catalog

The implementation must maintain sanitized deterministic fixtures covering at least:

## Implemented GitHub read contract v1

`tests/fixtures/github/contracts/v1/` contains paired `github-com` and
`ghes-3.18` manifests for the ten RFC-BPR-0002 read operations. Each manifest
pins the product, synthetic authority, API roots, request method, relative
path, canonical query, non-secret headers, GraphQL document and variables,
and selected response projection. The two response trees contain the same
synthetic semantic facts.

These fixtures are hand-minimized, sanitized provider projections, not
literal complete wire dumps. They deliberately omit authorization, cookies,
transport metadata, remote URLs, patches, source content, email addresses,
home paths, request identifiers, and provider fields that the normalizer does
not consume. The strict recorded-contract harness rejects additions or
omissions from this selected projection. A production transport must validate
the raw envelope and project the allowlisted fields before this strict
normalization boundary; additive raw GitHub response fields are not an excuse
to expose provider records inward.

The paired baseline contains repository, pull-request, viewer, author, review,
thread, comment, commit, check, and search identities. GraphQL repository and
pull-request node IDs are correlated with REST, and actor node IDs plus logins
are correlated across every normalized author. GraphQL file paths map
bijectively to the REST file collection and carry viewed state; threads carry
per-target resolve/unresolve availability. Contract variants directly
exercise null/deleted and non-User actors, fork heads, nullable review commit
IDs, renamed files, every viewed state, queued checks, and both line- and
file-level review threads. Every selected response is explicitly marked
complete; unknown completeness or a transport-declared continuation fails.
The files, threads, and nested comments GraphQL connections each have
terminal `pageInfo`; a true `hasNextPage` result fails rather than being
normalized as a truncated snapshot.

The fixture security gate verifies the exact tree and operations, rejects
duplicate JSON members and encoded secret/path/email/foreign-authority
bypasses, and checks both hosts against one independent normalized
expectation. No live or private repository response was recorded.

## Implemented GitHub capability contract v1

`tests/fixtures/github/capabilities/v1/` contains independent complete
introspection responses derived from the official public GitHub.com and GHES
3.18 schema snapshots with the probe's exact static query. Per-host manifests
pin the official source URL and digest, retrieval metadata, query digest,
generator version, complete response digest, and actual Mutation and
PullRequest field counts. Synthetic missing/incompatible mutation-contract
descriptors derive only from the GitHub.com response. The contract covers the
exact review-thread query fields, viewed-file state, mark/unmark inputs and
payloads, resolve/unresolve inputs and payloads, and their dependency
relationships.

These are public schema-contract observations, not live account-permission
claims or authenticated API recordings. Unsupported results are synthetic
because both selected schemas support all four capabilities. Operational
failures and partial/malformed responses are constructed in the focused probe
test and must remain all-Unknown. The fixture security gate pins the exact
tree, provenance and response digests, rejects duplicate JSON members and
secret-bearing content, proves the host responses are independent, and keeps
every relative descriptor reference inside the capability fixture root.

## macOS Keychain integration fixture

The BPR-0022 target-native fixture uses the fixed
`org.betterpullreview.github-native-credential.file-keychain.integration-test.v1`
service and one exact synthetic authority in the user's default file-based
Keychain. It never selects the production service. A resource-locked CTest
setup/lifecycle/cleanup sequence proves initial absence, add, retrieve,
in-place replacement, retrieve, delete, post-delete absence, and exact final
cleanup.

The parent generates runtime-only printable canaries, sends them to a
same-executable child over framed standard input, uses a neutral audited
working directory and bounded child deadline, and captures only fixed
pass/fail output. It checks arguments, the explicit child environment,
executable, source, and isolated scratch/XDG tree for raw canaries. Combined
source-boundary checks reject settings, SQLite, environment, file, process,
logging, UI, and alternate-Keychain fallbacks from the production adapter.
The scratch scan does not inspect the user's complete home directory, and the
fixture does not claim cross-rebuild cleanup after an ad hoc code-signature
change.

## Windows Credential Manager integration fixture

The BPR-0023 fixture uses the fixed
`org.betterpullreview.github-native-credential.windows-credential-manager.integration-test.v1/`
prefix and one exact synthetic authority. It never selects the production
prefix. A resource-locked CTest setup/lifecycle/cleanup sequence is prepared
to prove initial absence, create, retrieve, single-call replacement, retrieve,
delete, post-delete absence, and exact final cleanup on Windows 10/11 x64.

The parent generates two runtime-only printable canaries, including one at the
complete 2,560-byte portable/native bound, and sends framed bytes to a
same-executable child through anonymous standard input. It starts the canonical
executable directly with an exact inherited-handle list, a sorted minimal
Unicode environment, and a unique neutral scratch directory. The child emits
only fixed pass/fail text. The parent bounds execution, requires confirmed
child termination before credential cleanup or scans, checks parent/child
arguments and explicit environment, and searches fixed stdout/stderr,
executable, source, and the isolated scratch tree for random 32-byte prefix
and suffix probes from both canaries. The short probes detect truncated
replacement leaks without placing either complete credential on an audited
surface. Successful scratch removal must leave the exact path absent.

If termination cannot be confirmed, the test fails closed and deliberately
does not delete storage beneath a potentially live child; the independent
CTest cleanup still attempts exact credential deletion. The fixture does not
enumerate Credential Manager. It also does not inspect arbitrary application
settings, SQLite files, Qt UI text, Windows Event Log, ETW, or the user's
complete profile. Settings, SQLite, UI, logging, file, registry, environment,
DPAPI, Credential Locker, and alternate-vault exclusions come from the
production source boundary and must not be attributed to the runtime fixture
alone.

This fixture and its WIN32 registration are checked in but have not run on a
native Windows environment. Host-stub compilation and an x64 MinGW PE
cross-compile do not satisfy that lifecycle claim. BPR-0036 retains the
target-native run and exact cleanup as open work.

## Linux Secret Service integration fixture

The BPR-0024 fixture uses the fixed
`org.betterpullreview.github-native-credential.secret-service.integration-test.v1`
schema and one exact synthetic authority. It never selects the production
schema. Each item has only the 66-character `authority` value plus the
`xdg:schema` attribute supplied by libsecret, and resides only in the default
collection.

The resource-locked CTest setup/lifecycle/cleanup sequence proves initial
absence, a 73-byte store and retrieve, atomic replacement with a 2,560-byte
value and retrieve, delete, repeated absence, and exact final cleanup. Store
labels the value `application/octet-stream`; retrieve accepts that type or
exact `text/plain` because the tested gnome-keyring 50/libsecret 0.21.7 stack
was observed normalizing only the content-type metadata while preserving the
opaque bytes. After the runtime-canary lifecycle, the same native child repeats
the 73-byte and 2,560-byte lifecycle with deterministic opaque values
containing NUL, high-bit, and invalid UTF-8 byte sequences.

The parent generates both runtime-only printable canaries, sends framed bytes
to a same-executable child over standard input, provides an explicit minimal
environment and neutral isolated scratch directory, bounds child execution,
and accepts only fixed output. It checks parent and child arguments,
environment, stdout/stderr, executable, source, and isolated scratch for
canary probes. Parent and independent fixture cleanup both delete only the
exact fixture authority and verify that item absent. A separate controlled
D-Bus query verifies schema-wide integration-test absence after the matrix.

Controlled direct modes separately prove typed unavailable with no desktop
session bus, typed locked with a truly locked default collection, and
rejection of a hostile dependency-level `SECRET_SERVICE_BUS_NAME`. Production
does not read or mutate that variable: the adapter opens with the literal
canonical service argument and `SECRET_SERVICE_NONE`, rejects any opened proxy
whose exact name is not `org.freedesktop.secrets`, and establishes a session
only after that check. The live hostile owner observed only proxy-property
inspection and no `OpenSession`, collection, or credential-item method.
Prompt overrides deny interaction and make one bounded, no-auto-start
`Dismiss` when given a prompt. Source boundaries prove the absence of unlock,
prompt display, collection creation, alternate-vault, plaintext, settings,
SQLite, environment, file, process, network, logging, and UI fallbacks; the
runtime fixture alone does not claim arbitrary user-home or system-log
inspection. Fake-backed operation failures and direct native-status
classification exercise access-denied and failed-delete terminals without
claiming a desktop policy can reliably force those two states.

The fake contract and all three native fixture CTest cases passed in an Ubuntu
26.04 x86_64 full-system QEMU VM with an emulated CPU, Linux 7.0,
Qt 6.10.2, libsecret 0.21.7, and gnome-keyring 50. The four focused cases ran
under Xvfb, a desktop D-Bus Secret Service session, and a dedicated real user;
the controlled unavailable, locked, hostile-redirection, and cleanup checks
also passed. This directly verifies the selected Linux vault stack in a
full-system VM, not bare metal. Linux native HTTPS and the public composed
provider path remain pending, so this fixture does not verify the combined
Linux vault-plus-HTTPS claim.

## GitHub CLI process and trust fixtures

The BPR-0025 compiled-child fixture launches its own canonical executable
through the production `QProcess` runner. Helper modes directly inspect the
element-wise argument vector, requested child-environment allowlist, standard
input, and canonical directory currently outside repository ancestry. A
random parent environment name/value unknown to production is absent in the
child. Raw macOS inspection
allows and separately verifies only Darwin's deterministic
`__CF_USER_TEXT_ENCODING` addition; raw Linux inspection requires the
requested fact and explicit locale with no additions. Exact limit and
limit-plus-one cases cover input, retained output, error, duration,
cancellation, nonzero exit, crash, simultaneous stream pressure, and
termination/reaping. Every failing terminal exposes only a fixed category and
discards partial standard output and all standard error.

The Unix fixture directly observes closed inherited descriptors, disabled core
dumps, reset signal disposition, and that the child is the new session and
initial process-group leader. It proves clean leader exit, nonzero failure,
timeout, and cancellation signal descendants that remain in that initial
process group while the direct child is reaped. An escape case proves a
descendant that moves to another group survives until explicit fixture
cleanup. The fixture also proves a request whose absolute deadline expires
behind the runner's sole worker never launches later.

A compiled seam proves completion-watchdog quarantine, zero launch for another
request while quarantined, and recovery after the worker resumes. Source and
hostile architecture gates, not a compiled live-process failed-reap fixture,
require the owner-managed worker to retain a live `QProcess` after actual
bounded reap failure and continue kill/reap work without invoking the Qt
destructor inside the completed call.
Allocation-free request-shape validation and pre-cancellation precede
work-item allocation and request copying. Marker-backed N+1 cases cover path,
argument, environment, input, stream, time, and reap dimensions and require
zero launches.
Reads use a fixed buffer and request at most the remaining retained bound plus
one byte. This does not prove a hard cap on Qt's private in-flight pipe
allocation. Windows Job Object descendant containment, private pipe bounds,
and target-native behavior remain open in BPR-0036.

The invocation fixture compiles every selected GitHub.com and GHES 3.18 read
operation. It checks exact REST-prefix removal, literal GraphQL routing,
hostname/method/header arguments, standard-input bodies, fixed hardening
environment, platform credential-session facts, and the absence of arbitrary
environment or caller argument input. Profile, correlation, server, route,
method, header, body, executable, environment, and working-directory drift
fail before the process port is called. The production trust gate is also
tested with a fake successful child: it remains `cli_unavailable` and makes
zero calls.

`tests/scripts/verify_gh_cli_trust.py` is a separate direct selected-binary
fixture. It resolves one absolute `gh`, makes and executes an owner-private
copy, hashes that copy before use, and verifies its digest again after every
probe. It creates a private temporary config and random synthetic startup
token, generates a disposable loopback certificate, and captures only fixed
TLS/redirect categories, counts, booleans, version, and digest. No GitHub
endpoint is contacted, and every child runs from the canonical
non-repository temporary directory. The certificate fixture requires the
selected fixed bad-certificate alert and zero HTTP requests. The `302`,
`307`, and `308` POST redirect fixtures compare the body digest and
authorization value without retaining either, and record whether a distinct
loopback hostname is contacted.

`gh` rejects the fixture's ephemeral `host:port` as a `--hostname`, so the
verification-only invocation retains the synthetic `github.com` startup
hostname while its full URL selects loopback. Official selected-version
source establishes that relative production endpoints use the same Go
client. This is exact-binary behavior characterization, not exact
production-authority proof.

The selected macOS `gh` 2.95.0 binary, SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
produced one bad-certificate alert with zero HTTP requests but followed the
cross-authority `302` once. Its selected origin received the exact 47-byte
body and synthetic authorization; the distinct sink received neither. The
`307` and `308` origins received both, while their sinks were not observed
and therefore remain unproven rather than accepted as redirect rejection.
The `302` is direct evidence that the selected CLI cannot satisfy the
no-cross-authority contract, not a successful broker trust result. The
temporary executable and configuration are verification-only and removed
after the probe; production never admits `GH_CONFIG_DIR`.

The fixed production gate also records three independent source-backed
blockers. Production's canonical path/name validation does not bind the
eventually launched image to the probe's exact copied-byte digest across a
replacement or update race. Persisted CLI configuration can select
`http_unix_socket` through the platform home/application-data path, and
`gh api --hostname` has no per-invocation account selector that binds it to
the request's correlated credential identity generation. An external
`gh auth switch` can change the active account for that host. The fake
successful child still makes zero production calls because
executable-identity, redirect, ambient-configuration, and credential-identity
trust must all be established independently. BPR-0037 owns that
production-enablement work together with owner/permission and
validation-to-launch binding for the canonical/currently non-repository
working directory, Unix descendant escape from the initial process group, and
binding PID/process-group identity across cleanup so a reused numeric group
cannot receive a late signal. The current group signaling is not claimed
race-free. It must also bind selected Linux CA-file/CA-directory identity and
contents through the CLI's later open and directly prove hostile replacement
and content mutation fail closed.

## GitHub hosts

- GitHub.com current contract
- GHES 3.18 REST and GraphQL contract recordings
- Missing GraphQL capability
- Permission denied
- Rate limited and secondary-rate-limited responses
- Proxy and corporate CA configuration
- Stale head during mutation

## Pull request histories

- Single upload, simple edit
- Multiple uploads with unchanged files
- Force-push/rebase with retained old head
- Base branch advance without head movement
- Base branch change
- Pure rename
- Rename with edits
- Added/deleted/binary/submodule files
- Review associated with historical commit

## Diffs

- Replace, insert, delete, and mixed hunks
- Unequal old/new group heights
- Multiple nearby groups
- Long lines and tabs
- Unicode and invalid byte replacement policy
- Multiline comment/string crossing hunk boundary
- Whitespace-only policy changes
- Large generated file and highlighting fallback

## Conversations

- New thread, replies, edits, deletion, resolution, reopening
- Outdated anchor
- File-level comment
- Pending review drafts
- External-client mutation between refreshes

## Local state

- Read and acceptance invalidation
- Account/repository isolation
- Schema migration
- Abrupt termination and WAL recovery
- Deleted local database

## AI

- Codex one-shot JSONL
- Codex App Server session and approval events
- Claude stream-json
- External terminal fallback
- Head changes during analysis
- Invalid finding anchor
- Corporate host mapped to forbidden personal provider

## Architecture and concurrency harnesses

- Core-only build environment with Qt, SQLite, GitHub, Git, AI, and platform provider packages unavailable
- Deterministic event/observation replay corpus for navigation, receipts, defects, queue state, and AI staleness
- Manual executor fixtures that reorder, delay, fail, cancel, and deliver stale completions
- Qt model harness asserting exact insert/remove/move/data-change signals and persistent-index behavior
- QAction projection harness covering enabled, checked, and disabled-reason state across capabilities and selections
- Large-review structural counters proving active-set/changed-set work and absence of routine whole-model resets
- Shutdown fixture with pending GitHub, Git, diff, syntax, store, queue, and AI operations
