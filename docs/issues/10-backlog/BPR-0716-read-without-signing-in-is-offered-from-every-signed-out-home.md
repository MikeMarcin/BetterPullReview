---
schema: "implement-rfc.card.v1"
id: "BPR-0716"
title: "Read without signing in is offered from every signed-out Home"
status: "Backlog"
rfcs: ["RFC-BPR-0026"]
depends_on: []
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
  that follows [BPR-0715](../30-in-progress/BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md).

## Verification record

Not run; registration on 2026-09-10 under dispatch `ux-redesign-0028-pack.d2`.