---
schema: "implement-rfc.card.v1"
id: "BPR-1370"
title: "A text blob read names its own account instead of being bracketed by account checks"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"owner_request":{"date":"2026-10-09","channel":"chat","quotes":["Can the return result not tell us what account it read with instead of a separate check?"]},"reservation":"docs/issues/_reservations/BPR-1370--BPR-1370--68670e537484.json","related_cards":["BPR-1360","BPR-1327","BPR-1344","BPR-0794","BPR-0795"],"found_by":"BPR-1360's macOS live run (BPR-Test PR #43), 2026-10-09","filed_by":"rfc-implementer, at the coordinator's request","acceptance":["BPR-1370:why","BPR-1370:fixtures","BPR-1370:fail-before","BPR-1370:process-count","BPR-1370:mac-live","BPR-1370:rows"]}
acceptance: [{"criterion":"BPR-1370:why","outcome":"pass","revision":"5744012e0","evidence":{"kind":"artifact","path":"docs/issues/60-done/BPR-1370-a-text-blob-read-names-its-own-account.md#why-the-observations-exist","detail":"The observations exist because gh api has no per-invocation account selector: a gh auth switch, login or logout between the sign-in observation and a request changes the account every later call runs as. Every call names --hostname, so a token for another host is not in play. They detect but do not prevent a switch and bind no credential generation (RFC-BPR-0015, threat model). Kept: every read's account equals the lease's bound account and login, or the read and the lease are refused for good. A GraphQL answer's viewer is resolved from the token of that same request, so it binds this read with no window between check and read."}},{"criterion":"BPR-1370:fixtures","outcome":"pass","revision":"5744012e0","evidence":{"kind":"command","path":"tests/github_contract/gh_cli_git_smart_transport_tests.cpp","detail":"cold_open_course over real fixture gh processes through the provider's CLI lease: matching viewer reads both sides (2 gh processes); a viewer naming login other or database ID 999 is refused as authentication after 1 process, revokes the read, reads nothing more, and refuses the lease (no sparse capability, acquire refused); a binary side falls back to the REST read (6 processes) and is refused when its observation fails. Batch: seven viewer shapes without an exact login and database ID are malformed and do not revoke; viewer 999 refuses and revokes. github_provider test_cli_text_blob_reads_name_their_account covers the lease's rules with a viewer-naming fake (fallback on absent blob or failed query, none on cancellation or mismatch, 512 KiB cap, batch with no observations)."}},{"criterion":"BPR-1370:fail-before","outcome":"pass","revision":"5744012e0","evidence":{"kind":"command","path":"tests/github_contract/gh_cli_git_smart_transport_tests.cpp","detail":"The same test source built against main's product (5239776dc's four adapter sources checked out in the worktree): prints 'both sides 10 gh processes' and fails at 'a cold open of a text file takes one gh process a side ... (ten before [[BPR-1370]])'. At 5744012e0: 'both sides 2 gh processes', and a text read succeeds while every gh api user call fails. macOS arm64, Apple clang, build/dev Debug."}},{"criterion":"BPR-1370:process-count","outcome":"pass","revision":"5744012e0","evidence":{"kind":"command","path":"tests/github_contract/gh_cli_git_smart_transport_tests.cpp","detail":"gh processes per cold open of a text file, counted by a named semaphore the sandboxed fixture gh posts: 10 before, 2 after (sign-in 1 either way); a binary side 6 after. Live on PR #43 (timeline proc.done lines between selection and shown, and until quiet): 10 of 10 every before open, 2 every after open."}},{"criterion":"BPR-1370:mac-live","outcome":"pass","revision":"5744012e0","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1370-a-text-blob-read-names-its-own-account.md#live-cold-open-on-bpr-test43","detail":"Live, read only, MikeMarcin/BPR-Test#43, preparation off, shipped composition offscreen through BPR-1344's probe course on a local unpushed branch at 5744012e0's product, Release tests-on; same binary A/B, interleaved. After: 4 runs, 16 cold opens, median 849 ms, p90 961 ms, range 798-1011 ms, 2 gh processes per open. Before (BPR1370_OBSERVED_ONLY, the pre-change path): 3 runs, 12 opens, median 2313 ms, p90 2618 ms, range 2212-2723 ms, 10 per open. Load average 5-20 (1 min). X-RateLimit-Remaining core 4994 to 4475, GraphQL 4998 to 4888."}},{"criterion":"BPR-1370:rows","outcome":"pass","revision":"5744012e0","evidence":{"kind":"command","path":"tests/github_contract/CMakeLists.txt","detail":"macOS build/dev Debug: gh_cli_git_smart_transport, github_provider, gh_cli_transport, architecture_boundaries, git_protocol_boundary and test_network_isolation_manifest pass. clang-format 22 on touched C++; clang-tidy on the three touched adapter sources reports only two pre-existing naming findings in gh_cli_transport.cpp (lines 55 and 66). verify_documentation.py prints DOCUMENTATION_OK. No src/app or src/application source changed."}}]
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

