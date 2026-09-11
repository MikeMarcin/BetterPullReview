---
schema: "implement-rfc.card.v1"
id: "BPR-0722"
title: "The strip hides the pull request identity below 1400 pixels"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
---

# The strip hides the pull request identity below 1400 pixels

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
"Launching the app auto opened a review. I have no easy way to tell what
review it is." The three screenshots on the decision show the window about
1,390 points wide with microsoft/terminal#20607 restored and open, the strip
reading "change 1 of 6" and "file 3 of 4", and no repository, number or
title anywhere in the chrome.

## Cause

`apply_strip_width` (`src/adapters/qt/src/workbench_window.cpp:5365-5390`)
hides the identity with `IdentityAction->setVisible(!StripHome && available
>= 1'400)` (line 5380). The threshold was raised from 1,180 to 1,400 by
95fe51af ([BPR-0647](../60-done/BPR-0647-pull-request-overview-page.md))
because the fixture's long identity at 1,280 pushed the right region into an
overflow menu. The text is filled for a public-preview pull request exactly
as for a signed-in one (`"%1/%2  #%3 %4"`, lines 4330-4335, cropped to 360
pixels); signed-out mode plays no part. The result is that at every width
the packet asks the owner to look at (default and 1280x820), the only thing
that says which pull request is open is hidden, while the positions ("file 3
of 4") that mean nothing without it stay visible.

## Scope

- The pull request identity (owner/repository and number at least) is
  visible in the strip whenever a pull request is open, at every width the
  window allows (the floor is 860). Below the width where the full identity
  and the positions both fit, the identity yields its title first, then its
  owner, and the positions yield before the number does; nothing folds into
  an overflow menu (the BPR-0646 invariant).
- The window title stays set for the Dock and Mission Control as today; the
  hidden macOS title bar means the strip is the only in-window place for it.
- Owned paths: `src/adapters/qt/src/workbench_window.cpp` (strip width
  policy and identity elision), `src/adapters/qt/src/workbench_strip.cpp` if
  the identity presenter needs its own elision, and the strip-width
  assertions in `qt_workbench_window_scale_*` and `qt_composed_github_*`
  from BPR-0647.

## Acceptance

- `qt_workbench_window_scale_{100,125,200}` pin, at 860, 960, 1280 and
  1920 with the fixture's long identity, that the identity is visible with
  at least owner/repository and number, that nothing enters an overflow
  menu, and the order in which the parts yield; the 1,280 case fails at HEAD.
- `qt_composed_github_scale_100`, `application_composition_contract` with
  digests repinned, `check_boundaries`, strict tidy, `clang-format --Werror`
  and the documentation gate pass.
- The owner, on the signed bundle at its default size and at 1280x820, can
  say which pull request is open without opening anything; asked in the
  next attended session.

## Related

The restored review at launch is also why the owner's run never took the
packet's path through Home ("Read without signing in", paste the link):
[BPR-0716](../10-backlog/BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md)
covers Home over a restored review. This card is only the chrome's answer
to "what review is this".
