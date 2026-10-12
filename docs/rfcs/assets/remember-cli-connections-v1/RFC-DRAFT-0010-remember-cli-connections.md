---
id: "RFC-DRAFT-0010"
title: "Remember the chosen GitHub CLI connection across launches"
status: "Proposed"
---

# Remember the chosen GitHub CLI connection across launches

## Problem and grounded current behavior

On 2026-10-12 the owner reported having to choose CLI again every time they
close and reopen BetterPullReview. They corrected the requested workflow to
author-rfc; this document proposes the change without implementing it. The
reported installed-app behavior was not independently reproduced.

The source explains the reset. In the
[desktop composition](../../../../src/app/src/github_application.cpp),
`register_discovered_cli_account` adds profiles to the runtime's `Profiles`
and `ProfileCatalog`; it does not save the selection. Production startup
constructs native HTTPS profiles. `remember_startup` and `select_startup`
restore device grants, while `dispatch_intent` asks CLI read consent on each
CLI activation. Remembering a server's hostname in the queue does not retain
its transport or chosen CLI account.

The [provider](../../../../src/adapters/github/src/github_provider.cpp)
checks a pinned login against the observed viewer and keeps its first numeric
account ID in the runtime-local `CliPinnedAccounts` map. Recreating that map
after restart does not prove continuity with the previously accepted account.
The proposed persistence therefore needs both the connection choice and its
observed account binding, rather than a global `use_cli` flag.

