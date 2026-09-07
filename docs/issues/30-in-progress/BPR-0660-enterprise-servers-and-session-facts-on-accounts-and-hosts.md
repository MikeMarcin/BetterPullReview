---
schema: "implement-rfc.card.v1"
id: "BPR-0660"
title: "Enterprise servers and session facts on Accounts & hosts"
status: "In Progress"
rfcs: ["RFC-BPR-0028","RFC-BPR-0018"]
depends_on: []
priority: "P2"
---

# Enterprise servers and session facts on Accounts & hosts

RFC-BPR-0028's Accounts & hosts page draws one card per server: for a
device grant "signed in on <date>, renews itself; remembered on this Mac"
with Sign out; for an enterprise server the hostname, "signed in with a
token for this session only", and a "Remember this server" switch that
keeps the hostname and account across launches; and an "Add enterprise
server" action. [BPR-0652](../60-done/BPR-0652-settings-pages-accounts-appearance-reviews.md)
placed the account card with what the session projection carries (login,
display name, host, the connection state and the rate limit) and left the
rest, because none of it has an authority: `github_session_projection`
carries no grant kind, date or remembered fact (`accepted_device_lifetime`
lives in the host), the GitHub intents are activate, deactivate, open and
refresh with no sign-out, and host profiles come from configuration with
no durable record to remember a server into.

## Scope

- Project the credential kind (device grant or token), the grant's date
  and self-renewal, and the remembered fact onto the session projection.
- A sign-out intent through the GitHub host that revokes the accepted
  session and its stored grant.
- A durable server record (hostname, account name) with add, remember and
  forget intents, listed on the page as enterprise server cards.
- Owned paths: `src/application` GitHub host and projections, the vault
  and profile stores under `src/adapters`, `src/adapters/qt` account card,
  focused tests and the device-authorization contracts.

## Acceptance

- The card states the grant kind, date and remembered fact from the
  projection; Sign out revokes through the host; an enterprise server
  added and remembered survives a restart without its token.

## Notes (2026-09-07)

- **Grant facts.** `github_session_projection` gained an optional
  `github_grant_projection`: the credential kind (a device grant or a
  token), the date it was accepted, whether it renews itself and whether
  it is remembered on this machine. The GitHub host builds it from what it
  already held (`CredentialProvenance`, the accepted device lifetime and
  the durable binding) and publishes it with the session. Durability lands
  after the session is published, so the host republishes the accepted
  session once the vault confirms the binding is durable; without that the
  card said "for this session only" about a grant that had just been
  remembered. The account card states it in one line, and a token session
  says plainly that it lasts for this session only.
- **Sign out.** The card carries Sign out while a session is accepted, and
  it dispatches the deactivation intent the session panel already had, so
  the accepted session and its stored grant go together. No new intent: the
  core already decided what signing out means.
- **Kept servers.** A remembered enterprise server is queue state, not
  credential state: `application::saved_server` is a hostname and an
  account name, validated as a hostname (no scheme, no path, no port) and
  a bounded login, and it is stored in the queue database behind schema
  five, one row per hostname, remembering again replacing the account. The
  queue host stores before it shows, over its own pending flag, so a
  refused store never reaches the page. Accounts & hosts lists what the
  projection carries and offers Remember and Forget. At the next launch
  the application reads the kept servers before it composes the provider
  and registers each unknown hostname as a GHES 3.18 native-HTTPS profile,
  so the server is offered in the host picker. No token is kept anywhere on
  this path: signing in there is still the device flow.
- **Evidence.** `application_github_host` (the grant projection after the
  binding becomes durable), `qt_composed_github` (the card's grant line and
  Sign out dispatch), `sqlite_queue_enrollment_storage` (a version-four
  database migrates, keeps its saved repositories, and saved servers round
  trip through the database and the application port),
  `application_review_queue_enrollment` (restored, published, stored before
  shown, replaced by hostname, and refused outside the vocabulary),
  `qt_workbench_window_scale_100` (the editor on Accounts & hosts, listed
  from the projection, trimmed and lowercased intents, Forget by
  selection), and the new `application_saved_server_contract`: over
  github-com and ghes-3.18 the seed run remembers `ghe.kept.example` on the
  page and the reuse run, over the same application data root, finds it
  kept and offered as a host profile.
- **Negative mutations.** Erasing a server as a no-op fails the storage
  test; the host accepting any hostname fails the enrollment test; the
  window keeping the typed case fails the window test; and dropping the
  profile registration in the application, which no unit test reaches,
  fails the contract's reuse run with `offered=0`.
- **Follow-up.** `bpr_bound_automated_run` for the two-phase settings
  contracts declares the run count of one fixture: this contract declares
  four runs (two fixtures, two phases) and needed a 240 s CTest timeout,
  while `application_review_behaviour_contract` and
  `application_overview_since_review_contract` declare two runs each and
  keep a 120 s timeout that four 40 s budgets could exceed on a slow host.
  Carded separately; their runs take about 25 s each here.
