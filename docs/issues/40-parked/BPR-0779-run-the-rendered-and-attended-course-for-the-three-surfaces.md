---
schema: "implement-rfc.card.v1"
id: "BPR-0779"
title: "Run the rendered, keyboard and screen-reader course and the attended review for the four surfaces"
status: "Parked"
rfcs: ["RFC-BPR-0032", "RFC-BPR-0033", "RFC-BPR-0034"]
depends_on: ["BPR-0773", "BPR-0774", "BPR-0778", "BPR-0783", "BPR-0790"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["A8","B5","C6"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 - Captures exist for every named state at 100, 125, 150 and 200 percent in the four appearances with the Home column at most 720 points","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"artifact","detail":"400 captures = 25 named states x 4 appearances (Light, Dark, High-contrast Light, High-contrast Dark) x 4 scales (100, 125, 150, 200), in scratchpad/d7-captures-final, 45 MB, named <surface>-<state>-<appearance>-<scale>.png. Counted with find: 100 per scale; accounts 48, home 144, profiles 64, appearance 144. The scales genuinely applied: home-default-light at the four scales measures 1280x820, 1600x1025, 1920x1230 and 2560x1640. States: accounts {connected, signed-out, signing-in}; home {first-launch, default, long-list-scrolled, nothing-needs-me, over-a-review, group-none, group-organization, group-repository, group-account}; profiles {default, renaming, creating, failed}; appearance {default, editing, light-desktop, tile-0..tile-5}. Produced by the new qt_surface_capture_course_scale_{100,125,150,200}, which BUILT this course rather than extending one: RFC-BPR-0032 A8, RFC-BPR-0033 B5 and RFC-BPR-0034 C6 each speak of extending 'the existing four-appearance, four-scale capture course' and no such course existed - the four appearances were iterated inside individual translation units and the registered scale variants were 100/125/200 everywhere except qt_settings_window, which alone carried 150. Each state names itself before it is drawn, each raster is asserted non-null and exactly logical size x devicePixelRatio against BPR_EXPECTED_QT_SCALE, and a save that does not happen when a directory was named is a failure. 16/16 courses pass at zero pending build actions (scratchpad d7-int-ctest-04.log). UNRESOLVED, REFERRED TO THE OWNER: this card's clause 'with the Home column at most 720 points' contradicts its own RFC. RFC-BPR-0032 says 'Home is the window ground edge to edge, not a narrow column' and states no width anywhere; the only 720 in the tree is a component fixture's List.resize(720, 900). The course asserts the RFC's rule instead and reports the measurement the clause asked for: the column spans 1232 points at the 1280 capture width and 852 at the narrowest width the window supports, with horizontal scroll 0 at both. Which of the two is wrong is an owner decision, carried as candidate bpr-0779-home-column-width."}},{"criterion":"AC2 - Keyboard and VoiceOver walks reach every control with an accessible name; the profile row state is its description","outcome":"pending","revision":"ed2b3f14","evidence":{"kind":"command","detail":"THE AUTOMATABLE HALF IS DONE AND GREEN; VoiceOver ITSELF IS NOT AUTOMATABLE AND IS NOT CLAIMED. The course walks each surface's QAccessible tree - the tree a screen reader actually walks - in reading order and reports EVERY unnamed stop rather than only the first: Accounts signed out 14 stops, Accounts connected 16, Home 12, Profiles sheet 6, Appearance page 151, all with unnamed=0 at the accepted revision. RFC-BPR-0033 B5's 'the row state as its description' is asserted literally on Qt::AccessibleDescriptionRole of each profile row ('Open in 2 windows', 'Closed'). RFC-BPR-0032 A8's collapsed-panel announcement is asserted to report collapsed and not expanded while keeping its name. RFC-BPR-0034 C6's tile captions are asserted to appear in the strip's announcement. THIS WALK FOUND A REAL DEFECT, now fixed in ed2b3f14: colour_role_list's scroll well carried an object name but no accessible name, and a QScrollArea defaults to Qt::WheelFocus, which includes Tab, so it was a genuine Tab stop a screen reader reached unannounced - a direct miss of C6. Before the fix all four scale entries failed on exactly that one assertion (scratchpad d7-0779-ctest-all.log); after it 16/16 pass (d7-int-ctest-04.log). PENDING: the real macOS VoiceOver pass over these four surfaces is attended evidence and is carried as candidates bpr-0779-voiceover-accounts-home, bpr-0779-voiceover-profiles and bpr-0779-voiceover-appearance-tiles."}},{"criterion":"AC3 - The owner's macOS verdicts are bound to the executable digest; unobserved Windows and Linux items are recorded as not run","outcome":"pending","revision":"ed2b3f14","evidence":{"kind":"observation","detail":"PENDING BY CONSTRUCTION - this criterion is an attended verdict and cannot be satisfied by an agent. The review is PREPARED, not performed. Candidate binding: executable build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review, sha256 d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b, built from source revision ed2b3f14 at zero pending build actions; every attended candidate carries that digest as its revision so a verdict cannot drift onto a different binary. NOT RUN, and recorded as not run rather than as passing: native Windows and Linux empty-desktop quit behaviour. Only the injected policy outcome is proven, and native_empty_desktop_policy() answers stay_resident on this macOS host, so the quit branch is never taken here; RFC-BPR-0033's own verification note keeps that as platform-lane work 'recorded as not run, never as passing'. No card carrying platforms metadata exists for it, and none is invented on this lane. Also not run in this sandbox and unchanged by this card: the seven native courses recorded on BPR-0775 (credential store unavailable, a fixture seed failure, a stale pass expression, a hidden per-window Settings visibility check), and the three end-to-end appearance contracts, whose failure is proven independent of this pack by application_diff_workbench_public_path failing identically on the same binary with no appearance probe."}}]
---

