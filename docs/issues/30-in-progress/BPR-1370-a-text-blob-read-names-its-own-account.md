---
schema: "implement-rfc.card.v1"
id: "BPR-1370"
title: "A text blob read names its own account instead of being bracketed by account checks"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"owner_request":{"date":"2026-10-09","channel":"chat","quotes":["Can the return result not tell us what account it read with instead of a separate check?"]},"reservation":"docs/issues/_reservations/BPR-1370--BPR-1370--68670e537484.json","related_cards":["BPR-1360","BPR-1327","BPR-1344","BPR-0794","BPR-0795"],"found_by":"BPR-1360's macOS live run (BPR-Test PR #43), 2026-10-09","filed_by":"rfc-implementer, at the coordinator's request"}
---

# BPR-1370: A text blob read names its own account

## Observation

[[BPR-1360]] made a cold open read both sides at once. Live on
`MikeMarcin/BPR-Test#43` the open went from 4.3 s to about 2.35 s, short of
the 2.1 s expected. Each exact blob read through the connected GitHub CLI is
five `gh` processes: an account observation (`gh api user`) by the lease's
wrapper, one by the transport, the REST blob read, then the transport's and
the wrapper's second observations. The wrapper's observations share the
lease's one `gh` process slot, so the second side waits 300-500 ms for the
first side's.

The owner asked (chat, 2026-10-09): "Can the return result not tell us what
account it read with instead of a separate check?"

## Outcome

A blob that GraphQL can serve as text is read through GraphQL with
`viewer { login databaseId }` in the same query. The answer itself names the
account that performed the read, and the lease compares it with its bound
account and refuses on a mismatch, exactly as the observations do. The
existing batch query ([[BPR-1327]]) gets the same field and loses its
bracketing observations. Binary, truncated or oversized blobs keep the REST
read with the bracketing observations.

## Acceptance

- The card records why the observations exist and which guarantee is kept.
- Fixtures: a matching viewer, a mismatched viewer (refused) and a binary
  fallback.
- A test that fails before and passes after: a text blob read makes no
  separate viewer call.
- A `gh` process count per cold open, before and after.
- A live cold open on PR #43: three or more runs, median and p90, machine
  load, `gh` calls per open; aiming at about 2.1 s.

## Why the observations exist

Sources: `src/adapters/github/src/github_provider.cpp`
(`cli_observed_account_transport`, "Observations detect account changes; they
cannot contain a concurrent gh auth switch"),
`src/adapters/github/src/gh_cli_git_smart_transport.cpp` (`viewer`, used
before and after every read and acquisition), RFC-BPR-0015 ("The current
`gh api` has no per-invocation account selector..."), the adapter's
`AGENTS.md`, and the threat model's "Pre/post observations can detect some
account changes but cannot bind a credential generation or prevent a
concurrent account switch".

What they guard against:

- **The active `gh` account is mutable, global state.** `gh api` has no
  `--user`. Between the sign-in observation and any later request, the
  user (or another tool) may run `gh auth switch`, `gh auth login` or
  `gh auth logout`, and every later `gh api` call then runs as whoever is
  active. The lease is bound to the account `gh api user` reported at
  sign-in (database ID and login). A read made as another account could
  return what the bound account cannot see, or be cached under the bound
  account's namespace (cache keys include the account). So every request is
  checked against the bound account, and one mismatch refuses the lease for
  good (`Mismatch`), revokes its Git transports and removes its sparse
  capability.
- **A token for a different host.** Every call names `--hostname`, so `gh`
  picks the token it holds for that host. The observation is made on the same
  host, so the comparison is between accounts on one host.
- **What they do not guarantee.** They detect; they do not prevent. A switch
  and a switch back between the two observations goes unnoticed, and they
  bind no credential generation. The RFC and threat model say so.
- **Not about accounts.** The configuration-file and executable checks
  (`configuration_unchanged`, the Git executable observation in `admission`)
  are separate and stay on every read.

The guarantee kept: every read's account is the lease's bound account and
login, or the read is refused and the lease is refused for good. A GraphQL
answer's `viewer` is resolved by GitHub from the token that authorized that
same HTTP request, the one that read the blobs. It names the account that
performed this read rather than one that was active a `gh` call earlier or
later, so it is at least as strong as the observation pair for this read,
with no window between the check and the read. It still says nothing about the
next request, which makes its own check, and it binds no credential
generation. The configuration and executable checks are unchanged.

## Design

- **Batch query.** `compile_gh_cli_exact_blobs_request` selects
  `viewer{login databaseId}` beside `repository{...}`. The transport's new
  `read_exact_blobs_naming_viewer` (interface
  `viewer_named_blob_reader`, adapter-internal header
  `github_viewer_named_blob_read.hpp`) runs that one `gh` process and returns
  the verified blobs with the answer's account. A missing, empty, fractional,
  zero or string `databaseId`, or a missing login, is malformed and refuses
  the whole answer. An account other than the request authority's revokes the
  transport and answers every blob absent. The transport's plain
  `read_exact_blobs` is that read without the account, refusing as
  `authentication` when it revoked.
- **The lease.** `observed_git_transport::read_exact_blobs` reads through
  `read_exact_blobs_naming_viewer` when its transport offers it. The answer's
  account goes through `cli_observed_account_transport::admit`, the one
  comparison every observation now also passes (database ID and login against
  the bound account; a mismatch sets `Mismatch` for good), and the lease
  revokes the read on a mismatch.
- **Single exact reads.** `observed_git_transport::read_exact_blob` first
  asks the same read for its one blob, at most 512 KiB of text (the batch
  bound). If the answer carries the verified blob, that is the read: one
  `gh` process, no observation. If the blob is absent (binary, truncated,
  larger, missing or not verified) or the query failed, it takes the REST read
  between the observations exactly as before. Cancellation and an account
  mismatch do not fall back.
- **Unchanged.** Discovery, refs and pack acquisition, the REST read, the
  account-sign-in observation and every non-Git request keep their
  observations. A transport without the interface (every fake but the new one)
  keeps the old path.

