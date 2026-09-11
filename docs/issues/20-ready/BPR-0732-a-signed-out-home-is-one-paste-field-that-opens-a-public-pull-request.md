---
schema: "implement-rfc.card.v1"
id: "BPR-0732"
title: "A signed-out Home is one paste field that opens a public pull request"
status: "Ready"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0026"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","supersedes_scope_of":"BPR-0716"}
---

# A signed-out Home is one paste field that opens a public pull request

The owner, 2026-09-11, asked what on Home is "very messy"
([BPR-0716](../10-backlog/BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md)):
"a whole bunch of stuff on the bottom that doesn't have any use without an
account signed in. Review queue doesn't make sense when you don't have an
account signed in. Open a pull request by link doesn't work without an
account. Clicking read without signing in leads to another confusing page,
there's no reason for a separate screen at all just let me open the link in
anonymous read-only mode without an account from that first paste link."
Two screenshots of the signed-out dark Home accompany it (the initial
state and the state after "Read without signing in"); their visible text
is transcribed on the [BPR-0593](../10-backlog/BPR-0593-close-rfc-0018.md)
closeout under the d10 continuation.

## What is established

- The signed-out Home (`landing_state::sign_in`,
  `src/adapters/qt/src/workbench_landing.cpp:98-164`) promises in its own
  body text: "To open a public pull request without signing in, paste its
  link in the bar below." The bar does not do that. Return or Open on the
  bar sends `application::submit_setup_destination`
  (`src/adapters/qt/src/workbench_window.cpp:1307-1308, 5109-5114`); the
  app cancels restoration and calls `SetupDestination->submit`
  (`src/app/src/github_application.cpp:5650-5652`); `resolve`
  (`src/application/src/setup_destination.cpp:136-182`) reaches
  `core::resolve_setup_destination` (`src/core/src/setup_destination.cpp:6-21`),
  which is `ready` only when the matching profile observation is
  `Authenticated`, and that flag is `accepted_credential_authority()` plus
  an accepted session (`github_application.cpp:8648-8658`). The authority
  is "absent for every public-preview session"
  (`src/application/include/bpr/application/github_application_host.hpp:243-252`).
  So the bar answers "Sign in to continue to github.com" with a "Sign in
  and continue" button, before and after "Read without signing in"; even
  the final `open()` refuses without an authority
  (`github_application.cpp:8820-8826`).
  `tests/end_to_end/setup_destination_public_path_tests.cpp:95-133` pins
  that refusal as the behaviour. The owner's "doesn't work without an
  account" is a confirmed defect, not a misreading, and BPR-0716's "the
  bar under the card does open a pasted link" was wrong.
- The only signed-out open that works is the session panel on Settings >
  Accounts and hosts, which dispatches `core::open_pull_request_intent`
  directly (`src/adapters/qt/src/github_session_panel.cpp:515`); that is
  the route the public-preview courses and the owner's v006 launches used.
- "Read without signing in" (`workbench_window.cpp:782-795`) sends
  `activate_host_profile_intent{Mode=public_preview}`; the resulting
  session counts as signed in for the landing, so Home flips to
  `choose_pull_request` ("Choose from the queue" / "Paste a link") and the
  pull-request stack's empty page appears ("Enter an owner, repository,
  and pull request number above." with "Copy message", `:1621, :1642,
  :4768-4775`, text left over from the Settings locator, BPR-0669). That is
  the "another confusing page".
- Nothing ties the Review queue heading, its list or its six filter combos
  (`build_queue_landing`, `workbench_window.cpp:1738ff`, combos
  `:1785-1839, 1841ff`) to an account; they are built and shown in every
  landing state. The queue is a signed-in surface (RFC-BPR-0026 "Home and
  attention" is written for a viewer).

## Scope

- Signed out, Home shows the sign-in card, the one paste field and its
  Open button, and the status bar. The Review queue heading, list and
  filter row are not shown until a session with an account exists (a
  public-preview session has none). "Accounts & hosts settings" stays.
- The paste field opens a public pull request anonymously from the first
  Home, in one gesture: a pasted link or `owner/name#123` with no account
  activates the selected profile in public preview (the same intent
  "Read without signing in" sends today) and opens the destination, with
  no intermediate landing state and no "Sign in and continue" stop.
  Decide where the anonymous branch belongs: the core's
  `resolve_setup_destination` gaining a `public_preview` stage that the
  host services by activating preview then opening, or the observation
  carrying "preview available" beside `Authenticated`. The core decides;
  say why in the code. Sign-in is still offered on the card for anyone
  who wants their queue; an unknown server still goes through Add server.
- "Read without signing in" as a separate button and the
  `choose_pull_request` detour for a signed-out reviewer go away; the
  card's body text says what the field does. A GHES profile that refuses
  public preview keeps its refusal text where the field is.
- The pull-request stack's empty page is not shown on Home at all, and its
  text is corrected to name the Home bar where it is still reachable
  (BPR-0669's leftover).
- This card carries BPR-0716's scope (one visible control on Home in
  every landing state that opens without an account and says so);
  BPR-0716 depends on it and is finished from its own acceptance when the
  owner accepts this one. The `back_to_review` state keeps the same field
  and the same anonymous open.
- Owned paths: `src/adapters/qt/src/workbench_landing.cpp` and header,
  `src/adapters/qt/src/workbench_window.cpp`,
  `src/application/src/setup_destination.cpp` and header,
  `src/core/src/setup_destination.cpp` and header,
  `src/app/src/github_application.cpp` (the setup-destination and
  public-preview wiring only), their tests, RFC-BPR-0028's Home paragraph.

## Acceptance

- `core_setup_destination` (or the core selector that holds
  `resolve_setup_destination`) pins the anonymous stage for a known
  public host with no account and the `sign_in` stage kept for a host
  that refuses preview; it fails at HEAD.
- `tests/end_to_end/setup_destination_public_path_tests.cpp`: a pasted
  public link with no account reaches `pull_request_ready` in public
  preview with no sign-in step and no credential request; the existing
  "Sign in and continue" case is rewritten for the host that refuses
  preview. `application_anonymous_public_preview_*` stay green.
- `qt_composed_github_scale_{100,125,200}` pin that the signed-out Home
  has no queue heading, list or filter combos, that "Read without signing
  in" is gone, that the field is focused, and that the queue appears once
  a session with an account exists; `test_home_offers_reading_without_signing_in`
  is rewritten to the one-gesture open.
- RFC-BPR-0028's Home paragraph names the signed-out Home; RFC-BPR-0026
  AC-09/AC-10 rows on [BPR-0599](../10-backlog/BPR-0599-close-rfc-0026.md)
  point here.
- The owner pastes a public link on the signed bundle, signed out, and
  reads it without another screen; accepted in an attended session.
