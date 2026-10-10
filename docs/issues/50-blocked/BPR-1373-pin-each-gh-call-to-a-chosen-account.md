---
schema: "implement-rfc.card.v1"
id: "BPR-1373"
title: "Pin each gh call to a chosen account, and list every gh account on Accounts & hosts"
status: "Blocked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
blocked_by: "rfc-bpr-0015-cli-token-rule"
metadata: {"owner_request":{"date":"2026-10-09","channel":"chat","quotes":["pin each gh call to a chosen account, and let the Accounts tab recognise every gh-authenticated account"]},"reservation":"docs/issues/_reservations/BPR-1373--BPR-1373--6ccd2111f21d.json","related_cards":["BPR-1370","BPR-0909","BPR-0913","BPR-0914","BPR-0915"],"related_rfcs":["RFC-BPR-0015","RFC-BPR-0024","RFC-BPR-0032"],"filed_by":"rfc-implementer, at the coordinator's request","conflict":{"rfc":"RFC-BPR-0015","section":"Product-owner amendment, 2026-09-13","text":"Do not extract, print, pass, log, persist or copy the CLI token; do not run `gh auth switch`, `gh auth token` or a local listener.","also":["docs/architecture/github-provider-contract.md","docs/architecture/github-host-implementation.md","docs/architecture/security-threat-model.md","src/adapters/github/AGENTS.md"]}}
---

# BPR-1373: Pin each gh call to a chosen account

## Request

The owner (chat, 2026-10-09): pin each `gh` call to a chosen account, and let
the Accounts tab recognise every `gh`-authenticated account. This Mac's `gh`
holds two github.com accounts, `MikeMarcin` (active) and `MikeMarcinTest`.

Asked for:

- Enumerate `gh` accounts per host from `gh`'s machine-readable output, with a
  fallback that parses only login names.
- Bind a profile's `gh` account by host, login and numeric account ID.
- Run each `gh` call with `GH_TOKEN` from
  `gh auth token --hostname <h> --user <login>`, held in memory and passed only
  in that child's environment. It must never appear in argv, logs or storage,
  and is either scoped to the lease or fetched per call.
- Keep [[BPR-1370]]'s viewer check as the proof. A signed-out account shows
  its profile signed out and does not fall back to the active account.
- A `gh` too old for `--user` is detected, reported plainly, and falls back to
  today's active-account behaviour with its checks.
- Accounts & hosts lists each `gh` account as its own entry (login, host, `gh`
  as the source, which profiles use it), and a profile can choose one. The
  owner reviews the look in attended review.

## Conflict with RFC-BPR-0015 (why this card is Blocked)

The requested mechanism is forbidden by RFC-BPR-0015's current, binding text,
the 2026-09-13 owner amendment that enabled the CLI profile:

> Invoke directly with an argument array, never a shell. Construct a narrow
> noninteractive child environment, choose the exact HTTPS hostname, use a
> neutral non-repository directory and bound input/output/time/cancellation.
> Do not extract, print, pass, log, persist or copy the CLI token; do not run
> `gh auth switch`, `gh auth token` or a local listener.

