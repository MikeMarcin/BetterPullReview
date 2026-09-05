---
id: BPR-0560
title: Make GitHub connection diagnostics selectable and copyable
status: Done
rfc: RFC-BPR-0002
depends_on: []
---

# Make GitHub connection diagnostics selectable and copyable

The owner could only share the v007 failure via a screenshot because the connection diagnostic was not easily selectable/copyable. Make the status and diagnostic text selectable with the mouse and keyboard through ordinary Qt controls, preserving plain-text rendering and safe diagnostic content. Expose the current text to accessibility rather than only a generic label.

## Acceptance

- The displayed GitHub status and diagnostic can be selected and copied using normal platform gestures.
- Accessible text includes the actual message and updates/clears with projection changes.
- Verify the real widget interaction and focused Qt session tests; no host/provider effects or credential copying are introduced.

Observed source: `2a043335`; [v007 attended result](../../verification/reviews/designated-live-review-publication-v007/attended-result.md).

## Verification

Added an ordinary Copy message control to the status banner and empty/error
page. Both use the current displayed plain text only. The four labels now
support keyboard as well as mouse selection; accessibility exposes their real
text, with the generic role name moved to description. Composed tests exercise
both copy controls and ensure cleared/projected messages replace stale text.

- `cmake --build --preset dev --parallel 6`: pass.
- `cmake --build build/app-release --parallel 6`: tests-disabled Release pass.
- `ctest --test-dir build/dev --output-on-failure -R '^(qt_composed_github_scale_(100|125|200)|qt_anonymous_public_preview_scale_(100|125|200))$'`: 6/6, 4.69 seconds, network isolated.
- Full composed-window failure render inspected at 100%; Copy message and the
  exact diagnostic are visible. The three scale renders remain in the reused
  ignored build tree; no native system-appearance verdict is claimed.
- Scoped strict tidy for both changed Qt TUs and the composed test TU, format,
  code-simplifier inspection and diff hygiene pass. The consumed temporary
  `/tmp/bpr0560-tidy.log` was removed.
- Documentation structure gate passes. No host/provider/credential behavior
  changed and no live authentication was retried.
