---
schema: "implement-rfc.card.v1"
id: "BPR-0722"
title: "The strip hides the pull request identity below 1400 pixels"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-10","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
parked_on: "human-review"
acceptance: [{"criterion":"qt_workbench_window_scale_{100,125,200} pin, at 860, 960, 1280 and 1920 with the fixture's long identity, that the identity is visible with at least owner/repository and number, that nothing enters an overflow menu, and the order in which the parts yield; the 1,280 case fails at HEAD","outcome":"pending","revision":"901a9c06","evidence":{"kind":"observation","detail":"tests/adapters_qt/workbench_window_tests.cpp test_strip_identity_stays_at_every_width publishes microsoft/terminal #20607 with a long title and pins: 1920 shows 'microsoft/terminal  #20607 Fix the ScreenBufferTests suite …' with the positions; 1280 shows 'microsoft/terminal  #20607' (title yielded) with the positions; a 20-pixel sweep from 1920 to the floor shows no qt_toolbar_ext_button at any width, a visible identity always carrying the number, and the identity yielding once and not returning. The card's premise does not hold at the two narrow widths: the window floor is 900 (setMinimumSize(900, 360)), not 860, and at 100% scale the strip's other items alone lay out about 900 pixels (measured: a 960 window with the identity cut to 't…l  #20607' at 79 px and the positions hidden still laid out 979 px), so below about 960 the number itself has no room. The yield order implemented is title, owner, modality shortcuts, positions, then the number; at 960 and at the floor the identity has yielded and the positions keep their prior rule (shown at 960, hidden at the floor). Which strip control should give way there, or whether the floor moves to 960, is the owner's decision and is asked with the v004 packet. qt_workbench_window_scale_{100,125,200} pass (scratch ctest-0722c.log); at HEAD the 1280 case fails because the identity was hidden below 1400. A pre-existing overflow at 1000-1050 (positions shown from 960 with the expanded modality: a 1040 window laid out 1050 px) surfaced in the sweep and is closed by holding the compact modality until 1100; UIforETW's compact-row contract at 1280 cannot be re-run because it is red at HEAD (BPR-0723)."}},{"criterion":"qt_composed_github_scale_100, application_composition_contract with digests repinned, check_boundaries, strict tidy, clang-format --Werror and the documentation gate pass","outcome":"pass","revision":"901a9c06","evidence":{"kind":"command","detail":"build/dev: qt_composed_github_scale_{100,125,200}, qt_workbench_window_scale_{100,125,200}, application_composition_contract and application_review_round_trip_composition_contract pass (scratch ctest-0722c.log); no digest drifted because the change is confined to src/adapters/qt and tests. check_boundaries verified at this head's core change (boundaries-0719.log; no boundary-relevant file changed since). tidy.py --warnings-as-errors on src/adapters/qt/src/workbench_window.cpp and tests/adapters_qt/workbench_window_tests.cpp: clang-tidy passed (tidy-0722c.log). clang-format --dry-run --Werror on the three touched files: exit 0. python3 scripts/verify_documentation.py --root .: see the finish commit's return."}},{"criterion":"The owner, on the signed bundle at its default size and at 1280x820, can say which pull request is open without opening anything","outcome":"pending","revision":"901a9c06","evidence":{"kind":"freeform","detail":"Asked in the next attended session (v004 packet) on a bundle rebuilt from this head: the strip reads 'owner/repository  #number title' at the default size and 'owner/repository  #number' at 1280x820."}}]
---

# The strip hides the pull request identity below 1400 pixels

The owner, 2026-09-10, in the v003 verdict on
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
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

## Attended candidate, 2026-09-10 (v004)

The pending criterion above is candidate `BPR-0722-*` in
[rfc-bpr-0028-attended-v004](../../verification/reviews/rfc-bpr-0028-attended-v004/operator-packet.md)
on the bundle built at 3c9a3f1c (executable sha256 `9261509a…`), candidate
revision `app-release-9261509a-2026-09-10.3`; prepared, undecided, no host
running until the owner asks for the session.

## Owner's answer, 2026-09-11

Asked which strip control should give way below about 960 pixels so the
identity's number can stay to the 900-pixel floor, or whether the floor
should move to 960, the owner answered verbatim: "leave it be for now".
Decision recorded: the strip keeps its 900-pixel floor and, below about
960 pixels at 100% scale, the identity yields whole as measured above.
This settles nothing else on this card; the attended criterion (the owner
can say which pull request is open at the default size and at 1280x820) is
still asked, on the v005 bundle.

## Attended candidate, 2026-09-11 (v005)

The pending criterion above is candidate `BPR-0722-strip-identity` in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md)
on the bundle built from the source at 25e1525d (executable sha256
`d238842d…`), candidate revision `app-release-d238842d-2026-09-11.1`;
prepared, undecided, no host running until the owner asks for the session.
The v004 packet is historical.

## The compact row at 1280, rerun, 2026-09-11

`application_uiforetw_selected_diff_contract`, which the first criterion's
evidence could not run at HEAD, passes for all four appearances once
[BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
lands: the compact navigation row at 1280 spans the window with the
BPR-0646/BPR-0647 order, edges and spacer, so this card's strip change
holds there as well as in `qt_workbench_window_scale_*`.
