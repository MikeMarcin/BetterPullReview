---
schema: "implement-rfc.card.v1"
id: "BPR-0732"
title: "A signed-out Home is one paste field that opens a public pull request"
status: "Done"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0026"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","supersedes_scope_of":"BPR-0716"}
acceptance: [{"criterion":"core_setup_destination pins the anonymous stage and the kept sign_in stage","outcome":"pass","revision":"working tree over 1f5d6fb3","evidence":{"kind":"command","detail":"ctest -R core_setup_destination passed; the new cases fail at HEAD (stage public_preview did not exist)."}},{"criterion":"setup_destination_public_path: pasted public link with no account reaches pull_request_ready in public preview, no sign-in step, no credential request; GHES case rewritten; anonymous public preview stays green","outcome":"pass","revision":"working tree over 1f5d6fb3","evidence":{"kind":"command","detail":"application_setup_destination_public_path, application_anonymous_public_preview_public_path, application_profile_window_review_restoration_public_path, application_composition_contract passed."}},{"criterion":"qt_composed_github_scale_{100,125,200}: signed-out Home has no queue heading/list/filters, no Read without signing in, field focused, queue appears with an account","outcome":"pass","revision":"working tree over 1f5d6fb3","evidence":{"kind":"command","detail":"qt_composed_github_scale_100/125/200 and qt_anonymous_public_preview_scale_100/125/200 passed with test_home_signed_out_is_one_paste_field."}},{"criterion":"RFC-BPR-0028 Home paragraph names the signed-out Home; RFC-BPR-0026 AC-09/AC-10 rows on BPR-0599 point here","outcome":"pass","revision":"working tree over 1f5d6fb3","evidence":{"kind":"observation","detail":"docs/rfcs/RFC-BPR-0028-review-workbench-direction.md Home paragraph and BPR-0599 AC-09/AC-10 rows updated; verify_documentation.py DOCUMENTATION_OK."}},{"criterion":"The owner pastes a public link on the signed bundle, signed out, and reads it without another screen","outcome":"pass","revision":"2cdc0e57","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0732-signed-out-home, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint 81a8601820c70b1f5583507562c8b1b1275ff4dc5e9f115a62b0b7b60e2d08ae, decision 18afd9ae-c74b-4ddc-9e4f-e08f5f5d52ed, actor human, 2026-09-11T23:48:01Z: accept. The signed-out Home is the account card and one paste field; a pasted public link opened in one gesture with no queue and no 'Read without signing in'; accepted without a note. BPR-0716 finishes with this verdict."}}]
---

# A signed-out Home is one paste field that opens a public pull request

The owner, 2026-09-11, asked what on Home is "very messy"
([BPR-0716](BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md)):
"a whole bunch of stuff on the bottom that doesn't have any use without an
account signed in. Review queue doesn't make sense when you don't have an
account signed in. Open a pull request by link doesn't work without an
account. Clicking read without signing in leads to another confusing page,
there's no reason for a separate screen at all just let me open the link in
anonymous read-only mode without an account from that first paste link."
Two screenshots of the signed-out dark Home accompany it (the initial
state and the state after "Read without signing in"); their visible text
is transcribed on the [BPR-0593](BPR-0593-close-rfc-0018.md)
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

## Verification record, 2026-09-11 (d12)

The core decides. `setup_profile_observation` gains `PreviewAvailable` and
`setup_destination_stage` gains `public_preview`; `resolve_setup_destination`
answers `ready` for an account, else `public_preview` for a host that can
read without one, else `sign_in` (the host that refuses preview keeps the
stop), else `add_server`; `accept_setup_destination` accepts the new stage.
The branch lives in the core because the decision is which way in a pasted
link takes, and the header comment says so.

The host performs. `setup_destination_host::resolve` presents "Opening
<host> without signing in…" and calls the new port method `preview`; the
app's port reports `PreviewAvailable` as the composition's anonymous
authority on the GitHub.com contract (`PublicPreviewComposed`, so GHES and a
platform without the authority stay at sign-in). `preview` activates public
preview directly (not through `dispatch_intent`, which retires the pending
destination), arming `SetupAcceptedGeneration` so the identity transition it
publishes is the expected one; `observe_setup_session` then calls
`previewed`, and `open` accepts a public-preview session for the exact
profile and hostname. A refused activation cancels with its own notice.

Qt presents. The landing's `sign_in` card says the bar opens a public pull
request without signing in, read-only; the "Read without signing in"
button and its dispatch are gone. `update_home_landing` derives `account =
signedIn && !PublicPreviewSignedOut`: the queue heading and
`reviewQueueLanding` (list and six filter combos) are visible only with an
account, a public-preview session lands on the same `sign_in` card, and the
bar takes focus on arrival at a signed-out Home (once, so a projection never
steals focus). The pull-request stack is not shown while its page would
read "Open a pull request"; its text now names the Home bar. A pending
`public_preview` stage offers Cancel only. Over an open review with no
account, "Choose another pull request" moves to the bar, not the hidden
queue.

Pinned by `core_setup_destination` (three new cases),
`application_setup_destination` (preview on submit, exact-profile open,
sign-in kept for the refusing host), `test_home_signed_out_is_one_paste_field`
in `qt_composed_github_scale_{100,125,200}` (no queue, no button, bar focused,
no empty page, preview session keeps the card, account shows the queue) and
`application_setup_destination_public_path` (github.com course: a pasted
link with no account reaches `pull_request_ready` under public preview with
zero vault stores, zero vault retrieves and no account transport; the GHES
course keeps Add server then Sign in and continue).
`application_profile_window_review_restoration_public_path`'s account seed
now signs in from the card first, since a pasted github.com link no longer
stops at sign-in where preview is composed. Also run:
`qt_anonymous_public_preview_scale_*`,
`application_anonymous_public_preview_public_path`,
`application_composition_contract` (both `github_application.cpp` digests
repinned), the four boundary selectors, clang-format and strict tidy on the
touched files, DOCUMENTATION_OK. RFC-BPR-0028's Home paragraph and
RFC-BPR-0026 AC-09/AC-10 on [[BPR-0599]] point here. [[BPR-0716]] stays in
Backlog on this card and is finished when the owner accepts it. The last
criterion, the owner pasting a public link signed out on the signed bundle,
waits on the v007 attended session.

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0732-signed-out-home` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: signed out, Home is the card, one paste field and Open, no queue or filters, no Read without signing in; a pasted public link opens in one gesture.

## Attended verdict, 2026-09-11 (v007): accepted

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; the session was handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0732-signed-out-home`, decision `18afd9ae`, 23:48:01Z: **accept**.
Accepted without a note. [BPR-0716](BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md), whose scope this card took, finishes on the same verdict. Done.
