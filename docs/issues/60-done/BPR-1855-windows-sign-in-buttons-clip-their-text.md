---
schema: "implement-rfc.card.v1"
id: "BPR-1855"
title: "Windows sign-in buttons clip their text"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-04: \"Windows startup screen is clipping text\" (phone photo of the signed-out Home card: \"Ise GitHub Cl\", \"ıdd enterprise serve\", and the sentence past the right edge); later photos: Profiles \"lew profil\" and \"ɛnam\", review page \"how file\"","related_cards":["BPR-0646","BPR-0775","BPR-1577","BPR-1659","BPR-1660"],"related_rfcs":["RFC-BPR-0087"],"id_allocation":"Reserved on main (docs/issues/_reservations/BPR-1855--BPR-1855--21fff515b084.json).","frontend_design":"frontend-design loaded. No new look: the accepted chip keeps its padding, surface and type; only who owns the content geometry changes."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d9fb4ec86","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_home_card_fit_scale_(100|125|150|200)$'","detail":"macOS offscreen, Qt 6.12.0, Fusion behind a probe keeping windows11's six-pixel push label margin (and its matching CT_PushButton widening), Verdana (Segoe UI is not installed). Sign in, Use GitHub CLI, Add enterprise server and a long quiet push and tool chip: sizeHint >= words + 12, laid out at that width, words drawn unelided in a rectangle at least their width and inside the button, wide (960) and at the card's minimum width, at each scale. With the previous quiet_push_button restored the 100% and 150% runs fail: \"Use GitHub CLI: the words' rectangle is as wide as the words\"."}},{"criterion":"AC2","outcome":"pass","revision":"d9fb4ec86","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_home_card_fit_scale_(100|125|150|200)$'","detail":"Body is word-wrapped, ends inside the card, has its heightForWidth; at the narrowest it is more than one and a half lines tall at every scale."}},{"criterion":"AC3","outcome":"pass","revision":"d9fb4ec86","evidence":{"kind":"artifact","command":"ctest --preset dev -R '^qt_home_card_fit_scale_(100|125|150|200)$'","detail":"build/dev/tests/adapters_qt/home-card-fit/home-card-{100,125,150,200}-{wide,narrow}.png (build tree, not tracked): all three buttons show their words whole; the narrow card wraps the sentence onto three lines."}},{"criterion":"AC4","outcome":"pass","revision":"d9fb4ec86","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_|test_network_isolation_manifest$|architecture_boundaries$)' -j6; _coding-standard/scripts/format.py; _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <4 changed translation units>; python3 scripts/verify_documentation.py --root .","detail":"216 of 217 pass, including qt_quiet_buttons and the sweep (34 buttons per scale across the window, three Settings pages, Profiles and the idle review direction; New profile, Rename and Show files checked by name). architecture_boundaries fails on base 8a1f897e4 as well: QToolTip::showText in workbench_window.cpp:5014 from BPR-1850 (8d448697b), a file this card does not touch. clang-format 22 and clang-tidy 22 clean. The documentation gate reports only NEXT_ID BPR-1856 rewinding the shared allocator high water BPR-1859, which main reserved after this branch's base (BPR-1856 to 1858); the card itself validates."}},{"criterion":"AC5","outcome":"pass","revision":"edc0e4c1c","evidence":{"kind":"command","command":"On bpr-main-win, in a vcvars64 shell with Git for Windows' usr\\bin appended to PATH: cmake --preset windows-dev; cmake --build --preset windows-dev --target test_better_pull_review_qt_home_card_fit_tests tool_better_pull_review_network_sandbox; ctest --preset windows-dev -C Debug -R \"^qt_home_card_fit_scale_(100|125|150|200)$\" -V","detail":"All four pass, each ending home-card-fit-tests:pass swept=34 qt=6.12.0 platform=offscreen style=windows11 font=Segoe UI scale=<100|125|150|200>; no fusion fallback and no FAILED line. The station's renders (build/windows-dev/tests/adapters_qt/home-card-fit, not tracked) show Use GitHub CLI and Add enterprise server whole inside their borders with padding at every scale; the narrow description wraps to three lines (after \"once;\", \"link to\" and \"start a review.\") and the wide one stays on one line."}}]
---

# BPR-1855: Windows sign-in buttons clip their text

## Goal

