---
schema: "implement-rfc.card.v1"
id: "BPR-0646"
title: "Workbench chrome: strip, epoch line, modalities and rails"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
parked_on: "human-review"
acceptance: [{"criterion": "AC-01 strip and epoch line are the only chrome; the strip is the title row", "outcome": "pass", "revision": "f01f34ad", "evidence": {"kind": "observation", "detail": "qt_workbench_window_scale_{100,125,200}: the title-row test pins ExpandedClientAreaHint, the hidden title text, the platform inset and the tracked drag; no review header scroll area remains (test_review_header_is_gone). Observed in the signed app-release bundle at 1280x820 and 1920x1080: the strip in the window's first row beside the traffic lights, the epoch line beneath, nothing else above the comparison."}}, {"criterion": "AC-02 epoch line selects any two uploads by drag, click, cmd-click, menu and keys and drives the comparison", "outcome": "pass", "revision": "f01f34ad", "evidence": {"kind": "observation", "detail": "qt_epoch_comparison_control: test_epoch_line_selects_updates_by_click_drag_and_keys walks click, cmd-click, drag, the context menu and Left/Right/Shift stepping and checks each dispatched select_comparison_anchor_intent; the amber last-review tick and its tooltip are pinned; the composed public-path course drives the comparison through the existing epoch host."}}, {"criterion": "AC-03 the four modalities set the drawers, persist per window and never change on their own", "outcome": "pass", "revision": "f01f34ad", "evidence": {"kind": "observation", "detail": "qt_workbench_window_scale_100: the modality tests pin Overview/Read/Review/Discuss drawer states, Ctrl+1..4, the 18-pixel rails with label and count, that a manual toggle leaves the segment until the next modality, and the per-window persisted modality."}}, {"criterion": "AC-07 sign-in reachable in every state without scrolling", "outcome": "pass", "revision": "f01f34ad", "evidence": {"kind": "observation", "detail": "qt_composed_github_scale_100: the device panel sits first on Settings > Accounts and hosts; the strip's account button opens it and focuses Get a code while signed out; test_home_landing_funnels_to_an_account_and_then_a_pull_request pins the Home funnel from the signed-out landing."}}, {"criterion": "Focused selectors, composition contracts, public-path courses and the boundary gate pass", "outcome": "pass", "revision": "f01f34ad", "evidence": {"kind": "observation", "detail": "28 of 28 focused selectors, application_composition_contract and application_review_round_trip_composition_contract (digests repinned), the public-path courses, check_boundaries, strict tidy on every touched translation unit, clang-format --Werror and verify_documentation.py green at f01f34ad."}}, {"criterion": "Attended capture on the Mac candidate with a pull request open: strip, epoch line with nodes, a collapsed and an open rail, positions, sign-in reachable signed out", "outcome": "fail", "revision": "2cdc0e57", "evidence": {"kind": "observation", "detail": "Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0646-workbench-chrome, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint 512c9a8467ffcde75a315a7bb661358ebac65aacff5d8c3fcda7a2cb64c13fa8, decision 8409ad43-a2a6-49e5-88c9-4317f141ebe1, actor human, 2026-09-11T23:43:16Z, two screenshots: reject. Verbatim: 'Epoc line appears, I think there's only one epoch on this pr so it's hard to tell if it's functional. But the epoch labels switch position in every mode and that is very disorienting. I think this is because epoch is sharing the same space as the drawers titles, but I think epoch and the comparing whole PR dropdown belong on a row above above those headers just below the title that is full app width.' The line now draws (BPR-0725 accepted); the handle criterion stayed unjudgeable because microsoft/terminal#20607 has one update; the finding is the line's placement inside the centre pane, carded as BPR-0737, which also names a multi-update pull request for the next ask."}}]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# Workbench chrome: strip, epoch line, modalities and rails

First slice of the owner-locked [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)
direction (mockup states 1 to 3). Replaced the 220-pixel review header scroll
area in `workbench_window.cpp` and the toolbar it carried; the strip is now
the window's title row.

## Scope

- The 30-pixel strip as the full-surface asset draws it (full ink, bordered
  buttons, two drawer toggles at the left, modality control alone in the
  centre) with the listed controls; the existing next/previous
  change and file actions move into it with their positions; jump to next new
  comment reads the ledger's unread projection.
- The epoch line over the existing epoch selectors: nodes per upload, labels
  beneath, bracket handles, blue span, amber "my last review" tick with a
  tooltip; drag, click, ⌘-click, right-click menu, hover detail, and keyboard
  stepping of each handle; the named comparisons in a dropdown.
- Modality presets Overview, Read, Review, Discuss with ⌘1 to ⌘4; drawers that
  collapse to 18-pixel rails with a vertical label and an unread count; no
  automatic collapse; a per-window persisted modality.
