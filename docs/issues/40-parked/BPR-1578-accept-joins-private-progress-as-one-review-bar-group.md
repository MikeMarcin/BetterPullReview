---
schema: "implement-rfc.card.v1"
id: "BPR-1578"
title: "Accept joins private progress as one review-bar group and reads as a button"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner feedback 2026-10-17","owner_note":"Accept is a button next to other similar text labels in Changes and Files which are not buttons, it's kind of confusing. It also doesn't have the under bar that connects it to the dropdown control like Changes and Files do to their controls.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052","RFC-BPR-0007"],"related_cards":["BPR-1507","BPR-1515","BPR-1519","BPR-1531","BPR-1331"],"reservation":"docs/issues/_reservations/BPR-1578--BPR-1578--5ed4b95b967f.json"}
parked_on: "human-review"
---

# BPR-1578: Accept joins private progress as one review-bar group and reads as a button

The owner, on 2026-10-17, about the review bar (the second row: "File 5 of
17 « »", "Changes 2 ‹ ›", "Accept", then private progress's state menu):

> Accept is a button next to other similar text labels in Changes and Files
> which are not buttons, it's kind of confusing. It also doesn't have the
> under bar that connects it to the dropdown control like Changes and Files
> do to their controls.

File and Changes are plain labels, each over an accent rail that joins it to
its arrow pair ([[BPR-1519]]). Accept ([[BPR-1507]], Option-A since
[[BPR-1531]]) is a command, but it is drawn as a word in the same type beside
those labels, and it has no rail joining it to the private-progress menu it
belongs with.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
review bar as [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
laid it out, recorded in the [workbench specification](../../ux/workbench-specification.md).
The receipt semantics of [RFC-BPR-0007](../../rfcs/RFC-BPR-0007-local-review-ledger-actionability.md)
are unchanged.

## Acceptance

- AC1: The file-progress controls read as one group, as File and Changes do:
  one accent rail, drawn as theirs is, runs under Accept and the private
  progress menu from Accept's left edge to the menu's right edge, and stops
  between this group and the Changes group. It holds at the design width, at
  the width floor and at 1x, 1.25x and 2x. A check with the old two rails
  fails.
- AC2: Accept reads as a button and the File and Changes captions stay plain
  labels: Accept carries an icon beside its word inside the strip's button
  frame, the only text button in that stretch of the bar; the captions keep
  no frame. In the accepted state it reads **Revoke acceptance** in the
  checked frame; while it is disabled (no comparison, ledger unavailable,
  excluded file) the frame and icon take the disabled ink. Light and Dark.
- AC3: Behaviour is unchanged: the one retained Accept action, one click
  without a menu, Option-A (Alt+A) with its tooltip, the accessible name and
  description, the focus order, and the bar's width floor and yields.
- AC4 (human): The owner accepts the frozen candidate's review bar, before
  and after, in Light and Dark, unaccepted, accepted (Revoke) and disabled.

## Coordination

Stay within the review bar's Accept and private-progress group. Other cards
are restyling the Overview toggle (BPR-1577), adding refresh and loading
states (BPR-1575, BPR-1576) and redesigning the drawers, Agent and Compose
(BPR-1574). Load `frontend-design:frontend-design` before UX work. Never drive
or capture the owner's desktop; the owner looks at the frozen build.

## Checkpoint: candidate v001, 2026-10-17

Product commit `70373c686` on branch `bpr-1578-review-bar-accept`
(worktree `/Users/jaeger/.codex/worktrees/bpr1578-review-bar-accept/BetterPullReview`),
candidate head `06a7d1f5d` with main merged in (docs only). Not merged to
main until the owner accepts. Paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1578/handoff.json).

Design (loaded `frontend-design:frontend-design` first). No new colour, type
or QSS: the palette's Highlight for the rails, the strip's button frame and
ink. The one deliberate move is the glyph. Accept carries the strip's line
tick in the button ink, as the bar's other commands carry line glyphs. It
does not carry the green accepted circle its menu item shows: beside the
state menu that circle would read as "already accepted". The circles are
states and the line glyphs are commands, so the File and Changes captions,
which have neither glyph nor frame, read as words and Accept reads as a
button. The strip's rails became a list of groups. A third rail runs from
Accept's left edge to the progress menu's right edge, and an 8-pixel bare gap
before Accept keeps it apart from the Changes rail; without the gap the two
rails nearly touched and read as one. Revoke acceptance keeps the tick in the
checked, accent-tinted frame; disabled paints tick and word in the disabled
ink. The retained Accept action, Option-A, eligibility, accessible text and
focus order are unchanged. The alternative considered, a state caption
("Seen") with Accept as the group's button, adds a third label and
duplicates the menu's mark; it is offered to the owner as a Reject note.

| Acceptance | Evidence at `70373c686` | Result |
|---|---|---|
| AC1 | `qt_workbench_title_row_scale_{100,125,200}`: three group rails; the progress rail spans Accept's left edge to the menu's right edge in rendered pixels and is at least 12 px from the Changes rail. Controls: the old two rails fail the progress rail; hiding the gap fails the separation. `qt_workbench_window_scale_{100,125,200}`: the rail holds at the width floor | Pass |
| AC2 | `qt_review_state_icon` (`test_accept_reads_as_a_button_beside_plain_captions`): Accept's glyph is the command tick in button ink, not the state mark; File and Changes are frameless, glyphless, unfocusable labels; Revoke has the tick in the accent-tinted checked frame; disabled paints the disabled ink; a palette change repaints the tick. Control: Accept without its glyph renders differently and narrower. Light and Dark renders inspected | Pass |
| AC3 | Same test: Space on the focused Accept dispatches the exact acceptance. `qt_review_state_icon`, `qt_option_keys_typing`, `qt_workbench_focus_order_scale_{100,125,200}`, floor and yield tests, `application_uiforetw_selected_diff_contract` (exact 17-item bar order) and `application_review_ledger_composition_contract` pass; the ledger contract failed once while the disk was full and passed on immediate rerun | Pass |
| AC4 human | Session `0466d1f8235d8b124c055885370d2ea1`, candidate `BPR-1578-accept-group-v001`, fingerprint `d05d4488…`, ready and protected at cursor 4; frozen root `bpr1578-accept-group-v001.yp__s3p_` (`--check` CHECK_OK) | Pending |

Gates: clang-format 22.1.8 dry run over the six C++ files and clang-tidy
`--warnings-as-errors` over the four translation units pass;
`DOCUMENTATION_OK` and 67 documentation tests pass. The full dev `ctest` has
not run: the shared disk filled during the full build, so the dev tree was
removed for the freeze. It runs once on the merged tree at integration.

## Owner decision on v001, 2026-10-17

**Reject**, 2026-10-17T15:26:11Z: "I think we can do better, feel free to try
some redesigns of this element." (decision
`b607aea4-cbe4-4969-8b09-f1b7a6544f95`, superseding the earlier note "I think
we can do better." `c6744efd…`), session
`0466d1f8235d8b124c055885370d2ea1`, candidate `BPR-1578-accept-group-v001`,
fingerprint `d05d4488…`; handed back at event 7. Event cursor consumed: 7.
AC1–AC3 evidence stands for v001's code but the design is not accepted.

Next (v002): two or three genuinely different directions for the third group,
rendered in the real bar in Light and Dark (unaccepted, accepted/Revoke,
disabled), offered as one choice in a fresh short session; the chosen one is
then built as v002 for a final Accept or Reject. Frames are faint in Dark, so
each direction must read as a control there without relying on them.

## v002 directions, 2026-10-17

Loaded `frontend-design:frontend-design` again. Three directions for the
third group, each one group on one rail, painted as prototypes into the real
bar by the render-only `--review-bar-directions` mode (`bb4f84cfb`), in Light
and Dark with a seen, an accepted, an excluded and an absent file:

- **A. Caption and action**: `◐ Seen  [✓ Accept] [▾]`. The state is the
  group's caption, as "File 3 of 40" is File's; accepted reads
  `● Accepted  [Revoke] [▾]`.
- **B. Split button**: `[✓ Accept | ◐ ▾]`, Accept the accent-filled half until
  the file is accepted, then `[Revoke acceptance | ● ▾]` unfilled. The only
  one that does not depend on Dark's faint frames.
- **C. Progress track**: `[◐ Seen | ○ Read | ✓ Accept] [▾]`, reached steps
  marked, the current one tinted, Accept the last step; the widest.

Offered as one choice in the fresh session
`9aa0f416eba08632f0d5ca48bc4d5ff0`, candidate
`BPR-1578-progress-group-direction-v002`, fingerprint `c62c1f68…`, ready
at cursor 3. v001's server was stopped, its protection released (event 8)
and its frozen root removed. The chosen direction is built next as v002 for
a final Accept or Reject.

## Owner decision on round 1, and round 2, 2026-10-17

**Reject** of the three round-1 directions, 2026-10-17T18:02:17Z: "I like
progress the best but I'm not in love with any of them. Let's try one more
round, present progress again and more fresh alternatives." (decision
`3b3fdc7d-185c-454a-b3f4-2a4a60588f3f`, session
`9aa0f416eba08632f0d5ca48bc4d5ff0`, handed back at event 5). Event cursor
consumed: 5. That session's server was stopped and its candidate released.

