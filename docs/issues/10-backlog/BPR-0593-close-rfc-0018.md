---
schema: "implement-rfc.card.v1"
id: "BPR-0593"
title: "Verify and close RFC-BPR-0018"
status: "Backlog"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592","BPR-0678","BPR-0679","BPR-0680","BPR-0681","BPR-0682","BPR-0683","BPR-0684","BPR-0685","BPR-0686","BPR-0687","BPR-0688","BPR-0689"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0018"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0018

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0018](../../rfcs/RFC-BPR-0018-robust-settings-center.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 1 of 4.
Planned cards: BPR-0592, BPR-0593. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

The 2026-09-04 registration was subsequently activated by the owner's
2026-09-07 implementation direction in the canonical index. This closeout stays
in Backlog until its required acceptance is complete. The immutable base remains
the pre-registration commit; registration commits are planning history.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped application exposes a native Settings action and one singleton, modeless window per available BPR profile through platform-appropriate menus and shortcuts, with the edited profile visibly identified. | `qt_settings_window` menu, singleton, Default label and close course | Automated Default environment passes; target-native inventory pending | 68e0a40f, a881d9f9 |
| AC-02: All seven initial categories are present when their capabilities are composed, with stable sidebar/page/row identities and truthful unavailable states. | Seven-page composed matrix and agent/storage offered-model checks | Automated pass; unsupported capabilities remain explained | 51c24f6b, 6c0ba02b |
| AC-03: Appearance is migrated from the embedded workbench surface to the settings center with no duplicate authority; existing mode, typography, palette, import/preview/commit/cancel/reset, and persistence behavior remains intact. | Appearance host/store/Qt and application theme customization course | Pass, including export, scoped reset and actual Settings close | 03a8a02e, a881d9f9 |
| AC-04: Every setting gesture becomes one typed domain intent, reaches only its owning host, and updates the control only from an accepted authoritative projection. | Domain host refusal/late-result checks; queue field merge and composed intent checks | Pass on changed settings paths; medium review found no additional state defect | dd64c098, 51c24f6b, a881d9f9 |
| AC-05: Existing domain stores migrate and reopen from prior schemas; future, malformed, interrupted, and unavailable cases fail at their exact scope without corrupting another domain. | SQLite appearance v1/v2 and queue v5 migrations/reopen; malformed/future checks | Canonical migrations and pre-write malformed/future refusal pass | 03a8a02e, dd64c098, 22e30024 |
| AC-06: Credential, privacy, cache, notification, and agent actions retain their existing capability and secrecy boundaries. | Device flow, metadata-only notification preview, offered private opt-ins and exact maintenance course | Pass on fixture public paths; no live credentials/provider mutations | 01a3ada4, dd64c098, 51c24f6b |
| AC-07: Keyboard-only and screen-reader courses reach every sidebar item, row control, reset, confirmation, status, and close action in a natural order. | Qt keyboard, focus-return, page scroll, reset and Cancel courses | Automated checks pass; native screen-reader course pending | 68e0a40f, a881d9f9 |
| AC-08: Layout and rendered evidence pass at 100%, 125%, 150%, and 200% scale in Light, Dark, High-contrast Light, and High-contrast Dark, including long text and minimum window size. | `qt_settings_window` plus scale 125/150/200, each four appearances and seven pages; 112 captures | Automated layout pass at 760x480 with long metadata; attended visual verdict pending | 6c0ba02b |
| AC-09: One-setting updates preserve sidebar selection, page scroll position, focus, unaffected persistent indexes, and all unrelated projections without routine resets. | Stable page/scroll/focus and incremental workbench projections | Pass for one-setting changes and Settings reopen | 68e0a40f, dd64c098, a881d9f9 |
| AC-10: Hostile catalog, profile, rule, and row floods stop at declared limits with typed refusal; one window materializes at most one page per category; rapid previews coalesce; cancelled or superseded loads publish nothing late; and instrumentation observes zero database, vault, filesystem, network, process, or blocking-wait work on the Qt thread. | Appearance host cancellation/cache/coalescing, one page per category, architecture boundary checks | Collection limits, stable models, coalesced previews and cancellation pass; focused Settings capability-port instrumentation passes; exhaustive OS/native instrumentation remains unproven | 143abbc9, e354b45e, 29bb2980, a881d9f9, 6c0ba02b |
| AC-11: Tests-disabled packaged applications on macOS, Windows, and Linux expose the same setting inventory except for explicitly documented platform capability rows. | Tests-disabled target-native packaged inventory | Pending macOS/Windows/Linux current-candidate inventory; old isolated build is not this criterion | None |
| AC-12: Sign in starts supported device authorization directly and shows its code and browser action without Details, another Get code gesture, or pane scrolling; cancellation, expiry, and supported manual-token alternatives remain reachable. | Application device panel course from Home, GHES/absent refusal and Qt duplicate/cancel/expiry | Pass; supported flow starts directly, manual/session-only alternatives retained | 01a3ada4 |
| AC-13: User-configured servers and account selections survive restart through typed host persistence, while remembered grants reuse the existing vault path and session-only manual credentials are never presented as durable. | Saved server typed store/restart and existing device vault binding cases | Existing BPR-0660 and BPR-0679 evidence retained; manual tokens stay session-only | 01a3ada4 |
| AC-14: Configuration leaves the primary review surface compact, preserves tuned diff/appearance behavior, and exposes direct reconnect plus an exact pending destination handoff without changing credential or provider authority. | Compact workbench, composed GitHub, theme and direct reconnect paths | Pass: visible unknown-server Add/sign-in preserves exact target; cancel/stale stages preserve prior review, unavailable locations are explicit | 554564f8, 01a3ada4, a881d9f9 |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

The owner activated the broader UX work. BPR-0678 through BPR-0682, BPR-0684
through BPR-0688 are implemented. BPR-0689 retains exact setup destinations
and stages authentication without disturbing an existing review. BPR-0683 is
parked for required native/attended evidence, with its automated passes retained.
Continue RFC-0024 planning after the automated Settings integration boundary,
while preserving pending target-native/human criteria and canonical closeout order.
The current authorized workstream continuation is retained on [[BPR-0595]].

## Resumed implementation, 2026-09-07

The owner activated the broader UX frontier. [The authority inventory](../../development/settings-authority-inventory.md)
reconciles the integrated 0028 work and maps every criterion to BPR-0678 through
BPR-0683. These join BPR-0592 and BPR-0593 as the planned cards. Existing
partial implementation is not full criterion acceptance; not-run rows above
are superseded by the criterion ledger as each selected path is exercised.

BPR-0678: default-profile shell now uses stable per-page scroll containers,
retains panel identity and category scroll, identifies its default profile and
handles unconsumed Escape. Composed Qt checks pass at 100/125/200%; actual
760x480 offscreen Accounts/Reviews/Appearance captures inspected. This is
partial AC-01/02/07/08/09/10 evidence, not a full platform/accessibility verdict.
Two affected attended candidates (chrome and widths) were released by the
owner's resume-development authorization and need preparation after integration;
public-preview was subsequently released for BPR-0679 and also needs preparation.

BPR-0679: Home and the signed-out account action now start the existing device
flow directly. The executable panel course exercises Home, and the GHES
negative retains its no-request refusal. Running-flow duplicate, cancellation,
expiry and selected-host retry checks pass. Saved server authority/store and
session-only manual-token evidence from BPR-0660 remain unchanged. The
isolated tests-disabled build passed; one digest assertion was updated for
the reviewed course change and its bypass test passed separately. See the
card for commands and revision. Next: BPR-0680 appearance export/reset.

BPR-0684 repairs the Settings-close preview/import lifecycle discovered during integration preparation.

## Current integration evidence

At `3d36ce70` plus bookkeeping, both architecture checks and
`qt_composed_github_scale_100` passed (116.96s total). The actual application and
composed Qt target built successfully. Reused unchanged evidence: full core-only
125/125 at BPR-0681, selected domain host/store checks, device public course,
queue restart/pause course, theme customization 39.16s and maintenance 66.12s.
BPR-0685 adds all four scales/appearances with zero horizontal page scrolling;
112 deterministic offscreen rasters were generated and representative corrected
surfaces inspected. This does not stand in for screen-reader or target-native
packaged-app evidence.

One independent review was initially dispatched with the stale native role's
Astra high setting. It was interrupted on the owner's correction, and the same
bounded review continued with explicit Astra medium routing. The finding was
preserved: appearance legacy migration must validate before writing (BPR-0686).
The medium review reported no additional material finding in queue preference
merges, scoped resets, exports or close/import cancellation. No second review
loop is required; the repair receives its direct regression checks.

BPR-0686 fixed the migration finding (22e30024); malformed and future legacy
stores are refused before mutation. BPR-0687 bounds queue collections and keeps
accepted models and removal controls usable after refusal (e354b45e). Host/store
checks and Qt 100/125/200 passed, including maximum admitted collections.
BPR-0688 coalesces a burst to the latest preview and invalidates queued work on
close/reset (29bb2980); the panel and actual theme course passed. Documentation
verifier regression suite: 62 tests passed during this integration.

## Attended preparation, 2026-09-10

BPR-0683's three remaining macOS asks (AC-07 VoiceOver order, AC-08 the four
appearances at the real window, AC-11 the tests-disabled packaged inventory)
are one candidate in
[rfc-bpr-0028-attended-v002](../../verification/reviews/rfc-bpr-0028-attended-v002/operator-packet.md),
prepared under the adopted pack plan `ux-redesign-pack-0028`. Windows and
Linux packaged inventory stay pending on their stations. This closeout
remains in Backlog until BPR-0683 is Done.

