---
schema: "implement-rfc.card.v1"
id: "BPR-1720"
title: "Help > About BetterPullReview"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1720--BPR-1720--85e1152e4551.json","kind":"ux","origin":"owner request 2026-10-21 (relayed by the coordinator)","owner_note":"We need an about page. We can take inspiration fom the AllTheThings about page and copy it's GCT logo","owner_decision":"2026-10-21, relayed: AllTheThings and BetterPullReview are both the owner's projects and the GCT logo is the owner's; copy it as this project's own asset, under this project's licence and copyright, with no attribution or third-party notice","related_cards":["BPR-1566","BPR-1567","BPR-1250","BPR-0646"],"related_rfcs":["RFC-BPR-0014","RFC-BPR-0035"],"frontend_design":"frontend-design:frontend-design loaded before the page's layout, type, logo plate and wording"}
acceptance: [{"criterion":"AC1: Help > About shows the GCT logo, name, version, build commit and date, copyright, licences, the notices and corresponding source, and Copy version info","outcome":"pass","revision":"af07c9c6387213a74b6e46cdf3e8796290dc6738","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_about_window_scale_(100|200)$'","detail":"about_window_tests: title and accessible name About BetterPullReview; 'Version 0.14.0'; 'Commit 0123456789 from 2026-10-21' with the whole commit as tooltip, ', with local changes' when there were, and 'No commit is recorded for this build' without one; the logo named 'Game Core Tech logo'; '© 2026 BetterPullReview contributors. Released under the MIT License.' and 'Qt 6.12.0 is used under the GNU LGPL version 3; you may replace it.'; with installed documents, Third-party notices and Corresponding source shown and the absent sentence hidden, without them both hidden and 'Built from source, so no notices or source offer are installed.' shown. Copy version info writes exactly 'BetterPullReview 0.14.0 / Commit: <whole commit> (2026-10-21) / Configuration / Qt / System', one a line, reads Copied, is announced once as 'Version info copied', returns to its label after two seconds and opens nothing. Failing-before control: with the clipboard write removed, 'Copy version info puts exactly the report on the clipboard' fails."}},{"criterion":"AC2: the page performs no effect; links and documents go to the host, and the composed menu reaches the browser with the admitted address","outcome":"pass","revision":"af07c9c6387213a74b6e46cdf3e8796290dc6738","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_markdown_navigation|application_github_window_read_host|application_about_composition|qt_about_window_scale_100|architecture_boundaries|architecture_boundary_contract|application_review_round_trip_composition_contract)$'","detail":"Core: reference_page gains game_core_tech (gamecoretech.com, /) and source_repository (github.com, /MikeMarcin/BetterPullReview), each only with a browser. Page: each link and document button names its page or document to fake host actions once, in order; a failing browser shows 'The browser could not open github.com/MikeMarcin/BetterPullReview. Open that address yourself.', no browser 'There is no browser to open gamecoretech.com with.', a document that does not open names its folder, and the line goes on the next success; with no host actions nothing opens. Menu: one About BetterPullReview with the About role, last in Help under a rule off macOS, however often attached; the page is the window's own child and the same page is raised again; a bar no window owns shows one page of its own. Composition (application_about_composition): this build's version, configuration, running Qt and copyright; the recorded commit is a whole SHA-1 with its date; an empty legal folder installs neither document, two regular files install both and name the folder, and a link to the notices or a folder named like the offer counts as neither; the composed menu's Source code and gamecoretech.com reach the tests-only platform launch as exactly https://github.com/MikeMarcin/BetterPullReview and https://gamecoretech.com/. architecture_boundaries keeps QUrl and QDesktopServices out of the Qt adapter. The round-trip course passes against the built application with the resealed manifest pin."}},{"criterion":"AC3: keyboard, accessibility, light and dark and High Contrast, 1x and 2x with a crisp logo","outcome":"pass","revision":"af07c9c6387213a74b6e46cdf3e8796290dc6738","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_about_window_scale_(100|200)$'","detail":"Close has the keyboard when the page opens and is the default button; Tab goes gamecoretech.com, Source code, Copy version info, Third-party notices, Corresponding source, Close; each takes Tab and has a name; Space opens the source link and the offer and copies; Esc hides the page. In Light, Dark and both High Contrast themes the page takes its window's palette, the version reads at 4.5:1, the build line at 3:1 or more and each link at 4.5:1; the logo pixmap's device pixel ratio is the screen's and its width is 176 x that ratio (one image pixel per device pixel), its corner transparent and its plate dark. An open page follows its window from Light to Dark and redraws the logo's edge, and a 1.5x window font sets the name at 1.75 times it and widens the column. Run at QT_SCALE_FACTOR 1.0 and 2.0. Failing-before controls: without WA_WindowPropagation 'an open page follows its window from Light to Dark' fails; with Copy and Source swapped in the tab order the Tab case fails; without the FontChange handling 'a larger window font sets the name at 1.75 times it' fails."}},{"criterion":"AC4: build identity, docs and gates","outcome":"pass","revision":"af07c9c6387213a74b6e46cdf3e8796290dc6738","evidence":{"kind":"command","command":"cmake --build --preset dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"cmake/BprBuildIdentity.cmake wrote BPR_BUILD_COMMIT c9d535f62bf9340e2468792dc9c69011d8a0828c, BPR_BUILD_COMMIT_DATE 2026-10-21 and BPR_BUILD_LOCAL_CHANGES 1 for the uncommitted tree, and a second build left the header's modification time unchanged. Format check and clang-tidy pass on the changed units. The documentation gate refuses only this card's provisional id format, as it does every provisional card until allocation; with the card renamed to BPR-1720 and the counter at BPR-1721 in the working tree only, restored afterwards, the whole gate printed DOCUMENTATION_OK, and verify_documentation_tests ran 67 tests OK. Results in the Verification section."}}]
---

# BPR-1720: Help > About BetterPullReview

## Owner note

The owner, 2026-10-21, verbatim:

> We need an about page. We can take inspiration fom the AllTheThings about
> page and copy it's GCT logo

Relayed the same day: AllTheThings and BetterPullReview are both the owner's
projects and the Game Core Tech logo is the owner's. It is copied as this
project's own asset, under this project's licence and copyright, without
attribution or a third-party notice.

## Before

The application had no About page. The version reached only
`QCoreApplication::applicationVersion()`, no build recorded the commit it was
made from, and the installed `THIRD-PARTY-NOTICES.txt` and `SOURCE-OFFERS.txt`
were reachable only through the file system. The source was
`/Users/jaeger/Documents/GitHub/AllTheThings` (origin MikeMarcin/AllTheThings,
MIT, copyright Michael Marcin), whose AppKit About window shows
`Resources/GameCoreTechLogo.png` (1254 × 1254 RGB, no vector) at 250 points
over its name, version, "Made by Game Core Tech" and two links.

## Change

- **Asset.** `src/adapters/qt/assets/about/game-core-tech-logo.png`, the
  original downscaled once with Lanczos to 640 × 640 and embedded as the
  `/bpr/about` Qt resource; its README records the original's path and
  SHA-256 and the derivation. No SBOM component or notice.
- **Core.** `reference_page` gains `game_core_tech` and `source_repository`,
  so the page names its links and the existing reference-page host builds,
  checks and opens the address through the system browser opener.
- **Qt.** `about_window` presents `about_facts` and hands links and documents
  to `about_actions`; the clipboard write is its only act. The logo is drawn
  on a rounded dark plate with a hairline edge in the text colour, scaled from
  the master to the exact device size. The page takes its window's palette
  and font through `WA_WindowPropagation` and follows changes to them.
  `about_menu` adds About BetterPullReview, with the About role, last to each
  attached bar's Help menu and presents the owning window's page. The native
  Check for Updates… item moves below Qt's About slot in the macOS application
  menu.
- **Composition.** `about_desktop` reads the facts (compiled version and
  configuration, the recorded commit, the running Qt and system, the installed
  legal folder) and opens documents only while each is a regular file there.
  `profile_desktop` attaches the menu to every window's bar and the empty
  desktop's, after the updater. `cmake/BprBuildIdentity.cmake` records the
  commit, its date and local changes on every build with
  `git --no-optional-locks`, rewriting its header only when they change. The
  round-trip contract's source inventory gains the two files and its manifest
  pin is resealed.
- **Docs.** The workbench specification's Menu bar gains "About
  BetterPullReview"; the packaging README, release packages and the
  maintainers' notices section say where the logo and the page's documents
  come from.

## Acceptance

- **AC1.** Help > About shows the logo, name, version, build commit and
  date, copyright, MIT and Qt LGPL terms, the installed notices and
  corresponding source (or why they are absent), and Copy version info copies
  a bug report's facts. The copy case fails before the change.
- **AC2.** The page performs no effect: links name core reference pages and
  documents go to the composition, which opens only regular files; the
  composed menu's links reach the platform launch with exactly the admitted
  addresses.
- **AC3.** The page is keyboard-usable and named for assistive technology,
  takes Light, Dark and both High Contrast themes from its window and follows
  theme and font changes, and draws the logo at one image pixel per device
  pixel at 1x and 2x. Each control's case fails before the change.
- **AC4.** Build identity is recorded per commit; documentation, format, tidy
  and documentation gates pass.

## Verification

Lean dev build (`-O0 -g0`, the main checkout's CPM cache), macOS arm64,
Qt 6.12.0, offscreen:

- `ctest -R '^(qt_about_window_scale_(100|200)|application_about_composition|core_markdown_navigation|application_github_window_read_host|architecture_boundaries|architecture_boundary_contract|command_correctness_registry_contract|application_review_round_trip_composition_contract)$'`
  passed.
- `rfc0016_test_inventory` and `test_network_isolation_manifest` fail in this
  lean tree only on `gh_cli_git_smart_transport`, whose executable the tree
  did not build; neither names a test this card adds.
- Renders at 2x, Light and Dark, with and without installed documents, and an
  offscreen reconstruction of the AllTheThings window from its own logo and
  layout constants, were handed to the coordinator for the owner.

Not verified here: the order of About BetterPullReview and Check for Updates…
in the native macOS application menu, which needs the cocoa platform on the
owner's desktop; implementers do not drive it.
