---
schema: "implement-rfc.card.v1"
id: "BPR-0907"
title: "Windows: the owner-admission course's Windows-only branch waits for an 'already open' refusal presentation that never arrives"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win, measuring a WINDOWS_UNCONTAINED pin on application_profile_owner_admission_public_path","found_at":"b9cf4edb","run":"run-20260916-stations","dispatch":"dsp-009"}
acceptance: [{"criterion":"The card states whether the 'already open' refusal presentation is a product promise on Windows","outcome":"pass","revision":"02837c09","evidence":{"kind":"observation","detail":"Not any more: since BPR-0979 Windows hands a second launch to the owner (activation_queued), as POSIX does; the owner_busy notice text still exists in profile_management_window.cpp but this course no longer reaches it, and the old retry filter watched the window while the refusal shows a child notice."}},{"criterion":"The row's Windows branch observes what the product publishes; the row passes uncontained on the Windows station","outcome":"pass","revision":"02837c09","evidence":{"kind":"command","detail":"The Windows-only branches are removed so the course asserts the handoff on every platform; profile_owner_admission_contract.py prints PROFILE_OWNER_ADMISSION_OK uncontained, and through ctest the row passes in 8.22 s."}},{"criterion":"The WINDOWS_UNCONTAINED decision is made from that measurement; BPR-0872's rule still reads true","outcome":"pass","revision":"02837c09","evidence":{"kind":"command","detail":"Contained the seed refuses at the retained ancestor chain (SEED_REGISTRY_REFUSAL:0), which the dictionary already names dos-volume-name; pinned under it with the measurement; test_network_isolation_manifest does not name the row."}},{"criterion":"The other container-temp rows are classified by name","outcome":"pass","revision":"02837c09","evidence":{"kind":"observation","detail":"startup, management, multi_window, second_window_close, two_account_isolation, window_restart, window_layout and activation are pinned uncontained and pass; window_review_restoration fails for BPR-1236's selection difference, not this obstacle."}}]
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

## Answered and fixed — bpr-main-win, 2026-10-01

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, offscreen,
`build/windows-dev` Debug, at `02837c09`.

**Is the "already open" refusal a product promise on Windows? No, not any
more.** The product still has the words, as the `owner_busy` text
`profile_management_failure_text` shows in the management window's notice. But
since [[BPR-0979]] (2026-09-22) Windows hands a second launch of an owned profile
to its owner, as POSIX always has. So neither of this course's Windows-only
branches is reached any more:

- the contender's expected `owner_busy`. Run uncontained, the contender now
  gets `activation_queued` (value 1), and the course stopped there first:
  `same-profile startup routes or reports unavailable transport`;
- the cross-profile Open's refusal, and the retry inside its `Show`. Even
  when it was reached, the retry filter watched the management window. A
  refusal shows the notice inside that window, and a child's `Show` never
  reaches its parent's filter.

**The fix is in the course, not the product.** The two `#if` branches and
`retry_on_refusal` are gone, so the course asserts the same handoff on every
platform. A comment names the reason.

**Run uncontained:** `profile_owner_admission_contract.py` prints
`PROFILE_OWNER_ADMISSION_OK`. That covers owner, contender handoff,
cross-profile open, quit, restart, kill and recovery.

**The pin, from that measurement.** Contained, the seed still refuses the
registry (`SEED_REGISTRY_REFUSAL:0`). The cause is the store's walk of the data
root's retained ancestor chain, which asks for the DOS form above the
container's `AC\Temp`. The card expected a new cause term for it. But
[[BPR-0872]]'s dictionary already names exactly this under `dos-volume-name`:
"The product's retained ancestor chain asks for the DOS form". The siblings
pinned for the same walk use it too (BPR-0845's
`sqlite_queue_enrollment_storage` and `sqlite_workbench_settings_storage`). So
the row is `WINDOWS_UNCONTAINED` under `dos-volume-name`, with this
measurement in its comment, and the dictionary's rule still reads true.
Through `ctest`, `application_profile_owner_admission_public_path` passes in
8.22 s, and `test_network_isolation_manifest` does not name it.

**The other container-temp rows, classified.** None shares this row's second
obstacle. Every other profile public path is already pinned uncontained and
passes:

- `startup`, 11.21 s;
- `management`, 19.60 s;
- `multi_window`, 29.79 s;
- `second_window_close`, 8.13 s;
- `two_account_isolation`, 18.80 s;
- `window_restart`, 19.77 s;
- `window_layout`, 39.39 s;
- `activation`, 17.21 s, since [[BPR-1234]].

The one red row, `window_review_restoration_public_path`, fails for a
different reason: a restored sibling tab gains a selection ([[BPR-1236]]).