- The signed-out state shows the device sign-in (code, open browser, cancel)
  in the header without scrolling; the appearance settings panel moves into the
  Settings window drafted in `settings_window.{hpp,cpp}` under `Cmd+,` until the
  RFC-BPR-0018 design replaces it.
- The appearance roles, the diff colour themes and the DiffLinkMap painting are
  not changed by this or any RFC-BPR-0028 slice; the owner tuned them already.
- Core decides, host performs, Qt presents; no new authority in Qt. Owned
  paths: `src/adapters/qt` workbench window, session and device panels, epoch
  line and settings window, `src/app` composition wiring, their focused tests
  and the render evidence they pin.

## Acceptance

- RFC-BPR-0028 AC-01, AC-02, AC-03 and AC-07 at 1280x852 and 1920x1080.
- `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `application_composition_contract` and the public-path courses that pin
  header render evidence pass; the boundary gate passes.
- Attended capture on the Mac candidate showing the strip, epoch line, a
  collapsed and an open rail, and the device sign-in reachable while signed
  out.

## Notes

- Slice (a), settings window and signed-out sign-in. `settings_window.{hpp,cpp}`
  mounts a modeless Settings window with the seven RFC-BPR-0018 pages under
  `Cmd+,` (View menu, `QAction::PreferencesRole`); the appearance panel is
  adopted onto its Appearance page and leaves `workbenchReviewHeader`. The
  queue landing moves into the files drawer. The device sign-in panel is laid
  first in the header column and is shown only while there is no accepted
  session (`no_profile`, `establishing_session`, `session_failed`,
  `session_cancelled`), so it fits inside the unscrolled 220-pixel header; the
  focus order walks it before the session chrome, matching reading order.
  `qt_composed_github_scale_100` now pins that placement, the fit, the
  hide-on-session and the show-on-establishing transitions;
  `qt_settings_window` pins the pages, the adoption and the shortcut. The
  220-pixel `HeaderScroll` and the toolbar stay until the strip slice replaces
  them, so the `qt_workbench_window_scale_*` link-map evidence and the app
  composition digests are unchanged.
- Slice (a) verification, 2026-09-06: `qt_composed_github_scale_100`,
  `qt_settings_window`, `qt_workbench_window_scale_{100,125,200}`,
  `qt_appearance_*`, `qt_device_authorization_*`, `qt_github_session_*`,
  `application_composition_contract`, the appearance, queue, ledger,
  round-trip and workbench composition contracts, the device authorization
  course, the github.com public path and `architecture_boundaries` pass;
  strict tidy over the touched translation units and `clang-format --Werror`
  are clean; the signed `app-release` bundle builds. The attended capture of
  the new build could not be taken in that session because the Mac's screen
  was locked and the displays asleep; it stays owed to this card's acceptance.
- Slice (b), the strip. `workbench_strip.{hpp,cpp}` replaces the icon toolbar
  with RFC-BPR-0028's 30-pixel row: a `QToolBar` subclass that paints the bar
  and its rule, a `QProxyStyle` its presenters adopt (22-pixel bordered
  buttons, soft accent fill when checked, solid accent for the modality
  segments, disabled ink at 140 alpha rather than the platform's grey, an
  inset focus ring), painted line glyphs in the palette's ink, a divider, and
  the `modality_control`. Row order, left to right: files, identity, rule,
  previous and next change with "change 3 of 11", previous and next file with
  "file 3 of 40", spacer, the modality control, spacer, review details,
  private progress, refresh, comments, rule, account, settings. The change
  position is derived from the comparison surface's held result and
  selection exactly as the host derives the cursor it observes; the file
  position is the core's navigation cursor. Overview, Read, Review and
  Discuss (⌘1 to ⌘4) dispatch two exact `set_workbench_drawer_intent`s
  (Overview: files open, comments closed; Read and Discuss: files collapsed,
  comments open; Review: both open) and only when the core offers the drawer
  commands; the segment stays where it was chosen through a manual toggle.
  The account button reaches the device sign-in while signed out and the
  accounts settings page once signed in; refresh is the session chrome's own
  action. The window's width floor rises from 480 to 860 (820 before the Home button) so no strip control
  ever folds into an overflow menu; the identity yields below 1180, the
  positions below 960, and the modality segments drop the shortcut beside
  their names below 1000. Not in this slice: the "next new comment" group,
  because no unread projection reaches the Qt layer yet (the ledger's
  `UnreadConversations` is never set by the host and the queue's unseen
  count collapses to a bucket); it lands with the unread projection under
  BPR-0648. Rails, the 18-pixel collapsed drawers, Discuss's widening and
  per-window persistence of the modality are the next slice. The
  `workbench_window_tests` widget-count sanity bound moved from a hundredfold
  to a fiftyfold ratio (609 widgets against 60,000 lines); the equalities
  that make the claim are unchanged. The UIforETW capture probe pins the new
  nineteen-entry row; both application digests repinned.
- Slice (b) observation, 2026-09-06, signed `app-release` bundle at 1280x852
  on the dark palette, signed out: the strip is one 30-pixel row under the
  title bar with the files toggle checked at the left, the change and file
  pairs as bordered chevron buttons, the four segments "Overview ⌘1", "Read
  ⌘2", "Review ⌘3" (filled) and "Discuss ⌘4" alone at the centre, and
  review details, private progress with its menu arrow, refresh, comments
  (checked), a rule, account and settings at the right; the identity and
  positions are absent because no pull request is open. The device sign-in
  panel is the top of the header beneath it. Captured with the computer-use
  window screenshot; no image retained.
- Slice (b2), the strip as the title row (owner direction, 2026-09-06: "This
  title bar is wasting a ton of space, even on windows title bars tend to be
  ultra compact now", with Fork, Visual Studio Code on both platforms,
  Windows Terminal, Ghostty and the Claude app as references, Windows Visual
  Studio Code the closest). RFC-BPR-0028's chrome paragraph and AC-01 now say
  the strip is the row the window controls sit in. On macOS the window takes
  `Qt::ExpandedClientAreaHint` and `Qt::NoTitleBarBackgroundHint`, declines
  the safe-area margin, and hides the title text once shown through
  `native_window_chrome.cpp` (the Objective-C runtime from C++, guarded to the
  Cocoa platform so the offscreen tests are untouched; the title stays set for
  the Dock and Mission Control). The strip's first item is a platform inset,
  78 pixels on macOS and zero elsewhere, so the traffic lights sit in the
  strip and Files follows them; the strip tracks a drag that starts on its
  bare surface (spacers, inset, dividers, labels; never a control) and moves
  the window under the hold, and double-clicking that surface zooms and
  restores. Qt's system move was tried first and did nothing for a real
  drag on this Mac, and it cannot be proved offscreen; the tracked drag is
  pinned by `test_strip_is_the_title_row`. The Windows form (compact menu
  at the left, caption buttons at the right, snap layouts through
  `WM_NCHITTEST`) is platform work of its own under BPR-0653. Observed in the
  signed bundle at 1280x820: no title bar, the traffic lights in the strip's
  left inset, Files at 86 pixels, the segments centred, settings at the
  right edge; the window is 32 pixels shorter in chrome than before. A real
  60 by 80 point drag on the spacer moved the window by exactly that, a
  double-click on the same surface zoomed it to the screen and a second
  double-click restored 1280x820 at its previous position.
- Slice (c), the rails. `workbench_drawer_rail` is an 18-pixel button the
  drawer panel shows in its collapsed state instead of leaving the surface:
  a rule on the edge the drawer reopens from, the drawer's name vertically
  ("Files", "Comments") with a summary beside it, a focus ring, and a click
  that the window turns into the drawer's own toggle action, so the core
  still decides. The panel hides everything in its column but the rail and
  fixes its width to the rail's; expanded restores the host's minimum and
  the persisted width. The centre sizing gives a collapsed drawer the rail's
  width. The files rail states the selected file's place among the
  actionable files from the core's cursor; the comments rail states the
  review thread count from the pull request projection; the unread count the
  design names waits for its projection. A restored drawer layout sets the
  modality segment (files open and comments collapsed reads Overview, files
  collapsed and comments open reads Read unless Discuss is showing, both
  open reads Review) until a modality is chosen in the window, after which
  a manual toggle leaves the segment alone. Pins that said a collapsed
  drawer is hidden now say it is its rail: the collapse tests, the restart
  test, the composed agent-surface test, and the UIforETW focused-layout
  probes.
- Slice (e), no review header (AC-01). The 220-pixel scroll area and its
  host widget are gone: the central widget is the splitter, so the chrome
  above the comparison is the strip and the epoch line only. The device
  sign-in, the session chrome (in its compact layout), the queue landing,
  the pull request summary (bounded to 200 pixels) and its description
  (hidden until a pull request is open) live at the top of the files drawer,
  in that order, above the files list, until the repositories drawer
  (BPR-0651) and the accounts page (BPR-0652) take them. The files drawer
  presents expanded whenever the core offers no drawer command, so sign-in
  and the locator can never be shut behind a rail the reviewer could not
  reopen; the core's drawer state is untouched and returns with the
  command. Review details now toggles the epoch line and the selected-change
  details only, and says so. The composed device sign-in test, its render
  crop, the review-details test, the settings window test and the UIforETW
  probes moved with the panels; the probe's "review_details_rect_in_window"
  now measures the epoch line.
  Observed in the signed bundle once the screen was back: the chrome above
  the panes is the strip and the "Updates" row only, and the detail pane is
  hidden by default; the files drawer, however, had its sign-in, session
  chrome, queue and pull request panels squeezed into each other because the
  column exceeded the window height. The top region is now a bounded
  scrolling area (`workbenchDrawerTopScroll`, 420 pixels at most) holding
  those panels at their natural heights, sign-in first, with the files list
  beneath it.
- Slice (d), the epoch line. `epoch_line.{hpp,cpp}` is RFC-BPR-0028's line:
  one node per review epoch (each update that changed the pull request,
  never a commit or a file's history), oldest at the left, the two compared
  anchors as bracket handles with the accent span between them, the dates in
  their own row beneath the nodes ("Sep 6", the newest ", latest"), thinned
  to the ends, the handles and my last review when the history is dense,
  and an amber tick for my last review with only a tooltip (the head commit
  of the current viewer's review from the authority projection). Click a
  node compares from it (its head on the left), ⌘-click compares to it,
  right-click offers compare from here, compare to here, show this update
  alone and copy head commit, either handle drags, and with the line focused
  Left and Right step the left handle one stop at a time while Shift+Left
  and Shift+Right step the right handle; the left handle's stops begin with
  the oldest update's base so the whole pull request is one step from its
  first head. Hovering a node states the update's ordinal, cause, time and
  head commit (the projection carries no per-update file list yet). Every
  gesture leaves as one exact `select_comparison_anchor_intent` through the
  control's dispatch and the line draws what the core publishes back. The
  epoch control now shows the line as its one always-visible row, with
  "Updates" at its left and a "Comparing …" menu at its right that carries
  the three named comparisons and the history and maintenance actions; the
  exact selectors, OIDs, status and maintenance buttons are its details
  region, shown by the review details action, which is off by default and
  now also governs the selected-change detail pane. The epoch and workbench
  tests that assumed the selectors or the detail pane visible turn review
  details on first.
  The device sign-in panel's four buttons wrap to two rows of two, because
  the 320-pixel drawer clipped the last of them in one row. The public-path
  course in the application's main turns review details on before it drags
  the vertical splitter, since the detail pane it bounds is now off by
  default; both application digests repinned. Observed in the signed bundle
  at 1280x820: the "Updates" row reads "No updates loaded." with "Comparing
  exact anchors" at its right when nothing is open, the detail pane is
  hidden, and the drawer's top region shows sign-in, host and locator at
  their natural sizes with the files list beneath. The Review segment reads
  checked at launch (an instrumented run confirmed the button state; two
  earlier captures had shown it unfilled and could not be reproduced).
- Standing at e9544893 (2026-09-06): slices (a) Settings window and sign-in
  first, (b) the strip, (b2) the strip as the title row, (c) rails, (e) no
  review header, (d) the epoch line are committed; 28 of 28 focused
  selectors, the composition contracts, the public path, strict tidy on
  every touched translation unit and the docs gate pass at this head. Owed
  before this card can finish: the attended capture with a pull request open
  (the rails with their counts, the epoch line with nodes and handles, the
  change and file positions, 1280x852 and 1920x1080), which needs a signed-in
  session; the stored device grant expired (BPR-0644) and the device flow
  needs the owner's browser. Out of this card by design: the "next new
  comment" group (BPR-0648, no unread projection yet), Discuss's widening
  (BPR-0649), the Windows caption (BPR-0653).
- Correction, 2026-09-06 (owner: "all the configuration moved to the files
  drawer instead of the settings menu as designed"). The device sign-in and
  the session chrome (host picker, Connect, sign out, public preview,
  details, the locator and refresh) are now the Settings window's Accounts
  and hosts page (`workbenchAccountsPage`), sign-in first while signed out;
  the strip's account button presents that page and focuses "Get a code"
  while signed out. Nothing presents the page on its own: an automatic
  presentation on a begun flow took activation from the workbench in the
  middle of the queue composition course, so a flow is begun from the page
  the reviewer already opened. The drawer's top region scrolls
  horizontally when the queue landing's rows need more than the drawer's
  width, and the empty state's guidance now points at the account button. The files drawer's top region keeps only the pull request
  list (the queue landing) and the pull request summary, which are not
  configuration and which the repositories drawer and the overview will
  take. The workbench declares no focus stops across windows. The session
  panel's diagnostic report and summary now read their label relative to
  the panel rather than to the screen, since the panel lives in a window
  that is usually closed; the composed tests that inspect the session chrome
  present the accounts page first.
- Home's landing (owner direction, 2026-09-06: the default landing, Home,
  should carry a connect-account flow or funnel to the right settings page
  when no account or repository is configured). `workbench_landing` is a
  card at the top of the centre, above the epoch line, shown only while no
  pull request is open, no comparison is held and no load is under way.
  Signed out it reads "Connect a GitHub account" with "Sign in to GitHub…",
  which opens Accounts and hosts with "Get a code" focused, and "Accounts &
  hosts settings". Signed in with nothing open it reads "Open a pull
  request" with "Choose from the queue" (focus moves to the queue) and
  "Enter a pull request…" (Accounts and hosts with the locator focused).
  Its buttons are focus stops before the epoch line. BPR-0650's Home
  action list builds on this surface once there is something to act on.
  Pinned by `test_home_landing_funnels_to_an_account_and_then_a_pull_request`.
- Home (owner, 2026-09-06: "that screen is so full of stuff that makes zero
  sense until you've picked a PR it may as well be hidden"). The centre is
  a stack: Home (`workbenchHome`) while no pull request is open, no
  comparison is held, no load is under way and no workbench is active; the
  review workbench (drawers, epoch line, panes, review editor) otherwise.
  Home holds the landing card, the pull request stack (whose empty page is
  where a failed load says why), a "Review queue" heading and the review
  queue with its filters and rules, given the room the drawer never could.
  The pull request stack is one widget moved between Home and the drawer's
  top region as the page changes, so the projections it receives have one
  target. The files drawer holds files only. Pins that expected the queue
  in the reachable tab chain with a pull request open now claim its order
  on the full declaration; the review-details test activates the workbench
  first. BPR-0650's grouped action list builds on this page. Home is decided
  by whether the core offers the drawer command (the host publishes every
  command refused at launch, which is not an active workbench), and on Home
  while signed out the pull request stack's empty page, which would only
  restate the card, is shown only when it carries a diagnostic. Observed in
  the signed bundle at 1280x820: the strip, then Home alone -- the card,
  "Review queue", the queue's list, filters and rule editor -- with no
  drawers, epoch line, panes or review editor.
- Home's strip and the Home button (owner, 2026-09-06: the four modalities
  make no sense on Home; there should at least be a Home button). On Home the
  strip keeps only Home, account and settings (and the inset and spacers
  that place them); drawers, navigation, the modalities, review details and
  refresh are a pull request's and return with one. Hiding a QAction also
  disables it, so "a workbench is active" now means any command the core
  offers, not the drawer command alone. The Home button sits after the
  platform inset, before Files, with ⌘0, checked while Home shows; pressed
  over an open review it shows Home with the modalities kept as the way
  back, and a modality or a newly opened pull request returns to the
  review. Decided by the owner on 2026-09-07: Home stays a view with its
  button and is not a modality. Whether Home should instead be a fifth modality in the segmented
  control is left for the owner; the button answers the need either way.
  The width floor rises to 860 for the extra button; the UIforETW probe
  pins the twenty-one-entry row.
- Back to the review (owner, 2026-09-06: an accidental Home must not leave
  a reviewer hunting for the pull request they just left; a prev/next pull
  request cycle was raised and deferred to the repositories drawer's list
  order under BPR-0651). Home over an open review closes nothing, and now
  says so: the card at the top reads "Reviewing owner/repo #n title" with
  "Back to the review", which returns to the same file and change, and
  "Choose another pull request", which moves to the queue. This is the
  first row of BPR-0650's action list arriving early. Pinned in the
  composed Home test.

## Attended verdict, 2026-09-07: rejected

The owner ran the session at f492706b and rejected the capture. They did not
judge the chrome itself, because the way in defeated them first and the
workbench they reached was empty. In their words: "the app is so shit I cannot
judge it", "The public preview is buried deep in menus and looks innocuous, I
could barely find it", "we had described the UX as allowing pasting a URL but I
still have to manually split it myself", "that PR entry process is in the
settings menu instead of on the home", and "the entire set of information for
this PR is being crammed into a tiny box in files browser".

Carded: [BPR-0669](../60-done/BPR-0669-open-a-pull-request-by-pasting-its-url-from-home.md)
for URL entry on Home, [BPR-0670](../60-done/BPR-0670-public-preview-is-buried-and-does-not-say-what-it-is.md)
for the signed-out path, [BPR-0671](../60-done/BPR-0671-the-pull-request-summary-is-crammed-into-the-files-drawer.md)
for the summary box this card left in the files drawer, and
[BPR-0667](../60-done/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md)
and [BPR-0668](../60-done/BPR-0668-a-failed-comparison-leaves-an-empty-workbench-that-says-nothing-is-open.md)
for the empty workbench. The capture is re-offered only once a reviewer can
reach a comparison; there is nothing to photograph until then.

## Attended verdict, 2026-09-07 (second run): rejected

The owner ran the session again on the repaired build (a066705f, head
8ba0cb52) and rejected the capture a second time, with a screenshot kept
at
[captures/owner-overview-2026-09-07.png](../../verification/reviews/rfc-bpr-0028-attended-v001/captures/owner-overview-2026-09-07.png)
and this note, in full:

> Homepage flow is better. Although it'd be nice of this review page put
> the url it wants me to paste in a copy box like it did the command. It'd
> also be nice if for a screenshot I could just paste an image into this
> comment box rather than having to save one as a file and search my
> computer to attach it.
>
> As for the actual criteria the title row looks pretty good.
> The epoch line is quite hard to see if it's working properly or not.
> I cannot figure out how to collapse a drawer at all.
> Overview mode should have all drawers collapsed by default, or maybe
> always. Those drawers don't really make sense in overview imo.

What the capture also shows, which the note does not name: the pull
request loads and the review history is ready, so
[BPR-0667](../60-done/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md)
worked, but the status line still reads "Its comparison is not ready" and
the overview is squeezed into a column about 150 pixels wide between the
two drawers, with its own headings truncated to "Techni…etails" and
"Compari…hole PR".

Carried forward: [BPR-0674](../60-done/BPR-0674-overview-is-starved-by-the-drawers.md)
for the overview's width and the drawers in Overview,
[BPR-0675](../60-done/BPR-0675-a-drawer-cannot-be-collapsed-by-anyone-who-has-not-read-the-code.md)
for collapsing a drawer,
[BPR-0676](../60-done/BPR-0676-the-epoch-line-does-not-show-whether-it-is-working.md)
for the epoch line, and
[BPR-0677](../60-done/BPR-0677-the-comparison-stays-unavailable-after-the-history-is-ready.md)
for the comparison. The two review-page requests are AgentSkills ASK-0020.
The title row is accepted: "the title row looks pretty good".

## Attended candidate, 2026-09-10

Prepared again at the pack head in
[rfc-bpr-0028-attended-v002](../../verification/reviews/rfc-bpr-0028-attended-v002/operator-packet.md)
(bundle executable sha256 `484b2072…`), under plan `ux-redesign-pack-0028`.
The v001 packet is historical; no verdict is recorded on this card yet.

## Attended verdict, 2026-09-10

Session `cbd48b91…`, sequence 104, decision `bdd9244a`, actor human, on the
candidate above: **reject**. Verbatim:

> Strip is good enough.
> Can't open any files. Saw no epoch line.
> Flow to open the PR is hella confusing, I don't know why I have to go into the settings menu just to open a PR without an account. That should be doable directly from Home.

The decision carries no attachments; the two screenshots in the session
state belong to the v001 rejection of 2026-09-07. Read as three findings:
the strip (AC-01) is accepted; the files and the epoch line were unreachable
because the signed-out open no longer reaches a comparison, bisected to
`826fb871` and carded as
[BPR-0715](BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md),
which is not this card's code; and the Home flow is owner direction under
RFC-BPR-0026, registered as
[BPR-0716](../60-done/BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md).
The other nine candidates have no decision; BPR-0663, BPR-0667, BPR-0676 and
BPR-0677 need the same loaded review and could not be judged on this run.
This card stays parked: its remaining criterion needs the capture again at a
head where BPR-0715 is fixed, in a session prepared again.

## Attended candidate, 2026-09-10 (v003)

Prepared again in
[rfc-bpr-0028-attended-v003](../../verification/reviews/rfc-bpr-0028-attended-v003/operator-packet.md)
on the bundle built at the
[BPR-0715](BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md)
fix head (executable sha256 `9334b13a…`), candidate revision
`app-release-9334b13a-2026-09-10.2`. The v002 rejection above stays bound
to the v002 revision in the session history; the remaining criterion is
undecided on this revision. The v002 packet is historical.

## Attended verdict, 2026-09-10 (v003)

Session `cbd48b91…`, sequence 140, decision `65f3f881`, actor human,
2026-09-11T05:34:51Z, on the v003 candidate (fingerprint `4613c22c…`,
bundle `9334b13a…` built at da92d5a9): **reject**, with three screenshots.
An identical reject (`b1348a9e`, sequence 138) was withdrawn at sequence
139 and is superseded; nothing here depends on it. The session was handed
back at sequence 141 with this the only decision; the other nine candidates
are undecided and stay protected. Verbatim:

> Launching the app auto opened a review. I have no easy way to tell what review it is. Seems like I cannot open the ScreenBufferTests.cpp file in the review but I can open some other ones. Even in read mode viewing experience is not great. Also comments a grey instead of green like was requested with visual studio dark mode with visual assist color scheme. And includes are green not grey, seems like those colors got swapped maybe? Or the theme in general just got messed up.
>
> The drawers on the left are taking up way too much window space when collapsed. I'm thinking the top buttons in the title bar area are sufficient and we don't need the drawer columns or even the hide byttons on the drawers themselves. Probably on the right side as well. I dont think I want to fully remove them just yet though, can we make a setting to enable/disable the drawer controls (leaving just the title bar buttons when setting is disabled)?
>
> Ah I see there is some kind of limit that prevents us from seeing the one file. That's not going to work. We cannot have limits like that we need to be able to see every file in the reviews.

What the screenshots show, at about 1,390 points wide with
microsoft/terminal#20607 restored at launch: the strip as the title row
with "change 1 of 6" and "file 3 of 4" but no identity; the Updates row
reading "No updates loaded." beside "pair ready" and "Comparing exact
anchors"; the files drawer with four files; `ScreenBufferTests.cpp`
(+291 −0) refused with "Comparison unavailable — file exceeds the
comparison work limit (cached)"; `terminalOutput.cpp` rendered as "Diff
ready — 6 semantic changes (cached)" in Review and Read, comments in grey;
the Conversations drawer open with the read-only notice; and, with Files and
Repositories collapsed, two 18-pixel rails each with a "…" above it. The
diagnostics log for that launch (05:19:48Z to 05:34:45Z) holds only the
restoration's `catalog_open`, `catalog_recovery` and `catalog_prepare`
stages, no acquire stage and no request; every comparison was served from
the cache. The owner did not take the packet's path through Home because
the review was already open.

Read as five findings, none of them this card's criterion as written:

- The refused file is the core's LCS matrix bound with no textual fallback,
  owner direction that no file may be refused for size:
  [BPR-0719](../60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md).
- Comment and include colours have not changed since BPR-0199 (2026-08-07)
  and were not swapped; the request is a requirement under RFC-BPR-0006 held
  for the owner's confirmation of its extent:
  [BPR-0720](BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md).
- The collapsed drawers' footprint, the setting to hide the drawer-side
  controls, and the heading row that `adopt()` leaves visible above each
  rail (the "…"):
  [BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md).
  When it lands, RFC-BPR-0028 AC-03's rail wording is amended.
- "No easy way to tell what review it is" is the strip hiding the identity
  below 1,400 pixels (BPR-0647's threshold):
  [BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md);
  Home over a restored review remains BPR-0716.
- "Even in read mode viewing experience is not great" names no defect; the
  owner is asked what was wrong before anything is carded.

The epoch line without updates is not this card's rail or line drawing; it
is recorded on [BPR-0663](../60-done/BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md)
as an observation, not a verdict. The strip's v002 acceptance stands. This
card stays parked on human-review; its criterion is asked again on a bundle
that carries BPR-0721 and BPR-0722, when the rails and identity it asks
about are what the owner will see.

## Attended candidate, 2026-09-10 (v004)

Prepared again in
[rfc-bpr-0028-attended-v004](../../verification/reviews/rfc-bpr-0028-attended-v004/operator-packet.md)
on the bundle built at 3c9a3f1c, where
[BPR-0719](../60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md),
[BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md)
and
[BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md)
have landed (executable sha256 `9261509a…`), candidate revision
`app-release-9261509a-2026-09-10.3`. The third criterion is reworded for
the drawer-controls setting: with it Shown a rail carries its label and
count; with it Hidden, the default, a collapsed drawer leaves no column.
The v003 rejection above stays bound to the v003 revision in the session
history; the remaining criterion is undecided on this revision. No host is
running until the owner asks for the session.

## Owner's answers, 2026-09-11

The three questions the v003 verdict left open were answered. "Even in read
mode viewing experience is not great" meant "it was difficult to read the
code because of a lack of horizontal area for the text display", carded as
[BPR-0724](../60-done/BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md).
The dark palette question: "the whole palette, yes reset", landed on
[BPR-0720](BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md).
The strip below about 960 pixels: "leave it be for now", recorded on
[BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md).
This card's remaining criterion is unchanged and is asked again on the
v005 bundle.

## Attended candidate, 2026-09-11 (v005)

The pending criterion above is candidate `BPR-0646-workbench-chrome` in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md)
on the bundle built from the source at 25e1525d (executable sha256
`d238842d…`), candidate revision `app-release-d238842d-2026-09-11.1`;
prepared, undecided, no host running until the owner asks for the session.
The v004 packet is historical.

## Attended candidate, 2026-09-11 (v006)

The pending criterion above is candidate `BPR-0646-workbench-chrome` in
[rfc-bpr-0028-attended-v006](../../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md)
on the bundle built from the source at d1ba791f (executable sha256
`cc2afa7f…`), candidate revision `app-release-cc2afa7f-2026-09-11.2`;
prepared, undecided, no host running until the owner asks for the session.
The ask is unchanged; the v005 packet is historical.

## Attended verdict, 2026-09-11 (v006): rejected

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0646-workbench-chrome`, decision `d06b4b43`, 18:30:10Z (an earlier `fa3f3955` at
18:27:53Z on the same fingerprint is superseded by it and adds nothing), one screenshot
([owner-read-chrome-2026-09-11.png](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-read-chrome-2026-09-11.png)): **reject**. Verbatim:

