---
schema: "implement-rfc.card.v1"
id: "BPR-1140"
title: "The GitHub CLI read consent says \"this Mac's gh executable\" on Windows and Linux"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-1137's 'Found alongside, not changed here' section (ba521357)","reservation":"docs/issues/_reservations/BPR-1138--BPR-1140--96a55e982e06.json","related_cards":["BPR-1137","BPR-0773","BPR-0933","BPR-0803"],"related_rfcs":["RFC-BPR-0015"]}
acceptance: [{"criterion":"AC1 The GitHub CLI read consent's informative text reads the accepted sentence with \"this Mac's\" on macOS and \"this computer's\" on Windows and Linux, the noun from this_machine_text.hpp; nothing else in the question changes","outcome":"pass","revision":"91c005af","evidence":{"kind":"command","detail":"macOS 26.6.2 arm64, Qt 6.11.2, build/dev Debug at fcdb471a plus the change, identical to 91c005af (diff SHA-256 645ac014...): github_cli_consent.cpp builds the text with detail::this_machine_text(); qt_composed_cli_read_consent passes with the macOS sentence exactly, and qt_composed_github_scale_100 prints 'All GitHub Qt presentation tests passed', so test_cli_read_consent_is_a_short_decision still holds for the headline, details and buttons. Scratch control A (helper and test switched to the non-Apple branch on this Mac) passes with 'this computer's' and no word Mac. Windows and Linux confirmation runs are recorded as owed.","path":"docs/issues/60-done/BPR-1140-the-github-cli-read-consent-says-this-macs-gh-executable-on-windows-and-linux.md#done-on-macos-2026-09-25-bpr-main-mac-91c005af"}},{"criterion":"AC2 A Qt test asserts the exact informative text for its build platform and, off macOS, no word \"Mac\" in the headline, informative text or details; it runs in the full composed run and alone as qt_composed_cli_read_consent, and fails with the old literal where the wording differs","outcome":"pass","revision":"91c005af","evidence":{"kind":"command","detail":"test_cli_read_consent_names_this_machine in tests/adapters_qt/composed_github_tests.cpp, run in the full composed run after test_cli_read_consent_is_a_short_decision and alone behind --cli-read-consent as the contained row qt_composed_cli_read_consent (pass line cli-read-consent:pass). Scratch controls: B (HEAD's literal with the non-Apple expectation) fails, printing the 'this Mac's' sentence and 'FAILED: the GitHub CLI read consent names this platform's machine in the accepted sentence'; C (helper forced to 'this computer' with the macOS expectation) fails the same assertion; D (as A with 'this Mac' in the details) fails at 'off macOS the GitHub CLI read consent does not say Mac'.","path":"docs/issues/60-done/BPR-1140-the-github-cli-read-consent-says-this-macs-gh-executable-on-windows-and-linux.md#done-on-macos-2026-09-25-bpr-main-mac-91c005af"}},{"criterion":"AC3 On macOS qt_composed_cli_read_consent, qt_composed_github_scale_100 and test_network_isolation_manifest pass; the touched C++ files are formatted, tidy reports nothing on a changed line, and the documentation gate passes","outcome":"pass","revision":"91c005af","evidence":{"kind":"command","detail":"ctest --preset dev -j1 -R '^(qt_composed_cli_read_consent|qt_composed_github_scale_100|qt_composed_home_sign_in|test_network_isolation_manifest|architecture_boundaries)$' at 06:03:51Z: 5 of 5 pass. clang-format 22.1.8 and 23.1.1 --dry-run --Werror clean on github_cli_consent.cpp and composed_github_tests.cpp; git diff --check clean; tidy.py with clang-tidy 22.1.8 on both: clang-tidy passed; python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK.","path":"docs/issues/60-done/BPR-1140-the-github-cli-read-consent-says-this-macs-gh-executable-on-windows-and-linux.md#done-on-macos-2026-09-25-bpr-main-mac-91c005af"}}]
---

# BPR-1140: The GitHub CLI read consent says "this Mac's gh executable" on Windows and Linux

## Why

