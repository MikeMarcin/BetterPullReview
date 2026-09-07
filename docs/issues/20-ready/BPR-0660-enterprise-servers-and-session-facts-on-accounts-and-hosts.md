---
schema: "implement-rfc.card.v1"
id: "BPR-0660"
title: "Enterprise servers and session facts on Accounts & hosts"
status: "Ready"
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