## Attended preparation, 2026-09-10 (v003)

Prepared again, unchanged in substance, in
[rfc-bpr-0028-attended-v003](../../verification/reviews/rfc-bpr-0028-attended-v003/operator-packet.md)
on the bundle built at the BPR-0715 fix head (executable sha256
`9334b13a…`); the v002 packet is historical.

## Continuation after the v003 hand-back, 2026-09-10

Session `cbd48b91…` was handed back at sequence 141 with one decision, the
rejection of `BPR-0646-workbench-chrome` (decision `65f3f881`; `b1348a9e`
withdrawn and superseded). It is consumed on
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
and on RFC-BPR-0028, and became four cards: BPR-0719 (comparison work limit
refuses a file), BPR-0720 (dark palette comment and include colours, held
for the owner's confirmation), BPR-0721 (setting to hide the drawer controls
and the stray heading row above each rail) and BPR-0722 (identity hidden in
the strip below 1,400 pixels). `BPR-0683-settings-native-macos`, this
closeout's own candidate, and the eight other candidates carry no decision;
they stay ready and protected in the session, and their bundle inputs
(`src/adapters/qt`, `src/app`, the app-release bundle) stay guarded until
the owner says to resume development. Observations without verdicts are
recorded on BPR-0663 (no acquire stage in the restored run) and BPR-0715
(files and epoch line reached; log half pending).