The GitHub CLI read opt-in, the question behind Use GitHub CLI
(`make_github_cli_read_consent`, [[BPR-0933]]'s short decision), says this on
every platform:

> BetterPullReview will read as the account gh is signed in to, and that
> account can change outside this app. You trust this Mac's gh executable,
> configuration and certificates as they are set up. Connecting for reads
> does not authorize publishing; each review or comment asks separately.

`github_cli_consent.cpp` spells "this Mac's" as a literal. On Windows and
Linux the reviewer is asked to trust a Mac they are not using. The GitHub CLI
way in is offered there as well: [[BPR-1114]] hardened its Windows launches.

The product names the machine by one rule, which [[BPR-0773]] set: "this Mac"
on macOS, and "this computer" on Windows and Linux. [[BPR-1137]] moved that
rule into one place, `src/adapters/qt/src/this_machine_text.hpp`, and found
this sentence while doing so. It left the sentence alone, because it was
outside that card's scope.

## Scope

- The consent's informative text takes the machine's name from
  `this_machine_text.hpp`. On macOS it stays byte for byte as it is. On
  Windows and Linux it says "this computer's gh executable". The headline,
  the Show Details text, the buttons and their roles do not change.
- A Qt test asserts the exact informative text for the platform the test is
  built for. Off macOS it also asserts that the word "Mac" appears nowhere in
  the question. A focused row runs that test alone, so that each platform
  confirms its own wording without the full composed run.
- Owned paths: `src/adapters/qt/src/github_cli_consent.cpp`,
  `tests/adapters_qt/composed_github_tests.cpp` and the row's registration in
  `tests/adapters_qt/CMakeLists.txt`.

## Acceptance

- AC1 The GitHub CLI read consent's informative text reads "BetterPullReview
  will read as the account gh is signed in to, and that account can change
  outside this app. You trust this Mac's gh executable, configuration and
  certificates as they are set up. Connecting for reads does not authorize
  publishing; each review or comment asks separately." on macOS. On Windows
  and Linux it is the same sentence with "this computer's". The noun comes
  from `this_machine_text.hpp`. Nothing else in the question changes.
- AC2 A Qt test asserts the exact informative text for its build platform.
  Off macOS it also asserts that no word "Mac" appears in the headline, the
  informative text or the details. The test runs in the full composed run,
  and alone as `qt_composed_cli_read_consent`. It fails with the old literal
  where the wording differs.
- AC3 On macOS, `qt_composed_cli_read_consent`, `qt_composed_github_scale_100`
  (which also runs [[BPR-0933]]'s `test_cli_read_consent_is_a_short_decision`)
  and `test_network_isolation_manifest` pass. The touched C++ files are
  formatted, `tidy.py` reports nothing on a changed line, and the
  documentation gate passes.

## Boundaries

This is wording in the Qt adapter. It changes no consent behaviour, no host,
account or trust rule, and nothing in the GitHub adapter. The Windows and
Linux runs of the new row are owed to those stations after this card closes,
as [[BPR-1137]]'s runs are.

## Done on macOS, 2026-09-25 (bpr-main-mac, `91c005af`)

### What changed

- `github_cli_consent.cpp` builds the informative text as
  `"… You trust %1's gh executable, configuration and certificates as they are set up. …"`,
  with the noun from `detail::this_machine_text()`. On macOS the result is
  the old sentence, byte for byte. The headline, the Show Details text and
  the buttons are unchanged.
- `composed_github_tests.cpp` adds `test_cli_read_consent_names_this_machine`.
  It asserts the exact informative text for its build platform, and prints
  what the question says if it differs. Off macOS it also asserts that the
  word `Mac` (`\bMac\b`, so "Machine configuration" in the details does not
  count) is not in the headline, the informative text or the details. It
  runs in the full composed run, after [[BPR-0933]]'s
  `test_cli_read_consent_is_a_short_decision`. It also runs alone behind
  `--cli-read-consent`, as the new contained row
  `qt_composed_cli_read_consent` (`QT_SCALE_FACTOR=1.0`, pass line
  `cli-read-consent:pass`, timeout 30 s).

### Verification (macOS 26.6.2 arm64, Apple clang 21.0.0, Qt 6.11.2, `build/dev` Debug)

`build/dev` was built at `fcdb471a` with the change applied, which is exactly
`91c005af`: the commit's diff has the SHA-256 of the tested patch. The build
printed no `warning:` line.

| Check | Result |
| --- | --- |
| `qt_composed_cli_read_consent` | pass, 0.17 s: `cli-read-consent:pass`, with the "this Mac's" sentence exactly |
| `qt_composed_github_scale_100` | pass. It prints `All GitHub Qt presentation tests passed`, so the full run reaches the new test and `test_cli_read_consent_is_a_short_decision` |
| `qt_composed_home_sign_in` | pass: the shared helper's other user is unchanged |
| `test_network_isolation_manifest`, `architecture_boundaries` | pass with the new row |
| Control A, scratch: the helper and the new test both switched to the Windows and Linux branch on this Mac | pass. The sentence reads "this computer's", and no word `Mac` appears in the question |
| Control B, scratch: `HEAD`'s `github_cli_consent.cpp`, the old literal, with the test expecting the Windows and Linux sentence | fails. It prints "The GitHub CLI read consent says: … You trust this Mac's gh executable …", then `FAILED: the GitHub CLI read consent names this platform's machine in the accepted sentence` |
| Control C, scratch: the helper forced to "this computer", with the macOS expectation | fails the same assertion, printing the "this computer's" sentence |
| Control D, scratch: as A, with "this Mac" put into the details | fails at `off macOS the GitHub CLI read consent does not say Mac` |
| clang-format 22.1.8 and 23.1.1, `--dry-run --Werror`, on the two touched C++ files; `git diff --check` | clean |
| `_coding-standard/scripts/tidy.py` with clang-tidy 22.1.8 on `github_cli_consent.cpp` and `composed_github_tests.cpp` | `clang-tidy passed` |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The controls ran in a scratch `git archive` copy with its own build
directory, never in the shared tree, and the copy has been removed. They ran
on the first candidate. That candidate passed the same `.arg` expression to
`setInformativeText` directly. The committed one puts the translated text in
a local first, because clang-format asked for it. The strings and the test
are the same. The logs are station records in `build/logs/bpr-1140-mac/`,
not committed.

### Owed elsewhere: the Windows and Linux confirmation runs

Only a Windows or Linux build compiles the "this computer" branch of the
helper and of the test. Control A shows that branch on this Mac, but that
does not replace running it on those platforms. Each station confirms it in
its next ordinary pass, at or after `91c005af`:

```sh
ctest --test-dir <build> --output-on-failure -R "^(qt_composed_cli_read_consent|qt_composed_home_sign_in)$"
```

Expect `cli-read-consent:pass` on each. A failure there is this card's
defect. Record it here.

### Seen, not changed

The details still say "They need gh 2.100.0 or 2.101.0 with Apple Git
2.50.1" on every platform. That is accurate: it mirrors the allowlist in
`gh_cli_git_smart_transport.cpp`, which accepts only
`git version 2.50.1 (Apple Git-155)`, with no platform branch. So on every
platform a gh full comparison is refused unless Git reports Apple's build.
That is the transport's rule, not wording, and it is outside this card.
