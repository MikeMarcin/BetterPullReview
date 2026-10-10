---
schema: "implement-rfc.card.v1"
id: "BPR-1373"
title: "Pin each gh call to a chosen account, and list every gh account on Accounts & hosts"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"owner_request":{"date":"2026-10-09","channel":"chat","quotes":["pin each gh call to a chosen account, and let the Accounts tab recognise every gh-authenticated account"]},"owner_decision":{"date":"2026-10-09","channel":"chat, relayed by the coordinator","quote":"Go with B","amendment":"docs/rfcs/RFC-BPR-0015-github-cli-production-enablement.md#owner-per-account-configuration-amendment--2026-10-09"},"reservation":"docs/issues/_reservations/BPR-1373--BPR-1373--6ccd2111f21d.json","related_cards":["BPR-1370","BPR-0909","BPR-0913","BPR-0914","BPR-0915"],"related_rfcs":["RFC-BPR-0015","RFC-BPR-0024","RFC-BPR-0032"],"filed_by":"rfc-implementer, at the coordinator's request"}
parked_on: "human-review"
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

## Implementation (2026-10-09)

- Adapter: `gh_cli_account_pin`, `pin_gh_cli_configuration` and `run_gh_cli`
  (`gh_cli_transport`) make the per-call private directory. The transport's
  API, probe and mutation launches and the Git transport's blob, version,
  help and clone launches go through it. A pinned transport no longer
  observes the user's global `config.yml` and `hosts.yml`.
  `compile_gh_cli_auth_inventory_request` and `parse_gh_cli_auth_inventory`
  read the full inventory. `github_host_profile_candidate::CliAccountLogin`
  holds the login, with `valid_cli_account_login`. At CLI sign-in the
  provider refuses a viewer whose login is not the profile's, or whose ID
  differs from the first one bound for that profile in this run, as an
  account mismatch.
- Application: discovery reads the full inventory and registers one CLI
  profile per signed-in, keychain-held host and login. `make_cli` pins the
  configuration for that profile. A refused pinned session runs the inventory
  again once. A process-wide registry in the composition root records which
  BPR profile uses which pinned account, for "Used by".
- Qt: each server card lists every `gh` account (`server_card_cli_account`),
  with Use for this profile, Used here and Check gh again. A refused session
  on an account `gh` has signed out shows as signed out. In the sign-in
  dialog, the single CLI offer stays ([[BPR-0803]]) unless the host has more
  than one usable account. `frontend-design:frontend-design` was loaded
  before this work.

## Verification (macOS arm64, Apple clang, Release tests-on, `build/rel`)

- `gh_cli_transport`: the full inventory (every account, state, token source;
  a duplicated, doubly active or misfiled account fails). Pins are refused for
  a bad host or login. The private directory is 0500 with two 0400 files,
  `hosts.yml` holds exactly the host and login, and the directory is gone
  after the launch. A launch already carrying `GH_CONFIG_DIR`, or one whose
  directory cannot be made, is refused and never runs unpinned. After a
  global `hosts.yml` switch, the unpinned transport refuses and the pinned
  one reads.
- `gh_cli_git_smart_transport`, over the real runner, sandbox and a fixture
  `gh` holding three accounts and keychain canaries:
  - A pinned lease reads as `tester` while the active account switches among
    `other`, `tester` and `owner`.
  - The clone runs pinned: the fixture refuses a clone acting as another
    account, and that refusal was shown to fire.
  - An unpinned lease is refused at the switch (the behaviour before).
  - After `tester` leaves the keychain, the silent fallback is refused on a
    read (the lease is revoked and refused) and at sign-in (account mismatch).
  - A login now bound to another ID is refused.
  - No private directory is left behind.
  - Of 18 recorded lease launches, none carries a canary or a token variable
    in its arguments, environment, input or output. The child itself checks
    every launch for token variables, canaries and directory modes.
  - The diagnostic log's events are typed and have no text field, so no
    token can reach them.
- Mutation check: with `run_gh_cli` made to ignore the pin, the course fails
  at "the CLI sign-in observes the fixture account".
- Also passing: `github_provider`, `github_host_profile`,
  `architecture_boundaries`, `git_protocol_boundary`,
  `test_network_isolation_manifest`, `application_device_flow_cli_choice`,
  `application_setup_destination_public_path`, `qt_composed_cli_read_consent`,
  `qt_composed_home_sign_in`, and `qt_workbench_window_scale_100` and `_200`
  (the new account-list rows, sentences and choice). clang-format 22.1.8 on
  every touched C++ file.
- Already failing on main, not this change: `application_inline_publish_cli_consent`
  fails at "the new comment is written in the drawer", both here and on
  origin/main `cc4f834d9` built separately the same way.
- Live, read only, 2026-10-09: one GraphQL read of `MikeMarcin/BPR-Test`
  per account. Each ran through the launch shape `run_gh_cli` produces (a
  private 0500 directory, two 0400 files, the product's environment and
  write-deny sandbox), replicated in the shell rather than run from the
  product binary. Viewer `MikeMarcin` (`X-RateLimit-Remaining` 4830) and
  viewer `MikeMarcinTest` (4994, its own budget). `gh auth status --active`
  was `MikeMarcin` before and after. No token was printed or handled, and
  both directories were removed.

## Pending

- Owner look at the Accounts & hosts account list: Accept or Reject. Not
  frozen in this round: the owner was low on usage and asked for a freeze
  only if cheap. The freeze script has no Settings opening yet, and the app
  has no flag to seed a fixture inventory for the look.
