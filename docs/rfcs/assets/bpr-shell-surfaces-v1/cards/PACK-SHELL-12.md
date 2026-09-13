---
schema: "implement-rfc.card.v1"
id: "PACK-SHELL-12"
title: "Verify and close RFC-BPR-0032"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: ["PACK-SHELL-10", "PACK-SHELL-11"]
closeout_for: ["RFC-BPR-0032"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["A1","A2","A3","A4","A5","A6","A7","A8"],"alias_requires_live_allocation":true}
---

# Verify and close RFC-BPR-0032

RFC base: `0000000000000000000000000000000000000000`.

This card tracks formal closeout for its RFC. The checklist rows are the canonical
criteria of the live RFC; every row records the check run, its result and the commit.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **A1 — One card per server.** Accounts & hosts shows the title, the sentence, one card per configured host with header, account row, Access, Rate limit and Details, and the Add enterprise server button; the host-profile picker and its buttons, the locator, the status banner and the servers combo editor are absent from the page and from its accessibility tree. | Not run; import only | No producing revision |
| **A2 — Sign-in is the account row.** Sign in begins the device flow inside the card's account row and shows the code with its copy glyph, the address, Open browser, the validity, one status sentence and Cancel; denied, expired, unavailable and cancelled states show in the same row with one retry; no disabled button and no truncated field is shown, and the manual-token path stays reachable. | Not run; import only | No producing revision |
| **A3 — Enterprise servers are cards.** Add enterprise server adds a card in its edit state using the existing saved-server intents; Remember this server is a switch with its consequence; Forget server confirms and names its scope; a token session is labelled session-only. | Not run; import only | No producing revision |
| **A4 — Nothing repeats.** On every state of the Accounts page each of host, login, transport and connection state appears once, checked by a text inventory over the rendered page. | Not run; import only | No producing revision |
| **A5 — Home leads with the bar.** Signed in with nothing open, Home's first object is the bar with its heading, placeholder and filled Open, focused on arrival; the landing's focus-only buttons are gone; a load refusal is one sentence with Copy details under the bar; the pull-request stack's empty page is not part of Home. | Not run; import only | No producing revision |
| **A6 — Lists are rows.** Recently viewed and the grouped action list share one row anatomy; Resume is a row action; an empty group is absent and an empty list is one sentence with a link to Reviews & queue; the six queue preferences are one Filter menu with chips for non-default choices, absent while there is nothing to filter, with their store and reset unchanged. | Not run; import only | No producing revision |
| **A7 — Signed out is three things.** Signed-out Home is the sign-in card with one filled button, the bar headed for public reading, and the status bar with the session line; the status bar never says no pull request is open while Home is shown; Home over a review keeps its one return row. | Not run; import only | No producing revision |
| **A8 — Rendered and reachable.** The Accounts page in its connected, signed-out and signing-in states and Home in its signed-out, empty, listed and over-a-review states render at 100, 125, 150 and 200 percent in Light, Dark, High-contrast Light and High-contrast Dark with the column at most 720 points, every control reachable by keyboard in reading order with an accessible name, and the owner accepts the real macOS application through the attended-review workflow. | Not run; import only | No producing revision |

## Outcome

Reconcile every A row with direct evidence or a named unresolved criterion, run one integration pass over the composed Settings and Home courses, and record the truthful formal status.

## Acceptance

- All A1–A8 rows reference direct evidence or a specific unresolved criterion.
- One integration pass over the composed Settings, device and Home courses is recorded with its revision.
- Formal closeout obeys canonical order and actual platform and attended results.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
