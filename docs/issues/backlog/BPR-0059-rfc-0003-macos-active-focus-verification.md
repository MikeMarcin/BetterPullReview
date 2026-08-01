---
id: BPR-0059
title: Run owner-activated macOS epoch focus-transition verification
status: Backlog
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
