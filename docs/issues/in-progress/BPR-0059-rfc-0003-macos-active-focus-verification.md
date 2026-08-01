---
id: BPR-0059
title: Run owner-activated macOS epoch focus-transition verification
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0049]
---

# Run owner-activated macOS epoch focus-transition verification

On the real macOS application path, start from System Light with the
BetterPullReview window active and an existing non-null widget focus. Run the
checked-in appearance probe through the live System Light-to-Dark transition
and require `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK` together with the signal,
state, pixel-difference, changed-render, Cocoa, and QMacStyle success markers.

Directly retain the same persistent epoch indexes, stable semantic identities,
exact left/right OIDs, Whole PR alias, selection, collapse state, focused
widget, accessible metadata, readable inherited palettes, and zero epoch or
conversation model resets. Inspect both rendered images, prove that their
pixels differ for the appearance change rather than content drift, and restore
the owner's original system appearance.

Accessibility injection, programmatic activation, LaunchServices tricks,
synthetic clicks, offscreen rendering, static screenshots, or GitHub Actions
are not substitutes for an owner-activated focused native window. This card
owns only the active-focus evidence split from BPR-0049; it does not add final
presentation polish or a product theme selector. It blocks that macOS claim
and RFC-BPR-0003 closeout, not available-platform implementation,
documentation, security review, simplification, or later RFC work.

## Progress

- The checked-in Cocoa probe now reports
  `NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` and waits before collecting its
  initial evidence while the real window lacks either active-window state or
  a non-null focus widget. It does not call an activation API, synthesize
  input, or relax any post-focus assertion. Non-Cocoa probes still enter the
  existing fail-closed check immediately.
- The deterministic offscreen appearance-probe contract still passes 1/1 in
  12.73 seconds with its complete inactive/signal/state/pixel/render rejection
  and zero-evidence requirements.
- The first real Cocoa/QMacStyle attempt reached the explicit owner-focus wait
  with the loaded GitHub.com fixture, then timed out without an owner click.
  It reported `NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED`, accepted no PNG
  evidence, and the shell trap restored the original System Dark appearance.
  This is a clean unverified attempt, not acceptance evidence.