Round 2 (`frontend-design:frontend-design` loaded; render-only
`--review-bar-directions-round-2`, `342838cf2`): the progress track refined
and three new directions. Round 1 showed that soft tints and one-pixel
frames vanish in Dark, so each current state is a solid fill. The step still
to take is the acceptance green's tick beside a word in full ink (the
palette's highlight is too faint as text in both themes):

- **C2. Progress track, refined**: steps still to come in full ink, the
  current step solid accent, Accepted solid green, the chevron folded into
  the track's end, narrower.
- **D. Stepper**: Seen and Read dots joined by lines, ending in the Accept
  pill, which becomes a solid green "Accepted".
- **E. Meter chip**: one rounded chip with a ring that fills by thirds, the
  state's word and Accept as its action.
- **F. Accept only**: the button alone, filled, solid green when accepted;
  Seen and Read automatic, Exclude and Mark unread moved to menus.

Offered as one choice in the fresh session
`bf324baca66b72b02d331b235f5d586a`, candidate
`BPR-1578-progress-group-direction-v002-r2`, fingerprint `7eeeda67…`, ready
at cursor 3.

## Owner choice in round 2, and candidate v002, 2026-10-17

**Select: "C2. Progress track, refined"**, no note, 2026-10-17T19:06:14Z
(decision `ffeeb765-300f-4d8e-a325-e325e29de873`, session
`bf324baca66b72b02d331b235f5d586a`, handed back at event 5). Event cursor
consumed: 5. That session's server was stopped and its candidate released.

