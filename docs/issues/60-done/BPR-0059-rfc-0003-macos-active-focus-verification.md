---
id: BPR-0059
title: Run owner-activated macOS epoch focus-transition verification
status: Done
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

## Direct verification

The owner-activated rerun loaded the real recorded GitHub.com composition,
accepted an existing focused native widget, captured System Light, followed
the same-process operating-system transition to System Dark, and reported:

- `PUBLIC_PATH_OK` and `GITHUB_FIXTURE_OK:github.com`;
- `GITHUB_FIXTURE_QT_PLATFORM:cocoa` and
  `GITHUB_FIXTURE_QT_STYLE:QMacStyle`;
- `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK`;
- both local and global `Light->Dark` transition markers;
- signal, semantic-state, pixel-difference, render, and changed-render success
  markers.

Both inspected images are 2200x1440 at the same device-pixel geometry. They
show the same loaded pull request, Whole PR selection, exact left/right epoch
rows and OIDs, comparison status, conversation state, layout, and focused
owner field. The Light image has SHA-256
`66c1e75d69b8656de8cccde876cec98e0fe865420d41ae085ffda5361c40b2bb`;
the Dark image has SHA-256
`4d1b79b3237475f63b6dd19e3fdc2c302cb9e7eb7c2a207ba1eb384a7b058354`.
The files differ, remain readable, and were retained only in the transient
local evidence directory. The shell trap restored the owner's original System
Dark setting after the run.

The checked-in offscreen contract remains the deterministic rejection guard;
its final replay passed 1/1 in 11.59 seconds. No live GitHub request,
credential, private content, workflow, synthetic activation, or programmatic
focus action was used.

## Documentation disposition

The RFC verification ledger, issue board, closeout preparation, and current
appearance/accessibility evidence now record the completed native epoch-focus
claim. The result does not claim final workbench polish or a selectable theme;
those remain RFC-BPR-0005 and RFC-BPR-0006 respectively.

## Result

The real Cocoa/QMacStyle epoch composition now directly proves that active
focus, persistent semantic selection, exact OIDs, accessible state, readable
native palettes, and zero model-reset behavior survive an owner-driven System
Light-to-Dark transition. BPR-0059 is complete.
