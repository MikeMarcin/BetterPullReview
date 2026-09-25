---
schema: "implement-rfc.card.v1"
id: "BPR-1112"
title: "A withdrawn binding failure queued in a throwing turn or at shutdown may never run, and start_delete/prepare_device_rotation lose their completion on allocation failure"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-1048 (claude-fable-5-1, read-only, macOS from source), findings 1, 2 and 4","reviewed_candidate":"a670fd95","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision e2321e81-3011-4178-bd9b-aef06139a3be, 2026-09-24","reachable":"a throwing terminal or projection sink inside a turn or at shutdown; an allocation failure inside start_delete or prepare_device_rotation","related_cards":["BPR-1048","BPR-0884","BPR-0541","BPR-0841"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
---

# BPR-1112: A withdrawn binding failure queued in a throwing turn or at shutdown may never run, and start_delete/prepare_device_rotation lose their completion on allocation failure

## Source

[[BPR-1048]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `e2321e81`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `a670fd95`. The review found no blocking
issue, and it found the four acceptance criteria supported. This card takes
findings 1, 2 and 4. The review is recorded on BPR-1048 under "Independent
review (2026-09-24)".

Line references are the reviewer's and are in
`src/application/src/github_application_host.cpp` unless another file is
named. That file is unchanged from `a670fd95` to the revision where this card
was written.

## Defect

**1. A failure queued in a turn that throws may never run.** BPR-1048 queues
a withdrawn request's failure while a turn is active, and runs the queue when
the turn ends. If the turn ends in an exception, `run_turn`'s catch
(`:518-522`) does not drain `WithdrawnBindings`. The failures wait for a later
turn, and they stay counted until it comes. After `Closing`, almost no
ingress supplies one:

- `dispatch` returns early when `Closing` (`:557-560`);
- so does `enroll_profile` (`:656`);
- so does `invalidate_device_grant` (`:4499-4503`).

So `is_quiescent()` may never hold unless `shutdown()` is called again. The
reviewer suggests a test: a projection sink that throws at shutdown, then
`shutdown()` again, then assert `is_quiescent()`.

**2. A withdrawal at shutdown is not exercised.** The four BPR-1048 tests
reach quiescence through `quiescent_once_closed`
(`tests/application/github_application_host_tests.cpp:5613-5618`). None of
them withdraws a request while the host is shutting down. The reviewer gave a
suggested test. The summary this card was written from does not carry that
test's text, so the acceptance below states the property instead.

**3. An allocation before the lane call still escapes (review finding 4).**
BPR-1048 made each site build its terminal handler before it counts the
request. Two sites still change state before an allocation that can throw:

- `start_delete` sets `DeviceBindingDurable`, `BindingGeneration` and the
  delete terminal at `:4086-4090`, and only then builds `finish` as a
  `std::function` at `:4093`.
- `prepare_device_rotation` does the same at `:4536-4538`.

If that allocation throws, the state has changed but no handler exists to
undo it. For the rotation, `aCompletion` is lost. This predates BPR-1048 and
was outside its scope. The reviewer marks it as a follow-up.

## Scope

- Decide how a turn that ends in an exception delivers its queued withdrawn
  failures. It could drain them in the catch, or at shutdown, or it could
  rely on a second `shutdown()`. State the choice next to BPR-1048's paragraph
  in `docs/architecture/github-host-implementation.md`. If a second
  `shutdown()` is the answer, say who calls it.
- Make a withdrawal during shutdown reach quiescence, and pin it.
- In `start_delete` and `prepare_device_rotation`, allocate before the first
  state change, or restore the state when the allocation throws. The site
  then either keeps its previous state or delivers its one failure terminal.
  For the rotation that terminal is `aCompletion(false)`, exactly once.
  [[BPR-0541]]'s rule holds: a withdrawal is a failure, never a success.

## Acceptance

- AC1 With a projection sink that throws during shutdown, the queued withdrawn
  failures run and the host reaches `is_quiescent()`. A test shows it. If the
  design needs a second `shutdown()`, the test makes that call and the card
  records who makes it in composition.
- AC2 With the persistence lane refusing both executors while the host shuts
  down, the withdrawn request's failure runs, its count is released, and the
  host reaches `is_quiescent()`. A test shows it.
- AC3 With the allocation in `start_delete` and in `prepare_device_rotation`
  made to throw, neither site leaves a counted request, a pending flag or
  changed durability state without a terminal. The rotation answers `false`
  exactly once, or its caller gets the exception and no state has changed.
  The card states which. A test shows it for each site.
- AC4 Every new assertion fails on the unchanged product (negative control).
- AC5 No regression: `application_github_host`,
  `application_device_binding_restart`, `_launch_renewal` and `_relaunch`,
  and the device authorization host and public path.

The code is platform-neutral, and so are the test executors. One platform's
run meets the acceptance.

## Recorded on BPR-1048 from the same review, not in this card's acceptance

- **Finding 3.** When no turn is active, the failure runs at once. If the
  rotation's `completion(false)` then throws, the exception escapes to the
  caller: `:3956` calls the terminal outside any `try`, while the queued path
  catches it at `:3968-3971`.
  There is also a wording nit about "releases its own `Outstanding` count" on
  the card. If AC3's work touches `fail_withdrawn_binding`, make the two paths
  consistent.
- **Finding 5.** The launch-erase site counts the request (`:4473`) before the
  `std::function` conversion (`:4474`). In practice that conversion does not
  allocate.
- **Finding 6.** More than eight queued withdrawals is untested. The reviewer
  judges it unreachable.