Next, in order: ask the owner the three open questions (what was wrong in
Read mode; the extent of the BPR-0720 palette change; whether to resume
development now, releasing the nine undecided candidates for a rebuilt
bundle, or to review them on the current bundle first). Then, once released,
BPR-0719, BPR-0722 and BPR-0721 in that order as one milestone, a fresh
tests-disabled bundle, and a v004 packet regenerated last with the same ten
candidates plus the BPR-0719/0721/0722 asks; the packet is written only
after no active card can rebuild the binary.

## Continuation after the v003 follow-ups, first milestone, 2026-09-10

Dispatch `attended-prep-2026-09-10` d5 (mandate r2, "stop the attended
review session and fix the issues raised"). Session `cbd48b91…` was
stopped through the contract: `resume-development --all` released the
ten candidates (the nine undecided ones are `waiting`, unprotected, and
still pending on their cards; the BPR-0646 rejection `65f3f881` stays
recorded) and `stop` ended the loopback host (pid 24325). No watch is
armed.

Landed on main, in the recommended order:

- [BPR-0719](../60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md)
  (`901a9c06`): the core sheds the common prefix and suffix, charges the
  exact matrix for the changed region only, and degrades through the
  ordered-unique alignment and patience anchors instead of refusing;
  `computed_file_diff::AlignmentApproximate` names a degraded alignment
  and the Qt status line says "(approximate alignment)". Parked for the
  owner's check on ScreenBufferTests.cpp in microsoft/terminal#20607.
- [BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md)
  (`035b5322`): the strip identity yields title, owner, modality
  shortcuts and positions before its number, elided to the room the
  other items leave; the compact modality holds to 1100. Measured fact
  the card did not have: the floor is 900, not 860, and at 100% scale the
  other strip items alone lay out about 900 pixels, so below about 960
  the number has no room and the identity yields whole. Parked for the
  owner's check at the default size and at 1280x820.
- [BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
  (`6de81ee8`): `application_uiforetw_selected_diff_contract` prints
  `STARTUP_FAILED` 0.5 s into its first capture at `8ff3b664`,
  reproduced on a clean worktree build of that head. Not attributed to
  the follow-ups; it blocks re-running the 1280 compact-row and rail
  assertions that BPR-0722 and BPR-0721 touch.

Not started: [BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md).
Scouted facts for it: the heading row is a nested `QHBoxLayout`
(`workbench_drawer.cpp:399-419`) that `adopt()` (`:501-534`) never hides,
so `Heading` and `Collapse` must be toggled explicitly; the settings store
is already schema version 5 (`workbench_settings_database.hpp:31`), so the
flag is a version 6 column; a profile-level flag fits
`review_behaviour_settings` (`workbench_runtime_ports.hpp:478-488`) and
rides `combined_layout` unchanged, with the hand-built literal at
`src/app/main.cpp:6452` to extend; the Reviews & queue page is built by
`build_behaviour_editor` (`workbench_window.cpp:3614-3740`); the drawer
shortcuts are Alt+Shift+1/2/3, not ⌘1-4 as the card says; the UIforETW
contract asserts the 18-pixel rails, so a zero-width collapsed drawer by
default amends that contract as well as RFC-BPR-0028 AC-03 (`:86`,
`:90-91`, `:129-130`, `:168-169`, `:290-293`).

Next, in order: BPR-0721 as one milestone (defect first, then the setting
through store, projection, window and Settings page, then the RFC and
contract amendments); a fresh tests-disabled bundle; the v004 packet
regenerated last with the same ten candidates plus the BPR-0719/0721/0722
asks, written only after no active card can rebuild the binary. Owner
questions still open and carried forward verbatim: what specifically was
wrong in Read mode ("Even in read mode viewing experience is not great");
for BPR-0720, whether the whole dark syntax palette moves to the Visual
Assist scheme or only comments and includes, and whether the saved custom
palette is reset or left; and, new from BPR-0722, which strip control
should give way below about 960 pixels so the identity's number can stay
to the floor, or whether the floor moves to 960.

## Continuation after the v003 follow-ups, second milestone, 2026-09-10

Dispatch `attended-prep-2026-09-10` d6 (mandate r2). Landed on main:
[BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md):
the heading row no longer stays above a rail, and
`review_behaviour_settings::DrawerControls` (hidden by default, the owner's
direction) removes the rails and Hide buttons so the strip toggles are the
drawers' only controls; schema version 6; RFC-BPR-0028 AC-03 amended. Parked
for the owner's acceptance of both states. Measured facts: a panel that
leaves and rejoins a `QSplitter` is granted the room it last had regardless
of its fixed width, so the window re-applies `central_sizes()` (which now
works before a layout is published); BPR-0675's attended criterion now
needs the setting on. `application_workbench_composition_contract` stays
red at head (BPR-0718) and `application_uiforetw_selected_diff_contract`
was not rerun (BPR-0723); neither is attributed to this milestone.

Next: a fresh tests-disabled bundle and the v004 packet with the same ten
candidates plus the BPR-0719/0721/0722 asks, written only after no active
card can rebuild the binary. The three owner questions stand as recorded
above.

## Attended preparation, 2026-09-10 (v004)

The bundle was rebuilt at 3c9a3f1c (`cmake --preset app-release`, tests
disabled, executable sha256 `9261509a…`, signature verified) after
BPR-0721 was parked, so no active card could rebuild it. The session
`cbd48b91…` carries the ten v003 candidates at revision
`app-release-9261509a-2026-09-10.3` plus `BPR-0719-large-file-diff`,
`BPR-0721-drawer-controls` and `BPR-0722-strip-identity`: thirteen ready
and protected at sequence 182, none decided, in
[rfc-bpr-0028-attended-v004](../../verification/reviews/rfc-bpr-0028-attended-v004/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both. BPR-0683's candidate is this closeout's, unchanged in substance.

Next: on the owner's say-so, `serve` and a persistent `wait --after 182`;
then consume the verdicts card by card. Until then the only independent
work is BPR-0723 (the UIforETW startup failure), BPR-0718 (the red
workbench composition contract) and BPR-0720 once the owner answers; the
bundle inputs (`src/adapters/qt`, `src/app`, `src/core`, `src/adapters/sqlite`,
`src/application`) are protected by the thirteen candidates until the
owner says to resume development.

## Continuation after the owner's answers, 2026-09-10 (v005)

Dispatch `attended-prep-2026-09-10` d7 (mandate r3: the owner answered the
three open questions). The thirteen v004 candidates were released through
`resume-development --paths src/adapters/qt src/adapters/sqlite
--resources install:…` (all thirteen protect those paths; sequence 183)
before any source edit. Landed on main:

- [BPR-0724](../40-parked/BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md)
  (`25e1525d`): "it was difficult to read the code because of a lack of
  horizontal area for the text display". Read collapses the repositories
  drawer as well as the files drawer (300 pixels back to the comparison);
  what Read does with the conversations drawer (375-pixel minimum) or a
  single-pane layout is an options candidate, because the RFC's Read
  definition and AC-05 settle it the other way today. Parked.
- [BPR-0720](../40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md)
  (`c5bd59f8`): "the whole palette, yes reset". The dark syntax roles
  follow Visual Studio dark / Visual Assist; three values lightened to the
  4.5:1 floor on the diff fills (the audit and ratchet pass); the
  appearance store moves to schema 4 and clears `palette_stored` once on
  the way through (the owner's store holds a light-bound customisation at
  schema 3, so that is what the reset removes). Parked.
- [BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md):
  "leave it be for now" recorded as the decision to keep the 900-pixel
  floor and the whole-identity yield below about 960; its attended
  criterion is unchanged.
- [BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md):
  `application_appearance_theme_customization_contract` shares the
  `STARTUP_FAILED` signature at cc31119b on a clean worktree build, before
  this milestone's changes; recorded there, not attributed here.

The bundle was rebuilt after both cards were parked (executable sha256
`d238842d…`, signed, tests disabled; a rebuild at 25e1525d is a no-op) and
the session `cbd48b91…` carries the thirteen v004 candidates at revision
`app-release-d238842d-2026-09-11.1` plus `BPR-0720-dark-palette` and the
options group `BPR-0724-read-width`: fifteen ready and protected at
sequence 228, none decided, in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both.

Next: on the owner's say-so, `serve` and a persistent `wait --after 228`;
then consume the verdicts card by card (a BPR-0724 selection other than
"leave Read as it is" is a new bounded slice; the first option amends
AC-05). Until then the only independent work is BPR-0723 (now also the
theme customization contract) and BPR-0718; the bundle inputs
(`src/adapters/qt`, `src/adapters/sqlite`, `src/app`, `src/core`,
`src/application`) are protected by the fifteen candidates until the
owner says to resume development.

## Continuation after the two red contracts, 2026-09-11 (v006)

Dispatch `attended-prep-2026-09-10` d8 (mandate r4: "Fix the 2 failing
open work before we do the next round of reviews"). The fifteen v005
candidates were released through `resume-development --paths src/app`
(sequence 229) before the composition edit. Landed on main:

- [BPR-0718](../60-done/BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md)
  (`7c636594`): not a bisect; the probe failed its session-fallback check
  because BPR-0708's desktop path marked workbench persistence available
  without opening the store. Composition now opens it once and falls back
  to session defaults; per-window durable layouts are installed only when
  it opened. `application_workbench_composition_contract` passes.
- [BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
  (`d1ba791f`): the one word was the registry refusing a pre-created
  0755 root, since BPR-0695 made `--epoch-fixture-root` the registry root;
  eight contract scripts now narrow their roots (the theme, overview,
  saved-server, review-behaviour, epoch-maintenance and native-notification
  scripts had the same omission). Behind it, BPR-0390's displayed-comparison
  flag read empty after shutdown since BPR-0697/BPR-0705 release the read
  host and its comparison; the automated-run status is captured at the
  shutdown transition. The BPR-0646/0651 rail block was amended for
  BPR-0721's no-rails default. Both digests of
  `application_review_round_trip_composition_contract` repinned.

Re-run for the earlier dispatches: the UIforETW compact row at 1280 passes
(BPR-0722), the rail assertions could not be re-run as written because the
default has no rails and are amended (recorded on BPR-0721), and the theme
customization contract passes for BPR-0720. Also seen:
`application_native_notification_attended_contract` gets past startup now
and refuses at `platform_tray_not_composed` under the cocoa platform; not
attributed, not chased, no card. Gates at d1ba791f: composition contract
(140 s), both boundary scripts, strict tidy on the two touched TUs,
clang-format, `git diff --check`, documentation gate.

The bundle was rebuilt at d1ba791f (executable sha256 `cc2afa7f…`, signed,
tests disabled) and the session `cbd48b91…` carries the same fifteen
candidates at revision `app-release-cc2afa7f-2026-09-11.2`: fifteen ready
and protected at sequence 259, none decided, in
[rfc-bpr-0028-attended-v006](../../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both (`serve`, then `wait --after 259`).

Next: on the owner's say-so, `serve` and a persistent `wait --after 259`;
then consume the verdicts card by card. No independent unattended work
remains in this scope; the bundle inputs (`src/adapters/qt`,
`src/adapters/sqlite`, `src/app`, `src/core`, `src/application`) are
protected by the fifteen candidates until the owner says to resume
development.