> Strip is good
>
> > A handle on the epoch line changes the comparison
>
> There is no visible epoch line
>
> > a drawer collapsed to its rail carries a vertical label and an unread count
>
> Rail is either hidden or removed entirely, it was supposed to be a setting but there's no setting that i can find.
>
> > Signed out, sign-in is reachable without scrolling
>
> Technically yes, but the flow is very awkward, we should be able to paste into the url box on the home sceen and click something like open without signing in and it should just work. The home screen is very messy.

The screenshot, at about 1,470 points wide with microsoft/terminal#20607 restored at launch in
Read: the strip naming the pull request, "change 2 of 2" and "file 2 of 4"; the Updates row
reading "No updates loaded." beside "pair ready" and "Comparing exact anchors"; the two left
drawers collapsed to no column; the conversations drawer open; `ScreenBufferTests.cpp` rendered
with "Diff ready — 2 semantic changes".

Read per criterion:

- The strip: accepted, as in v002 and v003.
- The epoch line: not judgeable, because the line drew no nodes. The diagnostics log for the
  launch records only `catalog_open`, `catalog_recovery` and `catalog_prepare` (a cache hit; no
  acquire), and the line gains rows only from an accepted append or a requested history page,
  neither of which a cache hit produces:
  [BPR-0725](../60-done/BPR-0725-the-epoch-line-shows-no-updates-when-the-catalogue-already-holds-the-head.md).
  The same finding rejected [BPR-0663](../60-done/BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md)
  and [BPR-0676](../60-done/BPR-0676-the-epoch-line-does-not-show-whether-it-is-working.md).
