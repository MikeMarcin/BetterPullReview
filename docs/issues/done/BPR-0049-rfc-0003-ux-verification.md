---
id: BPR-0049
title: Verify RFC-BPR-0003 epoch UX and accessibility
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0046, BPR-0047, BPR-0048, BPR-0057, BPR-0058]
---

# Verify RFC-BPR-0003 epoch UX and accessibility

Exercise the real application at supported widths and 1.0x, 1.25x, and 2.0x.
Inspect independent selector operation, full exact OIDs, causes, aliases,
arbitrary/reverse pairs, unavailable retained history, loading/failure/
cancellation, pruning consequences, identity switches, keyboard traversal,
focus retention, accessible names/descriptions/announcements, and
incremental updates.

Direct native macOS Light/Dark evidence must prove system appearance
inheritance, readable contrast, stable exact selection/focus/model identity,
and zero resets across the live transition. Forced offscreen styles remain
geometry evidence only. Linux native-style evidence is required on the
available target; real Windows UI evidence remains the dedicated open card.

This is not final presentation-polish acceptance. Preserve RFC-BPR-0005's
distinct integrated-workbench polish/native-visual card and RFC-BPR-0006's
System/Light/Dark/high-contrast, font, and syntax ownership.

## Implementation audit frontier

Begin with the actual public GitHub.com/GHES application path and the reusable
epoch control already selected by BPR-0047. Reconcile the three scale-specific
Qt signal/accessibility cases with rendered force-push persistence evidence,
then add only the missing target-native focus/selection/model/palette proof.
Do not treat offscreen rasters as native appearance, redesign the interim
workbench, add a product theme selector, or claim RFC-BPR-0005 presentation
polish. Preserve exact OID and stable semantic identity assertions across any
live appearance transition.

## Implementation checkpoint

- The application-native probe now retains both epoch persistent indexes,
  stable identity roles, exact left/right OIDs, Whole PR selection, model-reset
  counts, palette, appearance-update count, accessibility metadata, and the
  pre-transition focus widget. It rejects a transition unless the real window
  was and remains active, the original non-null widget focus survives, the
  epoch palette stays readable, and every retained identity still agrees.
- Recorded public fixtures report the actual Qt platform and style chosen by
  the running application. These markers do not set either value and remain
  absent from tests-disabled composition.
- The deterministic offscreen contract has a realistic 30-second subprocess
  bound and explicitly requires inactive focus, signal, state, pixel, and
  changed-render failure with no evidence file. The registered CTest timeout
  is 60 seconds.

## Current evidence

- The macOS 1.0x/1.25x/2.0x control cases pass with exact aliases, independent
  selector intents, arbitrary/reverse/identical pairs, SHA-1/SHA-256 text,
  unavailable/pruned/deleted states, loading/failure/cancellation, keyboard
  actions, active Qt focus, accessible names/descriptions, persistent indexes,
  deduplicated announcements, and zero routine resets. The refreshed scale
  plus native-probe contract group passed 4/4 in 25.83 seconds; GitHub.com and
  GHES public fixtures also passed 2/2.
- An authorized Cocoa/QMacStyle System Light-to-Dark run produced distinct
  inspected 2200x1440 rasters and proved exact epoch selection/OIDs/identity,
  Whole PR alias, readable inherited palettes, appearance notification, and
  zero epoch/conversation model resets. The process was not active and its
  focus widget was null, so that run is retained only for the non-focus claims.
  The original system Dark setting was restored.
- A fresh Ubuntu 26.04 aarch64 full-system guest built with GCC 15.2 and Qt
  6.10.2. The three scale cases passed 3/3. The current real GitHub.com fixture
  then passed through unforced XCB and target-selected `QFusionStyle`, reported
  `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, and `RENDER_OK`, and
  produced an inspected 1100x720 raster with the exact epoch pair and complete
  40-character OIDs visible.

## Transferred claim

The owner-activated macOS active-focus transition remains intentionally open.
Accessibility injection, `activateIgnoringOtherApps`, LaunchServices,
Command-Tab scripting, and synthetic clicks could not make the unbundled test
process active in the current desktop permission context. They are not
substitutes for a real focused window, and the final probe now fails rather
than accepting the inactive state. BPR-0059 now owns this exact proof and is a
required RFC closeout dependency; completing this available-platform card does
not claim it. No screenshot or log is checked in.

## Completion evidence

- Commit `ad8dd55` retains stable epoch identity/OID/alias/model/accessibility
  assertions, native palette contrast, real-window activity, and exact focus
  across a live appearance change. It also makes the offscreen contract prove
  rejection rather than native success.
- The current `dev` build completed, and the refreshed 1.0x/1.25x/2.0x plus
  native-probe contract group passed 4/4 in 25.98 seconds. The documentation
  validator and `git diff --check` passed.
- The Linux XCB/QFusionStyle application and macOS Cocoa/QMacStyle
  Light-to-Dark evidence described above were directly rendered and inspected.
  The macOS inactive run is limited to its non-focus facts, and BPR-0059 owns
  the missing active-focus result.
- Tracked and on-disk GitHub workflow counts remain zero; no hosted workflow
  evidence was used.