## macOS station run (2026-10-09)

Product at `5744012e0`. macOS arm64, Apple clang, offscreen. The machine was
shared with other agents.

### Fixture course, before and after

`gh_cli_git_smart_transport` now composes the provider's CLI lease (the
`owned_cli_read_transport` shape: one `gh_cli_transport` over one runner, an
exact Git transport per read) over the fixture `gh`, signs in through
`observe_viewer`, and reads both sides of a modified file. The sandboxed
fixture may not write a file, so it posts a named semaphore on every launch
and the course counts the posts.

| | sign-in | cold open, two text sides | binary side | result |
|---|---|---|---|---|
| before (main's four adapter sources, same test) | 1 | 10 | - | fails at "one gh process a side" |
| after | 1 | 2 | 6 (1 GraphQL, then 5 as before) | passes |

### Live cold open on BPR-Test#43

BPR-1344's probe course (`bench_bpr1344_live_prefetch off`) on a local,
unpushed branch over `5744012e0`'s product, with one probe-only switch,
`BPR1370_OBSERVED_ONLY`, that sends a single read down the pre-change path.
The same Release tests-on binary ran both arms, interleaved. It signs in
through the GitHub CLI, opens `MikeMarcin/BPR-Test#43` (60 files), turns
preparation off, and times four cold opens (`module_05`, `_20`, `_35`, `_50`)
from the selection until the comparison is shown. Read only.

| | runs | opens | median | p90 | range | gh processes per open |
|---|---|---|---|---|---|---|
| before (pre-change path) | 3 | 12 | 2313 ms | 2618 ms | 2212-2723 ms | 10 |
| after | 4 | 16 | 849 ms | 961 ms | 798-1011 ms | 2 |

Per-run medians: before 2323, 2399 and 2284 ms; after 847, 851, 853 and
833 ms. The `gh` process counts are the same until the comparison is shown
and until the window is quiet. BPR-1360's run, at load 9-24, measured
2348 ms for the pre-change path.

A GraphQL text read takes 408-658 ms (median 486 ms); the REST read and an
observation took about 318 ms each. The two sides' reads overlap and never
wait for a process slot (`slot_wait` 0 in every after open; 290-330 ms once
per before open). So an open now costs about one GraphQL round trip plus
350-400 ms of local work, well under the 2.1 s the card aimed at.

Load average (1 minute) before each run: 19.8, 14.2, 9.8, 7.2, 5.1, 5.0 and
6.2. Rate limit from `X-RateLimit-Remaining` response headers before each run:
core 4994 to 4475 over seven runs (sign-in and the pull request read are about
57 a run, and each before run's opens about 40 more); GraphQL 4998 to 4888.
The run stayed far above the 1500 margin.

Not exercised live: no file in the course is binary, so the REST fallback was
covered only by the fixtures.

