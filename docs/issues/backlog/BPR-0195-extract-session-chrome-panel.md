---
id: BPR-0195
title: "Qt: extract the session chrome into a composable panel"
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0190]
---

# Qt: extract the session chrome into a composable panel

Preparation for BPR-0191, split out of it because the whole was too large to
land in one verifiable step.

## Why BPR-0191 was split

Measured before deciding, not guessed:

| | lines |
|---|---|
| `github_window.cpp` | 1731 |
| `workbench_window.cpp` | 977 |
| `github_window_tests.cpp` | 2074 |
| `workbench_window_tests.cpp` | 2558 |

`github_window` implements `github_projection_port`; `workbench_window`
implements `workbench_projection_port` **and** `epoch_projection_port`. Merging
them means reconciling three projection contracts, ~2700 lines of window
implementation, and ~4600 lines of tests in one commit.

That is not a size problem alone — it is a *recoverability* problem. A single
commit that large either lands green or leaves the tree broken, and there is no
useful intermediate state to inspect. The split gives one.

This card is deliberately behaviour-preserving. It ships no user-visible
change, and that is the point: the risky window swap in BPR-0191 should be the
only thing BPR-0191 changes.

## What this slice does

`github_window`'s central layout is already cleanly ordered — identity group,
locator group, status banner, then `PullRequestStack` carrying the review
content. The first three are the session chrome and move together into a
`github_session_panel` widget that implements `github_projection_port`.

`github_window` composes the panel and keeps every accessor it publishes today,
delegating to it. **No test changes.** 151 call sites in
`github_window_tests.cpp` reach into that window, and every one of them must go
on passing untouched — that is this card's principal evidence, because a
refactor that needed its tests rewritten would have proved nothing about
whether behaviour was preserved.

## What must not be done

Do not take the opportunity to fix anything. The accessible names, unique key
sequences, and command-projection behaviour were each forced by a real shipped
defect, and a behaviour-preserving refactor that also changes behaviour is two
commits pretending to be one.

Do not let the panel hold presentation state of its own. It renders what the
projection published, exactly as the window does today.

## Required direct evidence

- `github_window_tests.cpp` passes **unmodified**, and the diff shows it
  unmodified.
- The panel implements `github_projection_port`, and `github_window` no longer
  constructs the chrome widgets directly.
- Accessible names and key sequences across the assembled window are unchanged,
  asserted by the existing uniqueness tests.
- `composed_application_hosts` and the fixture contract's runtime markers still
  pass, so the composition is untouched.
- Each new assertion, if any, is shown to fail when the behaviour it constrains
  is neutered.