v002 on branch `bpr-1578-review-bar-accept`: product `e525d18c3`, main
merged at `06f7d7a9f`, tidy fixes in tests at `d82023470` (differs from the
frozen commit only in `tests/`). Seen, Read and Accept are steps of one
`progress_track` with private progress's chevron as its last cell, on one rail
behind the gap. The track lays out and paints the window's own buttons (two
new step buttons, the retained Accept button and the menu button); it decides
nothing. Click rules, the least surprising: Seen marks an unread file seen,
Read marks it read, a step already reached does nothing (Mark unread stays in
the menu), Accept toggles to Revoke with Option-A as before. The track's last
width yield shows Seen and Read as marks alone, which keeps the 900-pixel
floor. v001's `strip_command_button` is removed.

| Acceptance | Evidence on the merged tree (`06f7d7a9f`, tests at `d82023470`) | Result |
|---|---|---|
| AC1 | `qt_workbench_title_row_scale_{100,125,200}`: one rail under the track, at least 12 px from the Changes rail; controls (old rails, no gap) fail; `qt_workbench_window_scale_*`: the rail and the compact track at the width floor | Pass |
| AC2 | `qt_review_state_icon` (`test_progress_track_shows_and_takes_each_step`): unread has no solid step, seen and read are solid accent, accepted is solid green "Accepted", excluded and no file rest the track; names, tooltips, Space on a step, palette refresh. Mutation dispatching from a reached step fails the never-un-read check; the menu's Read on a read file marks it unread (the negative the step avoids) | Pass |
| AC3 | Same, plus Option-A, focus order with the two new stops, `qt_option_keys_typing`, floor and yield tests, the exact 16-item bar order in the app composition | Pass |
| AC4 human | Session `3760f12fb3aa7cf76b495c277c7d32d5`, candidate `BPR-1578-progress-track-v002`, fingerprint `b1c5e24a…`, ready and protected at cursor 3; frozen root `bpr1578-progress-track-v002.bebcwn1p` (`--check` CHECK_OK) | Pending |

