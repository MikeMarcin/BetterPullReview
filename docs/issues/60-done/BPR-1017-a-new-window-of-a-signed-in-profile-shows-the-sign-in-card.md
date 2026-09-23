---
schema: "implement-rfc.card.v1"
id: "BPR-1017"
title: "A new window of a signed-in profile shows the sign-in card"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P2"
metadata: {"origin":"BPR-0990 composed discovery course, 2026-09-23","found_by":"BPR-0990"}
acceptance: [{"criterion":"AC1 A composed course opens New Window after sign-in and asserts the second window's Home is the signed-in Home with the profile's queue, before it reads anything, with zero additional account reads and vault operations","outcome":"pass","revision":"bfd927e7","evidence":{"kind":"command","detail":"application_personal_discovery_public_path passes. After New Window and before any read in the second window, it requires session_ready, the choose_pull_request landing with the queue shown, the first window's queue and discovery sentence (4003 under Needs me), the strip naming the account as the first window does, and the strip's refresh enabled. Account reads and vault store/retrieve/delete attempts are unchanged. Before (product at 698a917d): the second window fails with no_profile, the sign-in landing, the queue hidden, 'Sign in' on the strip and no discovery sentence.","path":"docs/issues/60-done/BPR-1017-a-new-window-of-a-signed-in-profile-shows-the-sign-in-card.md#evidence"}},{"criterion":"AC2 The public-preview and signed-out variants present as they do today","outcome":"pass","revision":"bfd927e7","evidence":{"kind":"command","detail":"The same course opens a window signed out, and another after activate_public_preview(). Each lands on the sign-in card with no queue, no_profile and 'Sign in', reads nothing, and is closed. The identical assertions pass against the unfixed product (698a917d), so neither variant changed. adopted_presentation() is empty for a public-preview session and for a window with no session.","path":"docs/issues/60-done/BPR-1017-a-new-window-of-a-signed-in-profile-shows-the-sign-in-card.md#the-course-application_personal_discovery_public_path"}},{"criterion":"AC3 application_profile_multi_window_public_path and application_personal_discovery_public_path still pass; the latter drops its workaround","outcome":"pass","revision":"bfd927e7","evidence":{"kind":"command","detail":"Both pass (75.0 s and 6.6 s). The discovery course no longer reads pull request 18 in the second window before looking at Home. Also passing: the other five profile-window courses, profile management and setup destination public paths, core_github_window_read, application_github_window_read_host, qt_composed_github_scale_100/125/200, qt_workbench_window_scale_100/125/200, the boundary rows, the resealed round-trip composition contract and application_composition_contract with its real Release scan."}}]
---

# A new window of a signed-in profile shows the sign-in card

## Finding

In `application_personal_discovery_public_path` ([[BPR-0990]]) the first window
signs in, and then **New Window** opens a second window of the same profile.
The second window is attached to the profile's accepted session
(`github_application_host::attach_window` adopts it into the window's read
host), but its Qt window is never told: its session panel reports
`no_profile`, its Home shows the first-launch sign-in card
(`landing_state::sign_in`), the queue is hidden (`home_list()->queue_shown()`
is false) and Home's strip controls are absent, although its queue model holds
the profile's rows. It learns the session only from the next transition,
which in practice is its first pull request read. The session presentation is
published on transitions only; nothing replays the current one to a window
that attaches after it.

Observed at `3e12138a` plus the uncommitted BPR-0990 candidate; the course
works around it by opening a pull request in the second window before it looks
at Home.

## Outcome

A window attached to a profile whose account session is accepted presents that
session from its first frame: signed-in Home (bar, queue, strip controls),
the account on the strip, and the same enablement a window that signed in
itself would have. A public-preview session presents as public preview. No
read, vault or provider effect is added to produce it.

## Acceptance

- A composed course opens **New Window** after sign-in and asserts the second
  window's Home is the signed-in Home with the profile's queue, before it reads
  anything, with zero additional account reads and vault operations.
- The public-preview and signed-out variants present as they do today.
- `application_profile_multi_window_public_path` and
  `application_personal_discovery_public_path` still pass; the latter drops its
  workaround.

## Boundaries

RFC-BPR-0024 (profiles and windows) owns window/session presentation; this is a
post-closeout defect, not a reopening of its accepted criteria. No GitHub or
GHES mutation, credential change or new preference.

## Resolution — 2026-09-23, Mac integrator (Mac batch 2)

### Cause

`github_application_host::attach_window` adopts the accepted profile session
into the new window's read host (`github_window_read_host::adopt`, then
`core::github_window_read::adopt`). The core adoption computes the
session-ready transition output and then discarded it. Acceptance is published
to the windows that exist when it happens (`github_profile_runtime::publish`),
so a window attached afterwards had adopted the session and presented none of
it.

