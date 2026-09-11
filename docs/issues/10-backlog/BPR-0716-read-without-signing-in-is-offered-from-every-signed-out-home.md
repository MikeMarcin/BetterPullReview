---
schema: "implement-rfc.card.v1"
id: "BPR-0716"
title: "Read without signing in is offered from every signed-out Home"
status: "Backlog"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0732"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"bdd9244a-754a-4a7a-94b4-79c7110687d9"}
---

# Read without signing in is offered from every signed-out Home

The owner, 2026-09-10, in the verdict on
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
"Flow to open the PR is hella confusing, I don't know why I have to go into
the settings menu just to open a PR without an account. That should be
doable directly from Home."

This is owner direction, not a defect in BPR-0646 as written, and it is
registered on its own rather than folded into that card. It belongs to
RFC-BPR-0026's Home (signed-out navigation, AC-09; the composed Home flows,
AC-10) and is registration only: the adopted plan's `plan-rfc-0026`
milestone ([BPR-0598](BPR-0598-inventory-personal-discovery-url-flow-and-plan.md))
inventories Home and allocates its slices, and this card is an input to that
inventory. The plan itself is unchanged, so no amendment was published.

## What Home does today

[BPR-0670](../60-done/BPR-0670-public-preview-is-buried-and-does-not-say-what-it-is.md)
put "Read without signing in" on Home beside Connect, but only in the
landing's `sign_in` state (`workbench_landing.cpp`,
`ReadSignedOut->setVisible(aState == landing_state::sign_in)`). The pack-head
bundle restores the previous public-preview review at launch, so the owner's
Home was in the `back_to_review` state ("Reviewing microsoft/terminal #20607
… Back to the review / Choose another pull request"), and the only visible
route to a signed-out read was the session panel on Settings > Accounts and
hosts. The bar under the card does open a pasted link, but nothing on that
Home says the read needs no account, and "Choose another pull request"
opens Settings.

## Scope

- A signed-out reviewer reaches a public pull request from Home in every
  landing state, including a Home shown over a restored or open review,
  without opening Settings, and the control says what it does.
- "Choose another pull request" while signed out does not route through
  Settings.
- Owned paths: `src/adapters/qt/src/workbench_landing.cpp`,
  `src/adapters/qt/src/workbench_window.cpp`, their tests and the composed
  Home course in `qt_composed_github`.

## Acceptance

- From each landing state while signed out, one visible control on Home
  activates the selected profile in public-preview mode and focuses the open
  bar; `qt_composed_github_scale_{100,125,200}` pin it per state.
- The owner accepts the flow on the signed bundle in the attended session
  that follows [BPR-0715](../40-parked/BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md).

## Verification record

Not run; registration on 2026-09-10 under dispatch `ux-redesign-0028-pack.d2`.

## Owner direction, 2026-09-11 (v006)

On the BPR-0646 rejection in the v006 attended session (decision `d06b4b43`, 18:30:10Z), of
the sign-in criterion: "Technically yes, but the flow is very awkward, we should be able to
paste into the url box on the home sceen and click something like open without signing in
and it should just work. The home screen is very messy." That is this card's scope stated
once more: one visible control on Home, in every landing state, that opens a pasted link
without an account and says so. "The home screen is very messy" is broader and is an open
question to the owner (which parts of Home are noise before a pull request is open) for
the RFC-BPR-0026 Home inventory this card feeds; it is not carded on its own.

## Owner answer, 2026-09-11 (d10)

Asked what on Home is "very messy": "a whole bunch of stuff on the bottom
that doesn't have any use without an account signed in. Review queue
doesn't make sense when you don't have an account signed in. Open a pull
request by link doesn't work without an account. Clicking read without
signing in leads to another confusing page, there's no reason for a
separate screen at all just let me open the link in anonymous read-only
mode without an account from that first paste link. App seems to
automatically, but very slowly, auto-load the last PR on app restart which
is disorienting; a recently viewed list or something would be better."

Two cards carry it. [BPR-0732](../40-parked/BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
takes this card's scope with it (the signed-out Home is the card, one
paste field and the status bar; the field opens anonymously in one
gesture; "Read without signing in" and the detour go). The "doesn't work
without an account" remark was verified against the code, not assumed:
the Home bar goes through the setup destination, which is `ready` only
for an authenticated profile, so "the bar under the card does open a
pasted link" above was wrong; only the Settings session panel opens
signed out. [BPR-0733](../40-parked/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md)
replaces the automatic restore with a recently viewed list. This card
depends on BPR-0732 and is finished from its own acceptance when the
owner accepts that one.