Gates: clang-format 22.1.8 clean on the six C++ files; clang-tidy
`--warnings-as-errors` passes on the four translation units (three test-only
findings fixed at `d82023470`); `DOCUMENTATION_OK`. The full dev `ctest`
runs on the merged tree at integration after acceptance.

## Closeout, 2026-10-17

**Owner decision: Accept**, no note, 2026-10-17T19:59:55Z (decision
`c1ec85f1-fff7-445f-b867-ef609d2d69e8`, event 5 of session
`3760f12fb3aa7cf76b495c277c7d32d5`), on candidate
`BPR-1578-progress-track-v002` at `d82023470`, fingerprint `b1c5e24a…`.
Event cursor consumed: 6. The server was stopped, the candidate's protection
released and the frozen root removed; every earlier session of this card was
already stopped and released.

Integration. Main merged into the branch at `8d727fab6`. One full dev
`ctest` there (774 tests, 7739 s at load averages near 70): 765 passed and 9
failed, each reconciled:

- `application_uiforetw_selected_diff_contract`: real. The app reported
  private progress's rectangle in the track's coordinates; fixed at
  `820958163` by mapping it into the bar, then passed.
- `application_review_round_trip_composition_contract`: real. Its pinned
  `github_application.cpp` and source-manifest digests follow this change;
  resealed over main at `5a580a955` and again at `a30b1fa4a`, then passed.
- `rfc0016_production_identity_security`: this worktree's own untracked
  `.agents` link, which resolves outside the checkout; without it all 122
  tests pass.
- `live_mutation_isolation`: a 300 s timeout under load; run directly it
  prints `LIVE_MUTATION_ISOLATION_OK` (420 binaries) in 262 s.
- `application_composition_contract`: a 1560 s timeout under load; alone it
  passed in 466.7 s.
- `release_update_native_https`: the known intermittent updater proxy
  fixture; passed alone.
- `application_anonymous_public_preview_unavailable`: load; passed alone.
- `application_large_review_responsiveness` and
  `application_large_review_launch`: timing budgets ("Mac fixture
  responsiveness budget exceeded", `ProcessToHomeMarkerMs` 1678.7 then
  4432.8 against 1000) at load averages near 72, both runs; startup to Home
  and large-review navigation do not involve the track. Not shown passing on
  a quiet machine; BPR-1598 owns load-robust large-review timing rows.

Main merged again at `00287bcea` (BPR-1575 and BPR-1576 refresh feedback;
the window header conflict resolved by keeping both members). On that final
tree: 47 rows covering the window, the review bar, keys, the composed
selected-diff, ledger, navigation, Overview, header, zoom, refresh feedback
and close-review paths, plus the resealed round-trip contract.: all 47 pass
(508 s), and the round-trip contract passes. Docs gate on main after landing.

## Codex landing — 2026-10-17

Main `376a4190b` merged at `49b3c0f57`. Rebuilt the window test and app
targets. All 15 focused rows pass: review-state icon, Option keys while typing,
focus order/title row/window at 100%, 125% and 200%, selected-diff and ledger
composition, `application_composition_contract` and
`application_review_round_trip_composition_contract` (464.41 s total). The
three existing pins remain exact; the contract records this merged-tree check
in a dated note. clang-format 22 checks all six touched C++ files,
`DOCUMENTATION_OK` and `git diff --check` pass. Reused the unchanged focused
tidy evidence. The owner’s handoff schedules the one full suite and repository
format/tidy at item 9. Loaded frontend-design; accepted v002 is unchanged.

The v002 Accept was confirmed in its saved session through event 7. All four
sessions are stopped and released; both frozen build roots are already absent.

Before finish, main advanced to `1202dff1e` solely to allocate BPR-1601.
Merged that counter/receipt update; product and test inputs are unchanged, so
the passing focused evidence is reused.
