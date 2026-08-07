---
id: BPR-0195
title: "Qt: extract the session chrome into a composable panel"
status: Done
rfc: RFC-BPR-0005
depends_on: [BPR-0190]
---

# Qt: extract the session chrome into a composable panel

Preparation for BPR-0191, split out of it because the whole was too large to
land in one verifiable step. Deliberately behaviour-preserving: it ships no
user-visible change, so that the risky window swap in BPR-0191 is the only
thing BPR-0191 changes.

## What this slice did

| | before | after |
|---|---|---|
| `github_window.cpp` | 1755 | 944 |
| `github_session_panel.cpp` | — | 682 |
| `github_presentation_text.hpp` | — | 400 |
| `github_window_tests.cpp` | 2074 | **2074, byte-identical** |

`github_session_panel` owns the host-profile identity group, the pull-request
locator, and the status banner, plus the state only they use. `github_window`
composes it and keeps every accessor it published, delegating.

The reviewer-facing formatters — status wording, reason wording, capability and
rate-limit summaries — moved to `github_presentation_text.hpp` **verbatim**.
Both halves say the same words because they call the same functions, not
because two copies were kept in step.

## The port the card asked for, and why it is not here

BPR-0195 required that the panel implement `github_projection_port`. **It does
not**, and the reason is a property of the projection rather than a preference.

Inside a single transition the order is: commands, identity, capabilities,
*clear the pull request*, status, diagnostics. `apply_status` reads whether a
pull request is on screen, so a **content** mutation sits between two **chrome**
applications. A chrome-only `publish` would have to either re-implement that
ordering rule or be wrong — and a second copy of an ordering rule is exactly
how the two windows this RFC is merging came to disagree in the first place.

So the panel exposes the individual appliers and the composing window keeps the
routing, unchanged. That is also what makes the slice behaviour-preserving: the
sequence a projection is applied in is not re-derived anywhere.

Two facts still cross the seam, named rather than reached for:
`HasPullRequest` is read, and `PresentationChanged` is called. Both are
explicit members of `github_session_environment`.

`apply_command` returns whether the panel owned the command, because
`show_review_threads` drives the conversation view, which stays with the
window. A command owned by neither is a routing hole; a bool makes it one the
window can see.

## The defect this slice introduced, and how the untouched tests caught it

The panel's first `diagnostic_text()` read:

```cpp
return DiagnosticLabel->isVisible()
    ? DiagnosticLabel->text().toUtf8().toStdString()
    : std::string{ };
```

The window's had no guard. The guard reads as *more* correct — why report text
nobody can see? — and it is a behaviour change in a card whose whole purpose is
not to make any.

It broke thirteen reason codes at once. Every assertion about diagnostic
wording runs against a window that was never `show()`n, so `isVisible()` is
false for every widget in it and the accessor returned nothing. The failure
said `every GitHub reason has category-specific presentation wording`, which
reads like a lost translation and was a lost accessor contract.

Both callers turned out to exist and to want different answers, so there are
now two: `diagnostic_text()` reports the text the label holds, and
`showing_diagnostic()` reports whether the banner is showing one. The empty
state's fallback wording uses the second.

**This is the case for the card's rule.** A refactor whose tests had been
rewritten to match would have absorbed this silently — the rewritten assertion
would have shown the window, passed, and shipped an accessor that answers
differently for every caller who does not.

## Required direct evidence

- `github_window_tests.cpp` passes **unmodified**, and the diff shows it
  unmodified. **Met**: `git status` reports no change under `tests/`, and the
  three scale cases pass.
- The panel implements `github_projection_port`. **Refused, with reasons
  above.** It owns the chrome widgets and exposes the appliers; the window
  keeps the routing it already had.
- `github_window` no longer constructs the chrome widgets directly. **Met**:
  175 lines of construction moved; the window's constructor composes the panel.
- Accessible names and key sequences across the assembled window are unchanged,
  asserted by the existing uniqueness tests. **Met**, unmodified. Tab order is
  still set on the window, from panel accessors, so the sequence is unchanged.
- `composed_application_hosts` and the fixture contract's runtime markers still
  pass, so the composition is untouched. **Met**.
- Each new assertion, if any, is shown to fail when the behaviour it constrains
  is neutered. **No new assertions.** That is the point of this card: the
  evidence is the existing suite passing against moved code. The one thing that
  did fail during the move was a real regression, and it was caught by an
  assertion nobody wrote for this refactor.

## What this card does NOT deliver

- **No user-visible change**, by construction.
- **No merged window.** BPR-0191 still has to move the panel into the shell and
  reconcile the accessible-name and key-sequence sets across both.
- **No reduction in what the window knows.** It still routes every projection;
  it just applies fewer of them itself.
