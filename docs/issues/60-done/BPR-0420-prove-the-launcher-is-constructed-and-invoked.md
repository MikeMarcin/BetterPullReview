---
id: BPR-0420
title: Prove the agent process launcher is constructed and invoked
status: Done
rfc: RFC-BPR-0011
depends_on: [BPR-0335]
---

# Prove the agent process launcher is constructed and invoked

The portable half of [[BPR-0232]], split out under the owner's 2026-08-22
direction so it can be worked while its umbrella waits on Windows.

[[BPR-0232]] is gated on [[BPR-0240]] supplying target-native Windows creation
and descendant containment, which is real and is not this. What this card owns
is a portable assertion the board has already scoped precisely, in
[[BPR-0335]]'s own words:

> The panel already has separate construction-and-invocation evidence... The
> launcher is constructed on the composed path but has no assertion naming it
> separately from its linked symbol. That remainder stays with [[BPR-0232]].

So the work is named, small, and provable on any host.

## The shape already exists

`tests/end_to_end/agent_invocation_composition_public_path.cpp` is the pattern:
it reaches `window.agent_panel()` through the composed application, clicks
`start_button()`, asserts `StartIntents == 1U`, and carries a construction-cut
negative so the assertion fails if the object is not actually constructed. The
panel has that evidence. `bpr::adapters::agent::agent_process_launcher` does
not.

## Why this is separable, and the test it has to pass

`agent_process_launcher` is the shared, portable launcher — not
`windows_agent_process.cpp`, which is the `Q_OS_WIN`-gated backend. That backend
has since compiled under MSVC, but its first target-native canary still fails;
[[BPR-0240]] owns that lifecycle diagnosis and proof. Reaching the shared
launcher through the composed graph and proving it was constructed and invoked
says nothing about whether the backend lifecycle succeeds on Windows, exactly
as the panel's equivalent test says nothing about it either.

That distinction is the whole reason this split is legitimate, and it is the one
to check before writing any of it. [[BPR-0360]] and [[BPR-0364]] both considered
a split that looked like this one and correctly declined: their portable-looking
remainder had no way to be proven without Windows, and a card that cannot close
is worse than a card that waits. This one closes on macOS.

## Required final state

- A composed-path assertion names the launcher's construction separately from
  its linked symbol, and separately from the panel that drives it.
- Invocation is asserted through the real gesture, not by calling the launcher
  directly.
- A construction-cut negative exists: removing the launcher from composition
  fails the assertion rather than silently passing.
- The evidence is portable and makes no claim about the Windows backend.

## Non-goals

No Windows claim, no change to `windows_agent_process.cpp`, and no closure of
[[BPR-0232]] — the umbrella keeps its own dependency on [[BPR-0240]].

## Verification

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure \
  -R '^application_agent_invocation_(composition|construction_cut|launcher_construction_cut|gesture_cut)_contract$'
ctest --preset dev --output-on-failure \
  -R '^(application_composition_contract|application_review_round_trip_composition_contract)$'
cmake --build --preset dev --target coding-format-check
```

## Result

The testing composition wraps the real
`adapters::agent::agent_process_launcher` in a test-only
`application::agent_process_port` probe. The probe counts `run` calls and
delegates both `run` and `cancel` to the real launcher; production composition
continues to pass the launcher directly to `agent_invocation_host`.

The positive public-path course now has one shared exact predicate requiring
all of these distinct facts:

- the portable launcher was constructed;
- the panel observed exactly one real start gesture;
- the launcher probe delegated exactly one invocation;
- the composed host recorded exactly one process start; and
- the existing real child canary and exact-context assertions succeeded.

The new launcher-construction-cut course clicks that same panel button with the
launcher absent. It requires the start intent to remain observable, the host to
remain unconstructed, the launcher invocation count and all downstream effect
counters to remain zero, and the positive predicate to remain false. The
pre-existing host-construction and gesture cuts retain their separate
boundaries.

This is portable composition evidence. It neither changes nor makes a claim
about `windows_agent_process.cpp`, and [[BPR-0232]] remains open for its
target-native Windows dependency.

Because the composition translation unit is an integrity-bound application
surface, this slice also re-pins its LF-normalized source digest and the
production application-source manifest. Both values were obtained from the
contract's own fail-closed `found` diagnostic after confirming the source
change was intended. The production mutation-branch digest does not move.

## Verification result — 2026-08-24

| Surface | Direct result |
|---|---|
| Positive path, launcher cut, host cut, gesture cut, cancellation, staleness, cleanup retry, workspace diagnostic | 8/8 passed; the positive course launches the real child and the launcher cut compares the complete seven-counter host statistics value with zero |
| Application composition and source integrity | Both contracts passed; the full source digest is `f464ef06351ec2b82700b061608be249ad2cc03aa5f9a1c0c7e240144d3cbc87` and the production manifest digest is `0ad6f2210d285a53cdb1bc9efb31823935cb12085eb538bedb03771126728bf0` |
| Complete development matrix | 198/198 passed in 1,594.21 seconds |
| Core-only clean configure/build/matrix | 93/93 passed in 238.39 seconds |
| Tests-disabled production build and composition inventories | Build passed; production required 19/forbade 3 and development fixture required 20/forbade 2 |
| C++ formatting | 394/394 files passed under clang-format 22.1.8 |
| Static analysis | Both changed translation units passed warnings-as-errors clang-tidy when filtered to changed lines. The broader changed-file helper still reports only the unchanged `#pragma once` baseline in `public_path_poll.hpp` and `public_path_wait.hpp` |
| Documentation and diff hygiene | `DOCUMENTATION_OK`; documentation tests 105/105; `git diff --check` passed |

The conservative simplification pass found no safe reduction: the delegating
probe, separate launcher flag, shared exact predicate, and distinct cut each
encode a different acceptance fact. Independent capability-critical review
first found the two stale integrity pins, stale Windows-history text, and the
partial zero-counter assertion. All were reproduced and repaired. Its final
verdict reports no blocking or high-risk finding and supports closeout; the
target-native Windows lifecycle remains explicitly deferred to [[BPR-0240]] and
[[BPR-0332]].
