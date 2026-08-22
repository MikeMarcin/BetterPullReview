---
id: BPR-0420
title: Prove the agent process launcher is constructed and invoked
status: Ready
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
`windows_agent_process.cpp`, which is the `Q_OS_WIN`-gated backend that has
never been compiled. Reaching the launcher through the composed graph and
proving it was constructed and invoked says nothing about whether that backend
compiles on Windows, exactly as the panel's equivalent test says nothing about
it either.

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
ctest --preset dev --output-on-failure -R 'agent_invocation_composition'
cmake --build --preset dev --target coding-format-check
```