A second, smaller defect sat behind it. `workbench_window::publish` applies a
presentation's transition, which refreshes the strip's account button, and then
its session, which sets the account text. It refreshed Home's status line for
the new text ([[BPR-0775]]) but not the button. The first window only ever
showed the name because a later transition corrected it, so a window told the
session in one presentation kept "Not authenticated" on its strip.

### Change (`bfd927e7`)

- `core::github_window_read` keeps adoption's output (`adoption()`) until the
  lane's state changes, and drops it on retirement.
- `github_window_read_host::adopted_presentation()` maps it as the host maps
  every read: the transition, and the accepted session without grant facts,
  exactly as the window's first read would have. It is empty before adoption,
  once the window has moved on, and for a public-preview session, whose
  presentation is the profile host's.
- The composition (`github_application.cpp`), right after admitting a window's
  read host, publishes that presentation to that window alone. It does not go
  through the read host's publication, because that path feeds
  `observe_attached_read`, which would copy the new window's empty snapshot
  into the host.
- `workbench_window::publish` refreshes the account button when it applies a
  session.
- `docs/architecture/system-architecture.md` says so.

Nothing reads, enrolls, touches a vault or reaches a provider. The composition
pins (`application_review_round_trip_composition_contract.cmake`) are resealed
for `github_application.cpp` and the production manifest. The review: the
change inside the pinned scope is the five added lines after
`Host->attach_window(WindowRead)`, one publish to one window of a presentation
with no credential or provider type. No credential, provider, vault, transport
or mutation path changes, and the mutation-branch pin is unchanged
(`a7e2081e…`). The reseal covers `14a18b45..bfd927e7`; `git log` lists no
other change in the pinned scope.

### The course (`application_personal_discovery_public_path`)

- **Signed in:** after New Window, before the second window reads anything, it
  requires `session_ready`, Home's `choose_pull_request` landing with the queue
  shown, the first window's queue and discovery sentence (including 4003 under
  Needs me), the strip naming the account as the first window does
  ("Octo Reviewer (@octo-reviewer)"), and the strip's refresh visible and
  enabled. Account reads and vault store, retrieve and delete attempts are
  unchanged across the new window. The workaround (reading pull request 18 in
  the second window, then returning Home) is gone.
- **Signed out and public preview:** before sign-in, a window opened with no
  session, and then one opened after `activate_public_preview()`, each land on
  the sign-in card with no queue, `no_profile` and "Sign in". Each reads
  nothing and is closed again.

## Evidence

macOS 26.6.2 arm64, `build/dev`, the change as committed in `bfd927e7`:

- **Before** (the six product files at `698a917d`, the new course): the
  signed-out and public-preview windows present as above. The signed-in second
  window fails with status `no_profile`, the sign-in landing, the queue hidden
  with its 4 rows in the model, "Sign in" on the strip and no discovery
  sentence, which is this card's finding.
- **With the adoption replay but not the account-button refresh:** the landing
  and queue pass, and the strip reads "Not authenticated" against the first
  window's "Octo Reviewer (@octo-reviewer)".
- **After:** `application_personal_discovery_public_path` passes (6.6 s).
  `core_github_window_read` passes; it now requires the adoption output after
  adopt, empty before adopt, after the first state change and after
  retirement.
- **Neighbours, all pass:** `application_profile_multi_window_public_path`
  (75 s), `application_profile_second_window_close_public_path`,
  `application_profile_window_restart_public_path`,
  `application_profile_window_layout_public_path`,
  `application_profile_window_review_restoration_public_path` (557 s),
  `application_profile_management_public_path`,
  `application_setup_destination_public_path`,
  `profile_window_restoration_fixture_contract`, `core_profile_window`,
  `application_profile_management_host`,
  `application_github_window_read_host`, `qt_composed_github_scale_100/125/200`,
  `qt_workbench_window_scale_100/125/200`, `application_public_path`,
  `core_dependency_boundary`, `core_boundary_contract`,
  `architecture_boundaries`, `application_review_round_trip_composition_contract`
  after the reseal, and `application_composition_contract` (204 s, with its real
  tests-disabled Release build and scan).
- clang-format 23.1.1 passes all eight files. It also re-aligned the core
  header's member block, which was already unformatted at `698a917d`. The
  project tidy script's findings on the six translation units are all on
  untouched lines (`github_application.cpp` 8171, 15672, 15674;
  `dif_runtime.hpp`; `home_view_state.hpp`). The one on a touched line, a
  lambda's name in the course, is fixed.