On Windows the signed-out Home card showed "Use GitHub CLI" as "se GitHub
CL" and "Add enterprise server" as "dd enterprise serve": the first and last
letters were cut off inside the button's border. Later photos showed the same
on Profiles' New profile and Rename and on Show files under the review
page's "No file open": every bordered secondary chip in the app. Every bordered secondary
button must be as wide as its words in the font and at the scale it is
shown, and draw them whole. The card's sentence must wrap inside the card.

## Cause

Use GitHub CLI and Add enterprise server are `quiet_push_button`s
(RFC-BPR-0087's shared chip). The chip measured itself as its words plus six
pixels a side, then handed the inner rectangle to the style's
`CE_PushButtonLabel`. Qt's `windows11` style, the default on Windows 11,
takes a further `contentHMargin` of six pixels from each side of that
rectangle before drawing a centred label (`qwindows11style.cpp`, Qt 6.11).
The words were left twelve pixels narrower than themselves, so a centred
draw lost about one Segoe UI letter at each end. macOS and Fusion keep no
such margin, which is why it never showed there. `quiet_tool_button` had
the same split: its size came from integer metrics and its label from the
style, which elides when the two disagree.

The sentence was already a word-wrapping label in the card's layout; it is
covered by the same test so a regression would be seen.

## Design

- `button_surface` owns the chip's content geometry. `chip_content_size`
  measures the words with `QFontMetricsF`, rounded up, plus the icon and its
  gap when shown. `paint_button_label` centres icon and words inside the
  chip's six-pixel padding and draws the words through the style's
  `drawItemText`, so the platform still decides mnemonic underlining and
  text rendering, but no style label element can inset or elide them.
- `quiet_push_button` and `quiet_tool_button` size from
  `chip_content_size` and paint through `paint_button_label`. No widths are
  fixed: a different font, display scale or translation changes the measured
  size.
- Native fallbacks (default, menu, icon-only) are unchanged; the primary
  Sign in is a native `QPushButton` whose style both sizes and draws it.

## Acceptance

- **AC1**: Under a style that keeps Windows 11's six-pixel label margin and a
  wide typeface (Segoe UI where installed, else Verdana), each of Sign in, Use
  GitHub CLI and Add enterprise server, and a long-text `quiet_push_button`
  and `quiet_tool_button`, has a `sizeHint` at least its words plus padding,
  is laid out at that width, and draws its words unelided in a rectangle at
  least as wide as them and inside the button, at 100, 125, 150 and 200%
  scale, wide and at the card's narrowest.
- **AC2**: The card's sentence wraps, ends inside the card and has the height
  its lines need at each scale; at the narrowest it takes several lines.
- **AC3**: Offscreen renders of the card at the four scales show the words
  whole.
- **AC4**: Every visible button with words in the composed window, its
  Settings pages, Profiles and the review page's idle direction draws them
  whole at each scale; existing Qt tests still pass; format, tidy and
  documentation gates pass.
- **AC5**: The same test passes on Windows with Qt's own `windows11` style
  and Segoe UI at the four scales, and its renders show the words whole.

## Windows run

bpr-main-win ran AC5 at `edc0e4c1c` and all four scales passed under
`style=windows11 font=Segoe UI`. The commands first handed over lacked four
things the station had to add, now recorded in
[remote stations](../../development/remote-stations.md#one-test-target-on-windows-dev):
`vcvars64.bat` loaded first; Git for Windows' `usr\bin` appended to `PATH`
for the KSyntaxHighlighting configure's `perl`; the
`tool_better_pull_review_network_sandbox` target built with the test; and
`-C Debug` on `ctest`, since the tree is multi-config.

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev --target test_better_pull_review_qt_home_card_fit_tests tool_better_pull_review_network_sandbox
ctest --preset windows-dev -C Debug -R "^qt_home_card_fit_scale_(100|125|150|200)$" -V
```

Each run ends `home-card-fit-tests:pass swept=<n> ... style=windows11
font=Segoe UI scale=<s>` and writes `home-card-<s>-wide.png` and
`home-card-<s>-narrow.png` to `build/windows-dev/tests/adapters_qt/home-card-fit`.
`style=fusion+label-inset-6` there would mean the windows11 style was not
loaded, and the run is not Windows evidence.

## Not covered

- The Windows run is offscreen with Qt's windows11 style; the owner has not
  looked at a visible window on their machine.
- `architecture_boundaries` fails on the base for BPR-1850's tooltip in
  `workbench_window.cpp`; left for its owner.
- `epoch_comparison_control`'s Comparing button sets a fixed width, but from
  the style's own `sizeFromContents` with an explicit elide; it is not this
  pattern and was left.