- The rails: retracted five minutes later on BPR-0675 ("In an earlier answer I said there was
  no setting for this, I just was looking in the wrong place. I found it now and it works"),
  and both states of the setting were accepted on
  [BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md).
- Sign-in without scrolling: "Technically yes"; the criterion as written holds. The flow the
  owner wants (paste into the bar on Home, press Open, no account) and "the home screen is very
  messy" are Home direction under RFC-BPR-0026, recorded on
  [BPR-0716](../60-done/BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md).

This card stays parked on human-review for the epoch line only; its candidate is asked again,
narrowed to that criterion, on a bundle carrying BPR-0725.

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0646-workbench-chrome` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: narrowed to the one criterion left, a handle on the epoch line changes the comparison, on a bundle carrying BPR-0725.

## Attended verdict, 2026-09-11 (v007): rejected

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0646-workbench-chrome`, decision `8409ad43`, 23:43:16Z, two screenshots
([Overview](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-epoch-overview-2026-09-11.png),
[Read](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-epoch-read-2026-09-11.png)): **reject**. Verbatim:

> Epoc line appears, I think there's only one epoch on this pr so it's hard to tell if it's functional. But the epoch labels switch position in every mode and that is very disorienting. I think this is because epoch is sharing the same space as the drawers titles, but I think epoch and the comparing whole PR dropdown belong on a row above above those headers just below the title that is full app width.

The screenshots, at about 1,600 points wide with microsoft/terminal#20607 open: in Overview the "Updates" row spans the centre pane between the files drawer's column and the participants panel, its one node "Sep 7, latest" at about 720; in Read, with the conversations drawer open at the right, the same node sits at about 500 and "Conversations drawer" heads the pane beside it.

Read against the one criterion left:

- The line draws now ([BPR-0725](../60-done/BPR-0725-the-epoch-line-shows-no-updates-when-the-catalogue-already-holds-the-head.md) was accepted in the same round, as were [BPR-0663](../60-done/BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md) and [BPR-0676](../60-done/BPR-0676-the-epoch-line-does-not-show-whether-it-is-working.md)).
- "A handle on the epoch line changes the comparison" is still unjudged, and for a reason no bundle can fix: the pull request the packet names has one update, so there is one node and nothing for a handle to move to. The next ask names a public pull request with several updates.
- The finding is placement: the epoch control is the first widget of the centre pane, so its extent follows the drawer widths and the modality (`workbench_window.cpp:775`, `:834`, `:1258-1261`, `central_sizes()` at `:6300`). The owner wants it and the "Comparing …" menu on one full-width row under the strip, above the drawer headings. Carded as [BPR-0737](../30-in-progress/BPR-0737-the-epoch-line-and-the-comparison-menu-are-one-full-width-row-under-the-strip.md); when it lands, RFC-BPR-0028's chrome paragraph says the line is full width.

This card stays parked on human-review for the handle criterion; it is asked again on a bundle carrying BPR-0737, on a pull request with more than one update.

## Owner answer, 2026-09-11 (mandate r9)

For the handle criterion, which microsoft/terminal#20607 could not make
judgeable, the owner named a public pull request with more than one
update: "https://github.com/NVIDIA/stdexec/pull/1846 looks to have
multiple updates". The v008 candidate for this card is asked on
NVIDIA/stdexec#1846, on the bundle that carries
[BPR-0737](../30-in-progress/BPR-0737-the-epoch-line-and-the-comparison-menu-are-one-full-width-row-under-the-strip.md).
