---
schema: "implement-rfc.card.v1"
id: "BPR-1864"
title: "The window title names the open pull request"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","found_by":"Owner, 2026-11-04: \"Window title in the dock on Mac id like to see the open PR if there is one rather than the app name and the profile\"","related_cards":["BPR-0722","BPR-1129","BPR-1842","BPR-1863"],"id_allocation":"Reserved on main (docs/issues/_reservations/BPR-1864--BPR-1864--e96321f66234.json).","surface":"The workbench window's title (QWidget::windowTitle and its accessible name): what the macOS Dock's window list, Window menu and Mission Control, and the Windows and Linux taskbars name the window by.","design_skill":"frontend-design:frontend-design loaded for the wording and ordering choice.","owner_review":"One Accept/Reject question on the format, with example titles."}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"this slice's commit (child of 41d9ead29)","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R core_window_title --output-on-failure","detail":"Passed. Nothing open, Home, signed-out mode, a numberless pull request, number-first ordering, the profile only with more than one profile, shortening at a word at 48 characters with the repository kept, a word ending exactly at the cut kept whole, one long word cut within it, a too-distant space not cut at, controls collapsed to spaces, multi-byte characters counted and cut whole, determinism."}},{"criterion":"AC2","outcome":"pass","revision":"this slice's commit (child of 41d9ead29)","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R \"qt_workbench_title_row|qt_workbench_window_scale_100|qt_anonymous_public_preview_scale_100\" --output-on-failure","detail":"All pass on macOS (offscreen): qt_workbench_title_row_scale_{100,125,200} run test_the_window_title_names_the_open_pull_request (loading, accepted, retitle, a second profile, Home round trip, local cycle, [*], close); qt_workbench_window_scale_100 and qt_anonymous_public_preview_scale_100 still pass."}},{"criterion":"AC3","outcome":"pending","revision":"this slice's commit (child of 41d9ead29)","evidence":{"kind":"observation","detail":"Owner Accept/Reject on the format, through a fresh attended session with a frozen build; see the checkpoint."}},{"criterion":"AC4","outcome":"pass","revision":"this slice's commit (child of 41d9ead29)","evidence":{"kind":"command","command":"python3 _coding-standard/scripts/format.py --root . <changed C++ files>; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22 applied through the project script; documentation gate DOCUMENTATION_OK."}}]
---

# BPR-1864: The window title names the open pull request

## Goal

The owner, 2026-11-04: "Window title in the dock on Mac id like to see the
open PR if there is one rather than the app name and the profile".

macOS hides the workbench's title bar, but the window's title is what the
Dock's window list, the Window menu and Mission Control name it by, as do the
Windows and Linux taskbars. It said "BetterPullReview — Work profile" for
every window, so several review windows were indistinguishable there.

## Design

The core words the title (`core::window_title`, `bpr/core/window_title.hpp`)
from explicit state; the window gathers that state and sets the title and
its accessible name.

- **A pull request in front**: `#188875 Add the Flow Control panel —
  MikeMarcin/BPR-Test`. The number leads: it is short and is what tells two
  windows apart when the Dock menu is narrow. The title follows, then the
  repository.
- **Truncation**: the pull request's title keeps at most 48 characters
  (code points), shortened at the last space within 16 characters of the
  cut, ending in "…"; a single long word is cut within it. Control characters
  read as spaces and runs collapse. The repository is never what is cut.
- **Profile**: added last ("— Work profile") only when more than one profile
  exists, where it is what tells two windows on the same pull request apart.
  With one profile it says nothing.
- **Home, a local cycle in front, nothing open**: the old title,
  "BetterPullReview — Work profile", with "Reading without signing in"
  before the profile in a signed-out public preview, as before.
- **Loading**: once GitHub's first answer names the pull request
  ([[BPR-1842]]), the title names it as the title row does.
- **Tabs**: a window shows one pull request. Its document tabs are files of
  it, so switching them never changes the title; "the active tab" is the
  review or Home in front of it.
- **Updates**: the title is recomputed wherever the title row's name is
  (`update_strip_identity`), at the end of `update_home_landing`, on a
  profile label or public-preview publication, and when the profile
  projection arrives (count). So it follows a Home round trip, a local cycle,
  a retitle arriving with a refresh's republication, a close and a profile
  added or removed. BPR has no title editor; a description edit changes no
  title.
- **Qt**: "[*]" in a title is Qt's modified mark, so the window doubles it to
  show it literally.
- **Proxy icon / represented URL**: none. A pull request is not a local file,
  and the title bar that would carry the proxy icon is hidden.

Kept to the title: [[BPR-1863]] is changing File › New Window and the Profiles
menu nearby. The one line in `adopt_profile_management` is placed before the
code it changes.

## Acceptance

- **AC1**: Core title cases (`core_window_title`).
- **AC2**: The Qt course `test_the_window_title_names_the_open_pull_request`
  (`qt_workbench_title_row_*`): open (loading and accepted), retitle, a
  second profile, Home round trip, local cycle, "[*]", close.
- **AC3**: The owner accepts the format (one Accept/Reject question with
  example titles, frozen build).
- **AC4**: Format and documentation gates pass.

## Checkpoint, 2026-11-04

The first run of this slice met a full volume (ENOSPC); space was freed by
deleting this worktree's own `build/dev`, and the slice was then built and
tested in full. The Qt course first failed on the local cycle (the title
tested the stack's current page, which an offscreen window need not have);
the title now follows `LocalCycleShown && !showing_home()` as the strip does.
The core also gained a fix found against a real fixture title
(directxmath-336): a cut falling exactly at a space keeps the last word.

Owed: the owner's Accept/Reject (AC3) on a frozen build of this commit.

Prepared for AC3: frozen root `/Users/jaeger/bpr1864-window-title-v001.or3fxcyi`,
made by `scripts/freeze_attended_build.py freeze --open-pull-request --fixture
nested-folders-v1 --commit 278f012fa`; it opens bpr-fixtures/nested-folders#21
on its Overview. `better_pull_review` SHA-256
`63acb2965e4d8d929def93383d111c042a843e30fdb66f6d21d247a8395df11d`; the
launcher's own check reached `ATTENDED_PULL_REQUEST_OPENED`. Launch:
`python3 /Users/jaeger/bpr1864-window-title-v001.or3fxcyi/launch.py`.
New short session `769800ec26e5b004275ce1a15fe44832`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261104-bpr1864-window-title-v001/state`,
repo root the main checkout, candidate `BPR-1864-window-title-v001`
(fingerprint `58dcb816e343…`), consumed cursor 2, loopback host started.
The offline page is `review.html` in the state directory.
