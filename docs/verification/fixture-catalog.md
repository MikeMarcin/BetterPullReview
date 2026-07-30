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