[RFC-BPR-0015](../../RFC-BPR-0015-github-cli-production-enablement.md)
requires explicit CLI selection and tokenless account pinning.
[The product contract](../../../architecture/product-contract.md#credential-acquisition)
defines restart restoration for device grants but not CLI connections. The
accompanying [proposed amendment](proposed-cli-amendment.md) identifies the
narrow change to those adopted contracts.

## Proposed behavior and boundaries

### Remember a successfully chosen connection

After the reviewer explicitly selects a CLI account, accepts the existing
read-trust confirmation and successfully connects, remember that choice in
the current BPR profile. The confirmation must explain that the choice will
be reused on later launches until the reviewer signs out or replaces it.
Reusing that same choice does not repeat account discovery, account selection
or the read-trust question merely because a window or process was reopened.

Keep at most one selected connection per canonical server in a BPR profile,
following [existing account ownership](../../RFC-BPR-0024-bpr-profiles-multiple-windows.md#accepted-interaction-and-ownership).
The record identifies the CLI transport, canonical host and server contract,
selected login, authenticated numeric account ID, and the stable connection
identity/revision needed by existing authority-scoped private state. Also
retain which resumable connection was most recently selected. Numeric account
identity comes from the successful provider observation, never inventory
labels, saved repository metadata or caller-supplied text. Store no token,
credential copy, live lease, publication grant or discovery inventory.

Discovery alone, a cancelled trust question, failed sign-in or a stale
completion must not create or replace the remembered selection. A successful
explicit replacement changes it atomically. Selecting native sign-in for that
server replaces the CLI startup choice only after native sign-in succeeds;
device grants retain their existing resumable behavior and manual tokens
retain their existing session-only behavior. A failed replacement leaves the
previous durable choice intact.

### Revalidate on profile startup

Restore the remembered connection before startup chooses an authentication
path. Keep the existing selection of one startup host from retained window
destinations and focus order; use that host's saved transport/account rather
than the first profile with a matching hostname. With no retained review
destination, resume the most recently selected resumable connection. Closing
the last window must not erase this choice.

Reconstruct the pinned CLI profile and attempt a bounded connection without
another selection dialog. Run current tool, configuration and process checks,
then authenticate the exact saved host/login through the existing per-call
private configuration. Require the observed numeric account ID to match the
saved binding before accepting a session or exposing account-private Home,
Recent or saved review content. The record authorizes this validation attempt;
it does not establish a signed-in session or offline reading authority.

Preserve connection identities across restart without aliasing a different
host, account or transport when configuration or discovery order changes.
Apply existing exact-account reading restoration and cancellation rules after
session acceptance. Opening a link for a server with a remembered connection
uses the same selection and validation path. Multiple windows share the
profile's connection work; opening a second window does not independently
rediscover accounts or ask the same trust question.

Do not enumerate all installed CLI accounts on startup. Full inventory stays
behind the existing explicit CLI action or refused-session refresh. A fresh
profile, or an upgrade with no saved CLI record, keeps current native defaults
and requires the first explicit CLI selection. Old recents or saved server
names cannot manufacture past consent.

### Failure, removal and durability

Unavailable tools, an unsupported platform, a signed-out or mismatching
account, and failed validation leave the connection unaccepted with an
actionable reason through existing connection status and recovery controls.
Keep the saved choice for retry; never substitute the active CLI account,
native credentials or public preview. A later ordinary launch may retry it
under the same bounds. Account replacement requires explicit selection and
successful sign-in.

Sign out retires current authority and durably disables automatic reuse for
that server in this BPR profile, including when a device grant also remains
stored there. Forget server removes its remembered choice through the existing
forget operation. Neither action signs out of `gh` or changes its global
account/configuration. Late connection or persistence completions cannot
undo either action. Other BPR profiles retain their own choices.

Persist successful selection promptly, independently of window-layout saving,
and drain admitted writes on orderly Quit. Transactional recovery yields the
old complete record or the new complete record. If selection cannot be saved,
the current connection may remain usable, but report that it is session-only.
If disabling reuse cannot be saved, report that failure rather than claiming
it will stay signed out after restart. Unreadable, malformed or newer storage
must not authorize CLI execution; leave a usable signed-out application and
report the storage problem without silently resetting the data.

### Existing authority and implementation scope

Reuse [RFC-BPR-0015's per-account configuration](../../RFC-BPR-0015-github-cli-production-enablement.md#owner-per-account-configuration-amendment--2026-10-09)
and its supported-platform, tool, process, token and separate publication
confirmation rules. Remembered read consent grants no publication authority.
The Windows/Linux CLI limitation remains. This proposal changes persistence
and restoration, with no account-page layout or visual redesign.

The core decides restoration, replacement and stale-result admission from
explicit facts. The application host owns connection metadata, serialized
effects and lifecycle; adapters own storage and CLI execution; Qt presents
existing intents and typed status. Concrete types and schema belong to
implementation. This metadata must participate in the profile's existing
maintenance inventory and purge, without touching CLI-owned credentials.

No material question remains open for drafting. Automatic revalidation is the
proposed meaning of remembering the owner's choice; it is not a claim of
implementation, runtime acceptance or broader platform qualification.

## Acceptance

- **AC01:** In a composed macOS course, choose a CLI account, accept read
  trust and connect, then quit and start a new process on the same profile.
  It accepts the same host/login/numeric account without discovery, account
  selection or another read-trust question. Repeat after explicitly closing
  the last window, with no retained review destination.
- **AC02:** With two CLI accounts on one host, a native profile on that host
  and a second server, restart and exact-host PR navigation follow the saved
  choices despite changed discovery/configuration order and global CLI active
  account. Private content and connection identities remain correctly scoped;
  a sibling window shares the accepted connection.
- **AC03:** A missing or mismatching saved account, unavailable tool,
  unsupported platform or failed validation cannot accept another authority
  or expose its private state. The failure is visible, the choice survives,
  and a subsequent successful retry restores that exact connection.
- **AC04:** Sign out, Forget server and successful replacement have their
  defined restart results, including a previous device grant and late
  completions. Cancellation or unsuccessful replacement preserves the prior
  durable choice; another BPR profile is unaffected.
- **AC05:** Real storage reopen, interrupted writes, refused saves and
  refused loads demonstrate the defined durability and session-only behavior.
  Fresh and upgraded profiles without a selection do not run CLI implicitly.
  Profile maintenance accounts for the new metadata.
- **AC06:** Fixture canaries demonstrate that persistence and restored
  execution retain the existing tokenless boundary and global CLI state;
  restored read consent cannot bypass ordinary publication confirmation.
