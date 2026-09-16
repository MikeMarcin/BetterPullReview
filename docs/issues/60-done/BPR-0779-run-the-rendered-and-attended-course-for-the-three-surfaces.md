---
schema: "implement-rfc.card.v1"
id: "BPR-0779"
title: "Run the rendered, keyboard and screen-reader course and the attended review for the four surfaces"
status: "Done"
rfcs: ["RFC-BPR-0032", "RFC-BPR-0033", "RFC-BPR-0034"]
depends_on: ["BPR-0773", "BPR-0774", "BPR-0778", "BPR-0783", "BPR-0790"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["A8","B5","C6"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 - Captures exist for every named state at 100, 125, 150 and 200 percent in the four appearances, with Home the window ground edge to edge as RFC-BPR-0032 requires","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"artifact","detail":"400 captures = 25 named states x 4 appearances (Light, Dark, High-contrast Light, High-contrast Dark) x 4 scales (100, 125, 150, 200), in scratchpad/d7-captures-final, 45 MB, named <surface>-<state>-<appearance>-<scale>.png. Counted with find: 100 per scale; accounts 48, home 144, profiles 64, appearance 144. The scales genuinely applied: home-default-light at the four scales measures 1280x820, 1600x1025, 1920x1230 and 2560x1640. States: accounts {connected, signed-out, signing-in}; home {first-launch, default, long-list-scrolled, nothing-needs-me, over-a-review, group-none, group-organization, group-repository, group-account}; profiles {default, renaming, creating, failed}; appearance {default, editing, light-desktop, tile-0..tile-5}. Produced by qt_surface_capture_course_scale_{100,125,150,200}, which BUILT this course rather than extending one: no four-appearance, four-scale course existed, contrary to all three RFCs' wording. Each state names itself before it is drawn, each raster is asserted non-null and exactly logical size x devicePixelRatio against BPR_EXPECTED_QT_SCALE, and a save that does not happen when a directory was named is a failure. 16/16 courses pass at zero pending build actions (scratchpad d7-int-ctest-04.log). RESOLVED IN d8, AND THE CRITERION TEXT IS AMENDED: this row previously read 'with the Home column at most 720 points', which contradicted its own RFC. RFC-BPR-0032 says 'Home is the window ground edge to edge, not a narrow column' and states no width anywhere; the only 720 in the tree is a component fixture's List.resize(720, 900). The course asserted the RFC's rule and reported the measurement the clause asked for: 1232 points at the 1280 capture width and 852 at the narrowest width the window supports, horizontal scroll 0 at both. Attended candidate bpr-0779-home-column-width, decision 55acaf73-844f-4771-9739-99bdf8bc6281, 2026-09-16T07:40:12Z: 'The RFC governs - Home is edge to edge; the card's 720 clause is an authoring error', with the note 'No idea what this is talking about'. Nothing in the code changed; the clause is corrected here and in the card's Acceptance list."}},{"criterion":"AC2 - Keyboard and VoiceOver walks reach every control with an accessible name; the profile row state is its description","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"command","detail":"BOTH HALVES ARE NOW DONE. Automatable half, at ed2b3f14: the course walks each surface's QAccessible tree - the tree a screen reader actually walks - in reading order and reports EVERY unnamed stop rather than only the first: Accounts signed out 14 stops, Accounts connected 16, Home 12, Profiles sheet 6, Appearance page 151, all with unnamed=0. RFC-BPR-0033 B5's 'the row state as its description' is asserted literally on Qt::AccessibleDescriptionRole of each profile row ('Open in 2 windows', 'Closed'). RFC-BPR-0032 A8's collapsed-panel announcement is asserted to report collapsed and not expanded while keeping its name. RFC-BPR-0034 C6's tile captions are asserted to appear in the strip's announcement. That walk found a real defect, fixed in ed2b3f14: colour_role_list's scroll well carried an object name but no accessible name, and a QScrollArea defaults to Qt::WheelFocus, which includes Tab, so it was a genuine Tab stop a screen reader reached unannounced - a direct miss of C6. Before the fix all four scale entries failed on exactly that assertion (scratchpad d7-0779-ctest-all.log); after it 16/16 pass (d7-int-ctest-04.log). Attended half, 2026-09-16, on executable sha256 d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b: the owner ran real macOS VoiceOver over all three surfaces and ACCEPTED each - bpr-0779-voiceover-accounts-home (decision 506a1c44, 07:42:51Z), bpr-0779-voiceover-profiles (86bd6b22, 07:42:34Z), bpr-0779-voiceover-appearance-tiles (6825fbc6, 07:42:54Z). No VoiceOver behaviour is inferred from the QAccessible walk; the walk and the verdict are separate evidence and both are present."}},{"criterion":"AC3 - The owner's macOS verdicts are bound to the executable digest; unobserved Windows and Linux items are recorded as not run","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"observation","detail":"THE REVIEW RAN AND EVERY VERDICT IS RECORDED. Session e966c88f3320638208adf4fcf0651e31, 17 candidates, 17 decided, handed back 2026-09-16T07:43:06Z at sequence 70 with an empty pending list. Binding: every candidate carried revision 'ed2b3f14 / app sha256 d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b' and each decision carries its candidate fingerprint, so a verdict cannot have drifted onto a different binary. That digest matches build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review as it stands. ACCEPTED OUTRIGHT (10): bpr-0779-accounts-rendered (d0bb147d), bpr-0779-home-rendered (521502f7), bpr-0779-profiles-rendered (ef0a7de2), bpr-0779-appearance-rendered (c5349059), bpr-0779-macos-colour-panel (1b80dbd8), bpr-0779-dif-corner (123f144b, which settles owner amendment r2), bpr-0779-attention-tones-wording (f9a2a39b), and the three VoiceOver walks recorded under AC2. ANSWERED AS SHIPPED (5 select verdicts): bpr-0779-appearance-cosmetics 'Accept both as shipped' (c85f2679); bpr-0779-filter-and-refresh-wording 'Accept all three as shipped' (910af65f); bpr-0779-review-followups 'Accept both as shipped' (3ed46318); bpr-0779-session-failure-discoverable 'Acceptable as shipped' (5376adc5); bpr-0779-a5-nothing-else 'Keep Recently viewed when signed out and there is public reading history' (c8e02af0), which is recorded in RFC-BPR-0032 A5 as owner direction. TEXT CORRECTED (1): bpr-0779-home-column-width, recorded under AC1. REJECTED (1): bpr-0779-empty-desktop-dock, decision f4a23296-6504-4503-8ebf-a9d66608bcb0 at 2026-09-16T07:34:00Z, note 'Closing last window closes menu bar.' (it superseded an identical reject a31780de with no note). That is a direct answer to the question BPR-0782 B2 carried as unresolved - whether the parentless qt::desktop_menu_bar really merges into the macOS menu bar with no window open - and it answers it against us. It is carried as BPR-0796 and it stops RFC-BPR-0033 closing. It does not leave this criterion unmet: this criterion asks that the verdicts be taken and bound, and they were. NOT RUN, AND RECORDED AS NOT RUN RATHER THAN AS PASSING: native Windows and Linux empty-desktop quit. Only the injected policy outcome is proven, and native_empty_desktop_policy() answers stay_resident on this macOS host, so the quit branch is never taken here. d8 allocated BPR-0798 (platforms: win) and BPR-0799 (platforms: linux) on main, so this no longer has nowhere to live. Also not run in this sandbox and unchanged by this card: the seven native courses recorded on BPR-0775, and the three end-to-end appearance contracts, whose failure is proven independent of this pack by application_diff_workbench_public_path failing identically on the same binary with no appearance probe."}}]
---

# BPR-0779: Run the rendered, keyboard and screen-reader course and the attended review for the four surfaces

## The attended review, adjudicated - bpr-shell-surfaces-20260915 d8 / rfc-integrator-d8 / mandate r5

The live frontier moved off this card when it closed. It is on [[BPR-0782]], the closeout that
remains open.

- **The session.** `e966c88f3320638208adf4fcf0651e31`, 17 candidates, all 17 decided, handed back
  2026-09-16T07:43Z at sequence 70. Every verdict is bound to executable sha256
  `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b` from source `ed2b3f14`, and
  each candidate carried that digest as its revision, so no verdict can have drifted onto another
  binary.
- **Sixteen cleared.** Ten were accepted outright: the four rendered surfaces, the three VoiceOver
  walks, the real macOS colour panel, Dif's 40 px corner, and the attention tones and row wording.
  Five wording and decision questions were answered "as shipped": the two Appearance cosmetic
  departures, Filter and refresh, session-failure discoverability, the two silent refusals, and A5's
  signed-out Recently viewed. The sixteenth was the 720-point ruling below.
- **Two verdicts changed text rather than merely clearing a row.** The 720-point conflict was ruled
  an authoring error in **this card**, not in the RFC, and AC1's clause is amended accordingly. A5's
  "nothing else" was ruled not to forbid the signed-out Recently viewed, and RFC-BPR-0032 A5 now says
  so in the owner's own direction.
- **One rejection.** `bpr-0779-empty-desktop-dock`: *"Closing last window closes menu bar."* That is
  the exact question [[BPR-0782]] B2 carried as unresolved, answered against us. It is [[BPR-0796]].
  It does not reopen this card - the review ran, the verdicts are recorded, which is what AC3 asks -
  but it does stop RFC-BPR-0033 closing.
- **The Windows and Linux empty-desktop quit now has a home**: [[BPR-0798]] and [[BPR-0799]], with
  `platforms` metadata, allocated on main. It remains **not run**, never passing.

### What a successor must know

1. **No four-appearance, four-scale capture course existed.** All three RFCs say to *extend* one. The
   course was built here; the registered scales were 100/125/200 everywhere except `qt_settings_window`.
2. **The captures never compose Dif**, so they cannot answer the 40 px corner question. That candidate
   needs the real app.
3. **The walk found two real defects on already-accepted surfaces**, both fixed in `ed2b3f14`: the colours
   scroll well was an unnamed Tab stop (a direct miss of C6), and signed out, Home's bar drew its border
   around **527 points** of nothing against a size hint of **37**. Both are pinned by assertions that
   failed before the fix.
4. **`BPR-0779` AC1 contradicted its own RFC** on the 720-point Home column. Nothing was changed to satisfy
   it; the measurement it asked for is recorded (1232 at the capture width, 852 at the narrowest). The
   owner ruled on 2026-09-16 that the RFC governs and the clause was an authoring error, so AC1 is
   amended and the code is untouched. Their note was "No idea what this is talking about", which is
   itself a finding: a card clause nobody recognises is a clause nobody wrote on purpose.
5. **Three items the dispatch brief called "accumulated" were not recorded anywhere.** The two Appearance
   cosmetic departures are real *in the source* and are now recorded for the first time; the device-flow
   host claim is that `DeviceFlowHost` is assigned once and never cleared, which is by design, and what is
   genuinely missing is a course over the finished-on-A-then-begin-on-B sequence.
6. **The owner reported two more defects outside the review**, from the same sitting on the github.com
   card: the Open browser button did not open a browser, and the GitHub CLI way in was unusable. They
   share one root cause and are [[BPR-0797]]. Neither falsifies an accepted criterion of this pack, and
   both gates predate it - but they are why the surfaces this card photographed cannot actually sign
   anyone in on a developer build.
7. **A rendered capture is not a native fact.** Three of this pack's claims turned out to rest on
   inference from an offscreen widget tree: the parentless menu bar ([[BPR-0777]] AC2, now falsified),
   and the two device and CLI seams above, which every existing course passes over because each injects
   its own stand-in. When a criterion says "on macOS", the course has to watch macOS.

## Outcome

Extend the existing four-appearance, four-scale deterministic capture course to the Accounts page states, the Home states, the Appearance page states with every tile, and the Profiles sheet states; walk the keyboard order and VoiceOver names on macOS; prepare one attended candidate per surface, including the close-and-reopen course, through the shared attended-review workflow with absolute paths and one question per criterion.

## Acceptance

- Captures exist for every named state at 100, 125, 150 and 200 percent in the four appearances, with Home the window ground edge to edge as RFC-BPR-0032 requires. This clause originally read "with the Home column at most 720 points"; the owner ruled on 2026-09-16 that the RFC governs and the 720 clause was an authoring error (attended candidate `bpr-0779-home-column-width`).
- Keyboard and VoiceOver walks reach every control with an accessible name; the profile row state is its description.
- The owner's macOS verdicts are bound to the executable digest; unobserved Windows and Linux items are recorded as not run.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