Its acceptance criterion also requires: "Token and private response content
remain absent from parent arguments, environment, logs, diagnostics,
settings, SQLite and committed evidence". Under `GH_TOKEN`, the token stays
out of the parent's environment, but the parent reads it from
`gh auth token`'s output and passes it on, which the amendment forbids. The
same prohibition is restated in `docs/architecture/github-provider-contract.md`,
`github-host-implementation.md` and `security-threat-model.md` ("`gh auth
token --user` reveals the token and is prohibited"), and the adapter's
`AGENTS.md` ("Never read or pass token ... variables").

RFC-BPR-0024 ("one account per server per profile"; "Different BPR profiles
may use different GitHub.com accounts at the same time") and RFC-BPR-0032
("One account per server" per card) agree with "a profile chooses one", so
they do not conflict. Listing several `gh` accounts within one server card
changes RFC-BPR-0032's accepted look, which the attended review covers.

## Options

### A. The owner's mechanism: `GH_TOKEN` from `gh auth token --user`

Each `gh` child gets `GH_TOKEN` (github.com) or `GH_ENTERPRISE_TOKEN` (a GHES
host). `gh` reads `GH_TOKEN` only for github.com and `*.ghe.com`, so a GHES
profile needs the enterprise variable. The token comes from a separate
write-denied `gh auth token --hostname <h> --user <login>` child, whose stdout
the runner returns only on a clean exit. It is held in a wiped buffer, copied
only into the next child's environment entry, and kept out of every
diagnostic, completion and log. `gh repo clone` passes the variable on to Git
and to its own `gh auth git-credential` helper, so the clone is pinned too.

- **Lifetime: fetched per call.** Each request runs one extra local
  `gh auth token` process, with no network. A `gh auth logout` or
  `gh auth refresh` then takes effect at the next call, since `gh auth logout`
  does not revoke the token on the server. No secret outlives one child launch.
  The cost is one more process per request: 0.5-1.1 s here under load average
  169, so unmeasured at a normal load. That cost lands on [[BPR-1370]]'s
  two-process cold open, though the token fetches for the two sides can run
  side by side. A lease-scoped token would cost nothing per call but keeps a
  revocable secret in memory for the session. A signed-out account would also
  keep working until the lease ends, which contradicts "shows signed out".
- **Signed out.** `gh auth token --user X` fails when `X` has no credential.
  The profile is then marked signed out, with no fallback.
- **Old `gh`.** A closed `gh auth token --help` probe, alongside the existing
  `api`, `repo clone` and `auth git-credential` probes, checks for `--user`.
  Without it, the profile reports "this gh cannot select an account" and keeps
  today's active-account path and checks. Today's 2.96 floor already includes
  `--user` (added in gh 2.40), so with an admitted `gh` this path is reachable
  only through a fake.
- **Residual risk.** On macOS, while the child runs, another process running
  as the same user can read its environment. That user can also run
  `gh auth token` directly.

### B. Without the token: a private `GH_CONFIG_DIR` naming the account

Each pinned account gets an app-owned, read-only configuration directory: a
`config.yml` (`version: "1"`, so no migration) and a `hosts.yml` naming only
that host and login as `user`. Each `gh` child for the profile receives
`GH_CONFIG_DIR` pointing there. `gh` resolves the token itself, from the
keychain entry for that login, so BetterPullReview never reads, holds or
passes a token. The write-deny sandbox is unchanged, and the clone's helper
inherits the variable.

Probed on this Mac, 2026-10-09, `gh` 2.101.0, under the product's exact
`sandbox-exec` write-deny profile, reading `MikeMarcin/BPR-Test` through
GraphQL with `viewer { login }`:

| Configuration | Viewer | `X-RateLimit-Remaining` | Global active account after |
|---|---|---|---|
| none (ambient) | `MikeMarcin` | 4852 | `MikeMarcin` |
| private, `user: MikeMarcinTest` | `MikeMarcinTest` | 4994 (its own budget) | `MikeMarcin` |
| private, `user: BprNoSuchAccount` | `MikeMarcin` | 4851 | `MikeMarcin` |

Nothing was written to the private directory. The third row matters. When the
named login has no keychain entry, `gh` silently falls back to the unkeyed
(active) token, and its own `gh auth status` reports the missing account as
`success`. So under B:

- **Proof.** [[BPR-1370]]'s viewer check is the only thing that catches the
  fallback, and it does: viewer `MikeMarcin` is not the bound
  `MikeMarcinTest`, so the read is refused and the lease is refused for good.
- **Signed out.** This comes from the global inventory
  (`gh auth status --json hosts` with the user's own configuration), checked
  before a lease is made and after any refusal. A login that has left the
  inventory shows its profile signed out rather than "account changed".
- **Old `gh`.** The multi-account `hosts.yml` layout this relies on dates from
  gh 2.40, and the 2.96 floor already requires it. Any admitted `gh` therefore
  supports it, and the fallback is reachable only through a fake, as in A.
- **What changes.** The child's ambient `config.yml` no longer applies to
  pinned calls. That removes the user's `http_unix_socket` and other settings
  from those calls, closing one documented trust gap for them. A keychain
  stored `insecure` (token in plain `hosts.yml`) cannot be pinned this way; the
  inventory's `tokenSource` identifies those accounts, and they are refused
  plainly.
- **Rules it still touches.** The adapter `AGENTS.md` rule against passing
  "config-directory" variables, and arguably RFC-BPR-0015's "does not ...
  alter `gh` account selection" for the clone helper (written about the
  global selection). It respects the RFC's central rule that the app "neither
  receives nor extracts the token".

### Recommendation

B. It pins every call, including the clone's helper, without the app ever
touching a token, and it costs no extra process per call. It still needs the
owner's word on the `GH_CONFIG_DIR` point and the matching edits to the RFC
amendment, architecture documents and adapter `AGENTS.md`. A needs an explicit
owner amendment to RFC-BPR-0015, plus the same documentation edits.

## Design shared by both options

- **Inventory.** `gh auth status --json hosts`, without `--active`, admits
  every account per host as `{host, login, active, state, tokenSource}`,
  bounded as today (64 hosts, 128 accounts, 64 KiB). No token is requested.
  The fallback, for a `--json` failure, reads `hosts.yml`'s `users` keys only,
  through a bounded parser that rejects any other key under a user. Today's
  `--active` call is kept for the Use GitHub CLI gesture.
- **Binding.** A CLI host profile gains a bound account `{hostname, login,
  database ID}`, persisted in the profile catalog with no credential. The
  database ID comes from the first pinned `viewer` read. One CLI profile per
  host and login, instead of today's one per host.
  `register_discovered_cli_account` keys on both.
- **Lease.** `cli_observed_account_transport` receives the bound account at
  construction, not just the first observation, so a pinned call answered by
  another account is a mismatch from the first read. Every other check stays
  as it is.
- **Accounts & hosts.** In each server card, a "GitHub CLI accounts" group has
  one row per `gh` account: letter avatar, login, "via gh" with "active in gh"
  where true, the BPR profiles that use it, and a Use for this profile action.
  The current profile's chosen account is marked. A signed-out account shows
  "Signed out in gh" and is not selectable. Visual work loads
  `frontend-design:frontend-design` first, and the owner looks at it in a
  short attended session.

## Acceptance (to be confirmed once the conflict is settled)

- Fixture `gh` with two accounts: a call stays pinned while the fixture's
  active account switches between calls. This fails before the change, since
  today's lease refuses on the switch.
- No token in any process argument, diagnostic, log or completion, asserted
  with a canary. Under A, the canary appears only in the child's environment.
- Signed out: the profile shows signed out, and no call runs as the active
  account.
- Old `gh`: detected, reported, and falls back to active-account behaviour
  with its checks.
- Live, read only, on `MikeMarcin/BPR-Test`: read as each account while
  `gh auth status --active` stays `MikeMarcin`, with `X-RateLimit-Remaining`
  checked.
- Owner look at Accounts & hosts: Accept or Reject.
- `DOCUMENTATION_OK`; clang-format 22 on touched C++.

## Probe hygiene

The probes used scratch configuration directories outside the repository,
since removed. No token was printed, logged or written: the `gh auth token`
timings discarded their output to `/dev/null`, and printed account IDs were
masked.
