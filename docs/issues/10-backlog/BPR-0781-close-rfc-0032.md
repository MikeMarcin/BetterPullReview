---
schema: "implement-rfc.card.v1"
id: "BPR-0781"
title: "Verify and close RFC-BPR-0032"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0779", "BPR-0780"]
closeout_for: ["RFC-BPR-0032"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["A1","A2","A3","A4","A5","A6","A7","A8"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0032

RFC base: `38f32226351f5cc8e4657d55c3e151d8450c6ce5`.

This card tracks formal closeout for its RFC. The checklist rows are the canonical
criteria of the live RFC; every row records the check run, its result and the commit.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **A1 — One card per server.** Accounts & hosts shows the title, the sentence, one card per configured host with header, account row, Access, Rate limit and Details, and the Add enterprise server button; the host-profile picker and its buttons, the locator, the status banner and the servers combo editor are absent from the page and from its accessibility tree. | Not run; import only | No producing revision |
| **A2 — Sign-in is the account row.** Sign in begins the device flow inside the card's account row and shows the code with its copy glyph, the address, Open browser, the validity, one status sentence and Cancel; denied, expired, unavailable and cancelled states show in the same row with one retry; no disabled button and no truncated field is shown, and the manual-token path stays reachable. | Not run; import only | No producing revision |
| **A3 — Enterprise servers are cards.** Add enterprise server adds a card in its edit state using the existing saved-server intents; Remember this server is a switch with its consequence; Forget server confirms and names its scope; a token session is labelled session-only. | Not run; import only | No producing revision |
| **A4 — Nothing repeats.** On every state of the Accounts page each of host, login, transport and connection state appears once, checked by a text inventory over the rendered page. | Not run; import only | No producing revision |
| **A5 — Home leads with the bar.** Signed in, Home's first row under the strip is the address-field bar with its label, placeholder, ⌘L and Return, focused on arrival; the landing's focus-only buttons are gone; a load refusal is one sentence with Copy details under the bar; the pull-request stack's empty page is not part of Home; first launch and signed out show the strip with Home, account and settings only, the bar relabelled for public reading, the sign-in card with Sign in filled and Add enterprise server bordered, and the status bar line, and nothing else. | Not run; import only | No producing revision |
| **A6 — Rows in state order, panels by state.** Rows are always ordered Needs me, Waiting on others, Following from the core attention projection, with one row anatomy for the queue and Recently viewed; colour means state only, on the reason word and the row mark or panel rail; with State headers on each state is a bordered panel with a tinted sticky header, count, chevron and rail, collapsible by pointer and keyboard with a one-line summary when collapsed; empty states are absent and an all-empty list is one sentence linking to Reviews & queue; no empty box renders. | Not run; import only | No producing revision |
| **A7 — Group, account and Filter.** Group offers None, Organization, Repository and Account, each group a neutral panel headed by the owner's or account's avatar and name, with state bands nested inside when State headers is on and plain state-ordered rows when off; a host shows only where it differs; with more than one signed-in account each row carries the account chip at its right, a single-account group carries it in its header instead, and a single-account profile shows none; the strip holds Group, State headers, Filter and the refresh glyph before account and settings; Filter holds exactly My role, Added by, Quiet items and Sort within a group over the existing typed preferences with their store and reset unchanged; Group, State headers, collapse and Filter persist per window; Home over a review keeps its one return row; the status bar shows the session line and never that no pull request is open while Home is shown. | Not run; import only | No producing revision |
| **A8 — Rendered and reachable.** The Accounts page in its connected, signed-out and signing-in states and Home in its first-launch, default, long-list scrolled, nothing-needs-me, over-a-review and four grouping states render at 100, 125, 150 and 200 percent in Light, Dark, High-contrast Light and High-contrast Dark, every control reachable by keyboard in reading order with an accessible name and a collapsed panel's state announced, and the owner accepts the real macOS application through the attended-review workflow. | Not run; import only | No producing revision |

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
