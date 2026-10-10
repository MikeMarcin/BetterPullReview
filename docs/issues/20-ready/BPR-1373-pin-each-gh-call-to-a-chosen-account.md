---
schema: "implement-rfc.card.v1"
id: "BPR-1373"
title: "Pin each gh call to a chosen account, and list every gh account on Accounts & hosts"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"owner_request":{"date":"2026-10-09","channel":"chat","quotes":["pin each gh call to a chosen account, and let the Accounts tab recognise every gh-authenticated account"]},"owner_decision":{"date":"2026-10-09","channel":"chat, relayed by the coordinator","quote":"Go with B","amendment":"docs/rfcs/RFC-BPR-0015-github-cli-production-enablement.md#owner-per-account-configuration-amendment--2026-10-09"},"reservation":"docs/issues/_reservations/BPR-1373--BPR-1373--6ccd2111f21d.json","related_cards":["BPR-1370","BPR-0909","BPR-0913","BPR-0914","BPR-0915"],"related_rfcs":["RFC-BPR-0015","RFC-BPR-0024","RFC-BPR-0032"],"filed_by":"rfc-implementer, at the coordinator's request"}
---

# BPR-1373: Pin each gh call to a chosen account

## Request

The owner (chat, 2026-10-09): pin each `gh` call to a chosen account, and let
the Accounts tab recognise every `gh`-authenticated account. This Mac's `gh`
holds two github.com accounts, `MikeMarcin` (active) and `MikeMarcinTest`.
Accounts & hosts lists each `gh` account as its own entry (login, host, `gh`
as the source, which profiles use it), and a profile can choose one. A
signed-out account shows its profile signed out instead of falling back to the
active account. [[BPR-1370]]'s viewer check stays the proof.

## Decision

The owner first asked for `GH_TOKEN` from `gh auth token --user`. That
conflicts with RFC-BPR-0015's 2026-09-13 amendment ("Do not extract, print,
pass, log, persist or copy the CLI token; do not run `gh auth switch`,
`gh auth token` or a local listener") and with the architecture documents and
adapter `AGENTS.md` that restate it. This card was filed Blocked with two
options. The owner chose the tokenless one (chat, 2026-10-09: "Go with B"),
recorded as RFC-BPR-0015's
[2026-10-09 owner amendment](../../rfcs/RFC-BPR-0015-github-cli-production-enablement.md#owner-per-account-configuration-amendment--2026-10-09).
The token rule stands. A per-call private `GH_CONFIG_DIR` is the one
addition.

## Probe that chose the design (2026-10-09)

On this Mac, with `gh` 2.101.0 under the product's exact `sandbox-exec`
write-deny profile, a GraphQL read of `MikeMarcin/BPR-Test` with
`viewer { login }`:

| Configuration | Viewer | `X-RateLimit-Remaining` | Global active account after |
|---|---|---|---|
| none (ambient) | `MikeMarcin` | 4852 | `MikeMarcin` |
| private, `user: MikeMarcinTest` | `MikeMarcinTest` | 4994 (its own budget) | `MikeMarcin` |
| private, `user: BprNoSuchAccount` | `MikeMarcin` | 4851 | `MikeMarcin` |

Nothing was written to the private directory. In the third row, `gh` falls
back silently to the unkeyed (active) token when the named login has no
keychain entry, and its own `gh auth status` reports the missing login as
`success`. Only the viewer check catches that.

## Design

- **Per-call private configuration (adapter).** A
  `validated_gh_cli_configuration` may carry a pin (host and login, checked to
  GitHub's login alphabet). `run_gh_cli` launches every `gh` process of a
  pinned configuration. It creates a fresh directory under the configuration's
  neutral working directory, writes `config.yml` (`version: "1"`) and a
  `hosts.yml` naming only that host and login, makes the files and directory
  read-only and owner-only (0400 and 0500), adds `GH_CONFIG_DIR` to that child's
  environment alone, runs it, and removes the directory. If any step fails,
  the launch is refused without running `gh`. An unpinned configuration runs
  exactly as before. The transport's API, capability-probe and mutation calls
  and the Git transport's blob reads, version and help probes and clone all go
  through it, so the clone's `gh auth git-credential` helper inherits the pin.
  Local Git never does.
- **No ambient configuration for a pinned call.** A pinned child reads only
  the private pair, so the transport's observation of the user's global
  `config.yml` and `hosts.yml` (which a `gh auth switch` changes) no longer
  applies to it. The executable and working-directory checks remain.
- **Inventory.** `gh auth status --json hosts`, without `--active`, admits every
  account per host: host, login, active, signed in (`state` is `success`) and
  keychain-held (`tokenSource` is `keyring`). It is bounded as the active
  inventory is (64 KiB, 64 hosts, 128 accounts). The
  owner's fallback that parses only login names is not built: the RFC's 2.96
  floor guarantees `--json`, so older `gh` is refused before account status
  runs, as today.
- **Binding.** A CLI host profile may name its account login
  (`github_host_profile_candidate::CliAccountLogin`, allowed only on the CLI
  transport). Discovery registers one CLI profile per signed-in, keychain-held
  host and login. At sign-in the provider refuses, as an account mismatch, an
  observed viewer whose login is not the profile's. It also refuses one whose
  numeric account ID differs from the first ID it bound for that profile in
  this run. The lease then holds every read to that ID and login
  ([[BPR-1370]]).
- **Signed out.** When the pinned login has left `gh`, `gh` silently uses the
  active token. The viewer check refuses the sign-in or read, and the
  application re-runs the inventory. A login that is missing, not signed in or
  not keychain-held shows "Signed out in gh" in place of the account mismatch.
- **Accounts & hosts.** Each server card gains a "GitHub CLI accounts" list:
  one row per `gh` account (letter avatar, login, "via gh" and "active in gh"
  where true, the BPR profiles in this run that use it, and Use for this
  profile). A signed-out or file-stored account is listed but cannot be
  chosen. Choosing goes through the existing CLI consent and connection.

## Acceptance

- Fixture `gh` with two accounts and a switchable active account: a pinned
  lease stays on its account while the active account switches between
  calls. An unpinned lease over the same fixture is refused at the switch (the
  before behaviour).
- The silent fallback (pinned login without a keychain entry) is refused at
  sign-in and on a read, and refuses the lease.
- A canary token held only by the fixture's keychain never appears in any
  launch's arguments, environment or standard input, in any output the app
  keeps, or in the diagnostic log. No `GH_TOKEN`-family variable is ever set.
- The clone runs pinned: the fixture refuses a clone not pinned to the
  profile's account.
- Each private directory is owner-only and read-only while `gh` runs and is
  gone afterwards.
- Live, read only, on `MikeMarcin/BPR-Test`: read as each account while
  `gh auth status --active` stays `MikeMarcin`, with `X-RateLimit-Remaining`
  checked.
- Owner look at Accounts & hosts: Accept or Reject.
- `DOCUMENTATION_OK`; clang-format 22 on touched C++.