# BPR-0779: Run the rendered, keyboard and screen-reader course and the attended review for the four surfaces

## Current frontier - BPR-0779 is human-pending; the pack stops here, bpr-shell-surfaces-20260915 d7 / rfc-integrator-d7 / mandate r3

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `verification-closeout`: obligations `BPR-0779:*`,
  `BPR-0780:*`, `BPR-0781:*`, `BPR-0782:*`, `BPR-0791:*` and both exits.
- **Candidate.** Local `main`, **nothing pushed**. In order: `cb7d088f` promote 0780, `02d01197` claim 0780,
  `2e240e1c` promote 0779, `62edc07d` claim 0779, `ed2b3f14` the capture course and its two fixes,
  `44b8cef1` the documentation, `24523e0d` finish 0780, `694e9594` finish 0779 (human-pending),
  `d494f31d` the closeout evidence. Tree clean, `build/dev` fully linked at 0 pending actions, no probe
  process alive.
- **What is done.** `BPR-0780` is **Done**. `BPR-0779`'s objective half is done: **400 captures** (25 named
  states x four appearances x four scales) and accessibility walks over all four surfaces with
  **unnamed=0**. The three closeout cards carry evidence or a named unresolved criterion on every row and
  each records its integration pass.
- **What is outstanding, and it is only the human.** 17 attended candidates are **prepared and ready**,
  session `e966c88f3320638208adf4fcf0651e31`. Nothing is inferred from a capture or a fixture.
- **The RFCs stay `Proposed`.** `BPR-0781`, `BPR-0782` and `BPR-0791` stay in Backlog with this card as an
  unmet dependency, which is the honest state while the verdicts they need are outstanding.

### What a successor must know

1. **No four-appearance, four-scale capture course existed.** All three RFCs say to *extend* one. The
   course was built here; the registered scales were 100/125/200 everywhere except `qt_settings_window`.
2. **The captures never compose Dif**, so they cannot answer the 40 px corner question. That candidate
   needs the real app.
3. **The walk found two real defects on already-accepted surfaces**, both fixed in `ed2b3f14`: the colours
   scroll well was an unnamed Tab stop (a direct miss of C6), and signed out, Home's bar drew its border
   around **527 points** of nothing against a size hint of **37**. Both are pinned by assertions that
   failed before the fix.
4. **`BPR-0779` AC1 contradicts its own RFC** on the 720-point Home column. Nothing was changed to satisfy
   it; the measurement it asked for is recorded (1232 at the capture width, 852 at the narrowest) and the
   conflict is an owner question.
5. **Three items the dispatch brief called "accumulated" were not recorded anywhere.** The two Appearance
   cosmetic departures are real *in the source* and are now recorded for the first time; the device-flow
   host claim is that `DeviceFlowHost` is assigned once and never cleared, which is by design, and what is
   genuinely missing is a course over the finished-on-A-then-begin-on-B sequence.

## Outcome

Extend the existing four-appearance, four-scale deterministic capture course to the Accounts page states, the Home states, the Appearance page states with every tile, and the Profiles sheet states; walk the keyboard order and VoiceOver names on macOS; prepare one attended candidate per surface, including the close-and-reopen course, through the shared attended-review workflow with absolute paths and one question per criterion.

## Acceptance

- Captures exist for every named state at 100, 125, 150 and 200 percent in the four appearances with the Home column at most 720 points.
- Keyboard and VoiceOver walks reach every control with an accessible name; the profile row state is its description.
- The owner's macOS verdicts are bound to the executable digest; unobserved Windows and Linux items are recorded as not run.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
