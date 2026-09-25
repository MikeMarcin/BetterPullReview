---
schema: "implement-rfc.card.v1"
id: "BPR-1137"
title: "The home sign-in panel says 'signs this Mac in' on Windows and Linux"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-0653's station capture of 2026-09-25 (docs/verification/BPR-0653-2026-09-25, rest-window.png, Windows 11 at 150%)","reservation":"docs/issues/_reservations/BPR-1136--BPR-1137--8ee928da6b11.json","reservation_note":"Reserved in 444f3e10, whose commit message garbled both titles; this is the correct title. BPR-1136 is bpr-main-win's (the strip's Application menu and UI Automation).","related_cards":["BPR-0775","BPR-0773","BPR-0653"],"related_rfcs":["RFC-BPR-0032"]}
acceptance: [{"criterion":"AC1 Signed out, Home's first-launch card says \"A device code in the browser signs this Mac in once; the session is remembered here. Paste a pull request link to start a review.\" on macOS, and the same sentence with \"this computer\" on Windows and Linux. It takes the noun from the product's one machine-name rule, which the device sign-in row and the server card also use, unchanged.","outcome":"pass","revision":"ba521357","evidence":{"kind":"command","detail":"macOS 26.6.2 arm64, Qt 6.11.2, build/dev Debug at ba521357: src/adapters/qt/src/this_machine_text.hpp holds the rule once (\"this Mac\" under __APPLE__, \"this computer\" otherwise); workbench_landing.cpp takes the sentence's noun from it, and device_authorization_panel.cpp and server_card.cpp dropped their private copies for it. qt_composed_home_sign_in passes with the exact macOS sentence. qt_composed_github_scale_100 prints 'All GitHub Qt presentation tests passed', so the device row's 'Waiting for the browser to authorize this ' and the server card's 'remembered on this ' assertions still hold. Scratch control A (both platform tests flipped to the non-Apple branch on this Mac) passes with 'signs this computer in' word for word and no 'Mac'. The real Windows and Linux runs are recorded on the card as owed."}},{"criterion":"AC2 A Qt test asserts the exact sentence for its build platform and, off macOS, the absence of \"Mac\". It runs in the full composed run and alone as qt_composed_home_sign_in. It fails with the old wording where the wording differs.","outcome":"pass","revision":"ba521357","evidence":{"kind":"command","detail":"test_home_sign_in_sentence_names_this_machine in tests/adapters_qt/composed_github_tests.cpp, run by the full composed run after test_home_signed_out_is_one_paste_field and alone behind --home-sign-in as the contained row qt_composed_home_sign_in (pass line home-sign-in:pass). Pass on macOS, 0.25 s. Scratch control B (the old literal workbench_landing.cpp with the Windows and Linux expectation) fails, printing \"Home's first-launch card says: A device code in the browser signs this Mac in once; ...\" and \"FAILED: signed out, Home's first-launch card names this platform's machine in the accepted sentence\". Scratch control C (helper forced to \"this computer\" with the macOS expectation) fails the same assertion. Controls were not committed; the candidate was restored byte for byte and rebuilt."}},{"criterion":"AC3 On macOS, the Qt Home and sign-in rows pass, the new row passes, and the manifest gate passes. The touched C++ files are formatted.","outcome":"pass","revision":"ba521357","evidence":{"kind":"command","detail":"ctest --test-dir build/dev -R '^(qt_composed_home_sign_in|qt_home_list_scale_(100|125|200)|qt_workbench_home_discovery_scale_(100|125|200)|qt_device_authorization_scale_(100|125|200)|qt_anonymous_public_preview_scale_100|qt_composed_github_scale_100|test_network_isolation_manifest)$': 13 of 13 pass, 71 s. architecture_boundaries and core_dependency_boundary pass. clang-format 23.1.1 --dry-run --Werror clean on the five touched C++ files (their HEAD versions were clean). _coding-standard/scripts/tidy.py: clang-tidy passed on workbench_landing.cpp, device_authorization_panel.cpp, server_card.cpp and composed_github_tests.cpp. python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK."}}]
---

# BPR-1137: The home sign-in panel says 'signs this Mac in' on Windows and Linux

## Why

Signed out, Home's first-launch card says:

> A device code in the browser signs this Mac in once; the session is
> remembered here. Paste a pull request link to start a review.

on every platform. [[BPR-0653]]'s station capture shows it on Windows 11
([rest-window.png](../../verification/BPR-0653-2026-09-25/rest-window.png)).
The sentence is [[BPR-0775]]'s accepted first-launch copy. It came from the
macOS mockup, and `workbench_landing.cpp` spells the noun as a literal.

The product already has a rule for naming the machine. [[BPR-0773]] set it in
the device sign-in row ("Waiting for the browser to authorize this Mac.") and
the server card ("remembered on this Mac"): **"this Mac" on macOS, "this
computer" on Windows and Linux.** The landing card never took the noun from
that rule. Each of those two files has its own private copy of the helper, so
nothing pointed the landing at them.

## Scope

- The card's sentence takes the machine's name from the product's rule:
  "this Mac" on macOS, as accepted; "this computer" on Windows and Linux. The
  rest of the accepted sentence does not change.
- The rule lives in one place inside the Qt adapter. The device sign-in row
  and the server card keep saying exactly what they say now.
- A Qt test asserts the card's exact sentence for the platform the test is
  built for, and that off macOS it does not say "Mac". A focused row runs it
  alone.
- Owned paths: `src/adapters/qt/src/workbench_landing.cpp`, the adapter's
  internal machine-name helper and its two existing users
  (`device_authorization_panel.cpp`, `server_card.cpp`), the composed Qt test,
  and its registration.

## Acceptance

- AC1 Signed out, Home's first-launch card says "A device code in the browser
  signs this Mac in once; the session is remembered here. Paste a pull request
  link to start a review." on macOS, and the same sentence with "this
  computer" on Windows and Linux. It takes the noun from the product's one
  machine-name rule, which the device sign-in row and the server card also
  use, unchanged.
- AC2 A Qt test asserts the exact sentence for its build platform and, off
  macOS, the absence of "Mac". It runs in the full composed run and alone as
  `qt_composed_home_sign_in`. It fails with the old wording where the wording
  differs.
- AC3 On macOS, the Qt Home and sign-in rows pass, the new row passes, and the
  manifest gate passes. The touched C++ files are formatted.

## Done on macOS, 2026-09-25 (main, `ba521357`)

### What changed

- [`this_machine_text.hpp`](../../../src/adapters/qt/src/this_machine_text.hpp)
  holds the rule once, in `bpr::qt::detail`, inside the Qt adapter:
  "this Mac" under `__APPLE__`, "this computer" otherwise. The two private
  copies in `device_authorization_panel.cpp` (which used `QObject::tr`) and
  `server_card.cpp` are gone, and both files call the shared helper. Their
  words are unchanged; no translation catalogue exists, so `tr` and a
  literal print the same.
- `workbench_landing.cpp` builds the card's sentence as
  `"A device code in the browser signs %1 in once; the session is remembered
  here. Paste a pull request link to start a review."` with the helper's
  noun. On macOS the result is [[BPR-0775]]'s sentence, byte for byte.
- `composed_github_tests.cpp` adds
  `test_home_sign_in_sentence_names_this_machine`. It opens a signed-out
  `workbench_window` and asserts that Home's card is in `sign_in` with the
  exact sentence for its build platform. Off macOS it also asserts that the
  sentence does not contain "Mac". On a mismatch it prints what the card
  says. It runs in the full composed run, after
  `test_home_signed_out_is_one_paste_field`, and alone behind
  `--home-sign-in` as the new row `qt_composed_home_sign_in`
  (`QT_SCALE_FACTOR=1.0`, pass line `home-sign-in:pass`, timeout 30 s). The
  row is registered with `bpr_add_network_isolated_test`, so it is contained,
  and the manifest gate passes with it.

### Verification (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug)

| Check | Result |
| --- | --- |
| `qt_composed_home_sign_in` | Pass, 0.25 s. The sentence is the "this Mac" one, exactly |
| `qt_composed_github_scale_100` | Pass. It prints `All GitHub Qt presentation tests passed`, so the full run reaches the new test, the server card's "remembered on this " and the device row's "Waiting for the browser to authorize this " assertions |
| Home rows: `qt_home_list_scale_{100,125,200}`, `qt_workbench_home_discovery_scale_{100,125,200}` | 6 of 6 pass |
| Sign-in rows: `qt_device_authorization_scale_{100,125,200}`, `qt_anonymous_public_preview_scale_100` (server cards) | 4 of 4 pass |
| `test_network_isolation_manifest` | Pass with the new row |
| `architecture_boundaries`, `core_dependency_boundary` | Pass |
| Control A, scratch: both the helper's and the test's platform tests flipped to the Windows and Linux branch on this Mac | `qt_composed_home_sign_in` passes. The card says "signs this computer in", word for word, and no "Mac" |
| Control B, scratch: the old `workbench_landing.cpp` (the literal), with the test expecting the Windows and Linux sentence | Fails: `Home's first-launch card says: A device code in the browser signs this Mac in once; ...` and `FAILED: signed out, Home's first-launch card names this platform's machine in the accepted sentence` |
| Control C, scratch: the helper forced to "this computer", with the test expecting the macOS sentence | Fails the same assertion, printing the "this computer" sentence |
| clang-format 23.1.1 | Clean on the five touched C++ files; their `HEAD` versions were clean |
| `_coding-standard/scripts/tidy.py` | `clang-tidy passed` on `workbench_landing.cpp`, `device_authorization_panel.cpp`, `server_card.cpp` and `composed_github_tests.cpp` |

All three controls were scratch edits, and none was committed. The candidate
was restored byte for byte (diff and file comparisons) and rebuilt. Every
other row of the table, and tidy, ran on the restored candidate. `ba521357` is exactly the
verified tree. The two `BPR-1106` commits that landed beside it touch no
`src` or `tests` file.

### Owed elsewhere: the Windows and Linux confirmation runs

On a real platform, only a Windows or Linux build compiles the "this
computer" branch of the helper and of the test. Control A shows that branch
producing the right sentence on this Mac. It does not replace running it
where it ships. Each station confirms it in its next ordinary pass, at or
after `ba521357`:

```sh
ctest --test-dir <build> --output-on-failure -R "^(qt_composed_home_sign_in|qt_device_authorization_scale_100|qt_anonymous_public_preview_scale_100)$"
```

- **Windows (bpr-main-win): owed.** Expect `home-sign-in:pass`. The full
  `qt_composed_github_scale_*` run is not a substitute there, because it is
  red earlier for reasons that are not this card's ([[BPR-0653]]'s criterion
  3 record). That is why the focused row exists.
- **Linux (bpr-main-linux): owed.** Expect `home-sign-in:pass`.

A failure on either station is this card's defect. Record it here, or on a
follow-up card if this one is closed.

### Found alongside, not changed here

`github_cli_consent.cpp`'s GitHub CLI read consent says "You trust **this
Mac's** gh executable, configuration and certificates as they are set up." on
every platform. It is the same defect in a different surface, the consent
dialog behind Use GitHub CLI. It needs its own card. It was left alone
because this card's scope is the Home card, and the consent text belongs to
the GitHub CLI way in ([[BPR-0803]]). Reported to the coordinator uncarded.
