---
schema: "implement-rfc.card.v1"
id: "BPR-0304"
title: "Implement explicit public-preview Qt presentation"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0303","BPR-0317"]
---

# Implement explicit public-preview Qt presentation

## Required final state

- Qt names the mode public preview and never describes it as authenticated or
  equivalent to the review workbench.
- Viewer, resolution, viewed, actionability, mutation, triage, queue-role, and
  epoch associations are absent/disabled with accessible explanations.
- Measured rate limits, 404 ambiguity, GHES refusal, refresh availability, and
  switch-to-sign-in are presented exactly as BPR-0299 approved.
- Three-scale, focus, accessibility, incremental projection, cut-wire, and
  forbidden-action tests exercise the real adapter without provider/vault calls.
- A selected public file reaches the real rendered diff panes with exact path,
  base/head identity, content, and scroll/focus evidence; missing viewer identity
  does not silently remove the RFC's promised diff-reading surface.

## Lane — 2026-09-02

[[BPR-0303]] and [[BPR-0317]] are terminal; claimed straight into
`30-in-progress/` by the integrator, its worker started in a worktree on the
head that carries both, under the transition exception. What they leave at
this card's door: the host's `access_mode`, `public_preview_refusal`, and
`public_preview_legality`; the session projection's core rate limits
(`Limit`, `Remaining`, `ResetUnixSeconds`) that the usage ring reads; the
snapshot's `ReviewCommentsAvailability` and `ReviewCommentGroups` that the
read-only conversation presents; the mounted device panel and its dispatch
that the sign-in gesture reaches; and the bound definitions in
`docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json`
— the words this card puts on screen.

## What this slice delivered — 2026-09-02

**One narrow projection, published by the host.**
`src/application/include/bpr/application/public_preview_projection.hpp` is
the third header the Qt adapter may include, beside the GitHub and
device-authorization projections: `public_preview_presentation` carries the
mode (`signed_in`/`signed_out`), the core budget (`Limit`, `Remaining`,
`ResetUnixSeconds`), the three fixed limitations, an optional typed refusal
whose `SignInOffered` is `static constexpr true`, and the read-only review
comments; `public_preview_gesture` carries `sign_in`. The host takes a
trailing defaulted `public_preview_projection_port*` and publishes by change
on activation, on every observed budget, on every transition and
diagnostic, and on invalidation; `request_public_preview_sign_in()` hands
the gesture to the device-authorization flow for the same profile. No
provider, vault, URL, or credential type crosses.

**The words.** Under public preview the profile selector entry, the window
title, and the workbench header read "Public preview — signed out" with the
profile's host beside it; under an authenticated session nothing changes.
`public_preview_mode_text()` is the single source of that string.

**The ring.** `qt::usage_ring` is a small hand-painted widget in the session
panel's mode strip — at the top of the column rather than inside the hidden
connection-details disclosure, because a hidden ring could be neither a
keyboard stop nor a primary mode indicator. Its arc fills with the consumed
fraction of the hourly core budget; its tooltip reads "<N>% of the hourly
limit · Resets in <H> hr <M> min" (minutes only under an hour, "Resets now"
once passed), then the three limitations, then the sign-in action; it has an
accessible name and description and is a focus stop; activating it emits
the sign-in gesture, which the window routes to the mounted device panel's
"Get a code" for the profile the picker names. Times are computed from a
`NowUnixSeconds` the presentation carries — the host has no clock and
publishes zero, so composition binds `set_public_preview_now`; that is
[[BPR-0305]]'s.

**Refusals and unavailability.** Every public-preview refusal presents its
typed reason with the limit and reset when it has them and the sign-in
action — the negative mutation that dropped the action failed all three
selectors on "every refusal offers the way out of the mode". Triage,
queue-role, viewed-state, and resolution controls are disabled with
"… is unavailable in public preview." tooltips, never hidden and never
defaulted. REST review comments are a read-only conversation when observed,
"Review comments were not observable." when unavailable, and never an empty
conversation; the read-only view reuses the conversation projection with
resolution unclaimed, a shared type rather than a signed-out-only one.

**`qt_anonymous_public_preview_scale_100`, `_125`, `_200`.** A new
executable driving `workbench_window` offscreen at the three scales: the
three mode-label sites; the ring's tooltip at a known fraction and reset;
its accessible name and focus order; the sign-in gesture from the ring and
from a refusal reaching the device dispatch; the disabled controls and
their tooltips; read-only versus not-observable comments; and the cut-wire
negative — no projection published, nothing public-preview-shaped shown, no
gesture possible. Allocated in the inventory, each anchored on its composed
sibling at the same scale.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| The three new selectors, every composed and device Qt scale, the side chat and finding queue, both host suites, the manifest and inventory gates, and both composition contracts | `ctest --preset dev -R '^(qt_anonymous_public_preview_scale_(100\|125\|200)\|qt_composed_github_scale_(100\|125\|200)\|qt_device_authorization_scale_(100\|125\|200)\|qt_side_chat_panel\|qt_finding_queue_view\|application_anonymous_read_host\|application_github_host\|test_network_isolation_manifest\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 18/18 passed | this commit |
| The architecture boundaries hold — no network, process, desktop-services, URL, or provider type in the adapter | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the touched files | exit 0, no output | this commit |
| The ledger's `--all` inventory form with the three selectors allocated | `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all` | `RFC0017_TEST_INVENTORY_OK` | this commit |
| The sign-in offer on a refusal is load-bearing | The worker removed the two lines appending the sign-in text from a refusal's presentation, rebuilt, ran the three selectors; restored byte-identically | all three failed: `FAILED: and every refusal offers the way out of the mode`; restored, 3/3 | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
