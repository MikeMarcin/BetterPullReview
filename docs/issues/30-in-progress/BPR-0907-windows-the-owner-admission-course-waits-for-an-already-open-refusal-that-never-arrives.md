---
schema: "implement-rfc.card.v1"
id: "BPR-0907"
title: "Windows: the owner-admission course's Windows-only branch waits for an 'already open' refusal presentation that never arrives"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win, measuring a WINDOWS_UNCONTAINED pin on application_profile_owner_admission_public_path","found_at":"b9cf4edb","run":"run-20260916-stations","dispatch":"dsp-009"}
---

# BPR-0907: Windows: the owner-admission course's Windows-only branch waits for an 'already open' refusal presentation that never arrives

## Defect

`application_profile_owner_admission_public_path` is one of the six
container-temp rows of the private-descriptor family ([[BPR-0875]],
[[BPR-0900]]). `bpr-win` has now taken it apart into **two obstacles**, and
this card is the second one, which is not a harness question.

**Contained**, the row refuses at the registry (`SEED_REGISTRY_REFUSAL:0`),
because the store's ancestor-chain walk (`open_state_directory` →
`validate_ancestor_chain`) is denied above the leaf: the lowbox token cannot
open the ancestors of the container's `AC\Temp`, and no descriptor written on
the leaf changes that. A `WINDOWS_UNCONTAINED` keyword, added and measured and
then reverted, **removes that refusal**: the seed is accepted and the course
runs on.

**Uncontained, the row is still red**, and what it fails on is now exact.
Six `wait_for` call sites in
`tests/end_to_end/profile_owner_admission_public_path_tests.cpp` shared one
default message, so every timeout read "bounded public course condition".
Named per site (`b9cf4edb`, on the Windows lane), the failure is:

```
FAILED: the refusal is retried    (profile_owner_admission_public_path_tests.cpp:134)
```

That assertion sits inside `#if !defined(__unix__) && !defined(__APPLE__)` —
a **Windows-only** branch of the course. Its `retry_on_refusal` filter waits
for the management window to publish a status containing "already open" as a
`QEvent::Show`, then clicks Open again. Within its 30 s bound it never fires.
So the Windows branch of this course waits for a refusal *presentation* the
product does not publish there, and the application then exits 1 in own-mode,
with a `WinError 32` handle still held on `profile-registry.sqlite3` as a
cascade of that failure.

Whether the refusal **should** be published that way on Windows is an
application question, not a harness one, and it is the question this card
carries. The station that found it deliberately did not decide it, minted
nothing, and did not land the pin.

## Why the pin is not landed yet

A `WINDOWS_UNCONTAINED` pin would trade a contained refusal for an uncontained
failure and leave the row red either way, so it buys the row nothing today.
The measurement stands (on [[BPR-0875]] and above); the pin becomes worth
landing only once the row is green uncontained, and when it is, the cause term
to land it under is a **new** one — the store's ancestor-chain walk denied
above the leaf — which none of the seven existing terms in the
[[BPR-0872]] dictionary describes. Landing it then means the keyword in
`tests/**` and the dictionary entry in `scripts/**` in one commit on main,
after measurement, exactly as the other twelve pins were landed.

## Scope

- Read what the product actually publishes on Windows when a second launch
  meets an owner: is there an "already open" status at all, is it a management
  window status, and is it shown as the course expects? Decide whether the
  Windows branch of the course is asserting a promise the product makes, or
  one it never made. That decision is the card's subject; fix whichever side
  is wrong.
- Then run the row **uncontained** and record the result. Only then decide
  the pin, with its new cause term, or record why the row should stay
  contained and red.
- Check the other five container-temp rows for the same two-obstacle shape
  before assuming they share this one's second obstacle; `bpr-win` measured
  `profile_window_restart` and `profile_window_layout` at
  `TEST_RUNNER_CHILD_EXIT_FAILED:1` without naming what they fail on.

## Acceptance

- The card states whether the "already open" refusal presentation is a
  product promise on Windows, with the code that publishes it or the reason
  it does not.
- The row's Windows branch either observes the presentation the product
  publishes, or the product publishes the one the course expects; the row
  passes uncontained on the Windows station.
- The `WINDOWS_UNCONTAINED` decision for this row is made from that
  measurement; if pinned, its new cause term lands with the dictionary in one
  commit and [[BPR-0872]]'s rule still reads true.
- The other five container-temp rows are classified as sharing this second
  obstacle or not, by name.
