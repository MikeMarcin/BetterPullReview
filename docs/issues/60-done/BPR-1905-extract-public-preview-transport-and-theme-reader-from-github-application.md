---
schema: "implement-rfc.card.v1"
id: "BPR-1905"
title: "Extract the public-preview transport and theme reader from github_application.cpp"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"f430ebd907772b95576230d0858ae600e2f220c4","mandate":"Owner requested on 2026-11-06 a direct, behavior-preserving refactoring of the largest C++ files into smaller cohesive files, starting with github_application.cpp: one bounded extraction round, landed on main, with a card, coherent commits and a one-line refactoring journal entry. Files aim for about 500 physical lines; up to 1,500 when one responsibility clearly reads better together. No new public API, framework or dependency; no formatting or header games."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4a7a21315","evidence":{"kind":"observation","detail":"Counter, log, counting transport, shipped anonymous factory and Git smart lease moved with unchanged bodies to src/app/src/github_application/public_preview_wire_transport.{hpp,cpp} (229/403 lines); helpers stay file-local."}},{"criterion":"AC2","outcome":"pass","revision":"4a7a21315","evidence":{"kind":"command","detail":"public_preview_wire_self_check.cpp (186 lines). application_anonymous_public_preview_live_candidate_wiring passes tests-on; better_pull_review_composition builds tests-off in app-release."}},{"criterion":"AC3","outcome":"pass","revision":"4a7a21315","evidence":{"kind":"command","detail":"theme_document_file.{hpp,cpp} (32/242 lines). application_appearance_theme_customization_contract and application_appearance_older_theme_contract pass."}},{"criterion":"AC4","outcome":"pass","revision":"4a7a21315","evidence":{"kind":"command","detail":"macOS dev: app builds; 12 focused rows pass (architecture_boundaries, architecture_boundary_contract, application_composition_contract, application_review_round_trip_composition_contract, six application_anonymous_public_preview_* rows, two appearance theme contracts); tests-off composition builds; clang-format 22 check and DOCUMENTATION_OK. Reseal covers BPR-1905 and BPR-1901's pre-existing drift; mutation pin unchanged. Windows/Linux not built."}},{"criterion":"AC5","outcome":"pass","revision":"4a7a21315","evidence":{"kind":"observation","detail":"Completed card and one-line docs/development/refactoring-journal.md entry (linked from docs/README.md) accompany the source commit for the main fast-forward."}}]
---

# Extract the public-preview transport and theme reader from github_application.cpp

`src/app/src/github_application.cpp` is 42,564 lines. Its first 3,000 lines
are an anonymous namespace of small composition adapters, followed by the
profile runtime (about 2,800 lines), `github_application::implementation`
(about 19,300 lines, members defined in the class), the startup and public
`github_application` members, and 14,400 lines of tests-only
`github_application_fixture_access` members.

This round moves the adapters whose collaborators are narrow and whose
behavior is already covered, without exposing the profile runtime or the
implementation class to another translation unit.

## Acceptance

- AC1: Extract the signed-out public-preview wire transport (pull-request
  read counter, wire operation log, single-attempt counting transport,
  shipped anonymous transport factory, anonymous Git smart transport
  factory) into its own source and private header.
- AC2: Move the tests-only `public_preview_single_attempt_self_check` body
  out of the composition root into its own source, keeping its behavior in
  both tests-on and tests-off builds.
- AC3: Extract the bounded theme document file reader, including its
  Windows and POSIX file-handle code, into its own source and private header.
- AC4: Preserve behavior: the composition library builds and its focused
  public-path, appearance/theme, public-preview and composition contract
  courses pass; architecture boundaries, clang-format 22 and documentation
  checks pass. The composition contract's source pins and inventories are
  resealed for the moved code only.
- AC5: Land the change, the completed card and a one-line refactoring
  journal entry on main.

## Result

Source `4a7a21315` moves three responsibilities, bodies unchanged, into
`src/app/src/github_application/` (owner, mid-round: subfolders are welcome
where they help organization):

| Responsibility | Was (github_application.cpp) | Now | Lines |
|---|---|---|---|
| Signed-out public-preview wire: read counter, operation log, single-attempt counting transport, shipped anonymous factory, anonymous Git smart lease | 1877–2368, anonymous namespace | `public_preview_wire_transport.hpp` / `.cpp` | 229 / 403 |
| Tests-only single-attempt self-check (false tests-off) | 27982–28150 | `public_preview_wire_self_check.cpp` | 186 |
| Bounded theme document reader with Windows/POSIX file handles | 2776–2997, anonymous namespace | `theme_document_file.hpp` / `.cpp` | 32 / 242 |

`github_application.cpp` goes from 42,564 to 41,674 lines; the new files total
1,092 (+202 lines of declarations and out-of-line signatures). No new file
exceeds 500 lines. The headers declare only the concrete classes the profile
runtime holds by value or constructs; the failure-name/outcome helpers, the
shipped factory's refusal and the file-handle classes stay file-local. Every
moved class still compiles in every build, as before; the composition root
drops only the POSIX, `QSaveFile` and `<cerrno>` includes that the moved
code used (`<Windows.h>` stays for `GetWindowsDirectoryW`).

The five files join the composition library, the attended source surface and
the round-trip contract's inventory and production manifest. That contract
was red on main before this round: [[BPR-1901]]'s `attended_source_identity`
pair was missing from the inventory and its `main.cpp` change had drifted the
manifest pin (last matching `bb3ff53ff`). Its change was reviewed here (a
macro replaced by a same-valued constant from a new translation unit) and the
reseal covers it. The mutation-branch pin is unchanged. The command
correctness registry's `github_application.cpp` input digest was already stale
(`722f…` against `7dfa…`); it gates evidence reuse only and is left as is.

Checks on macOS (arm64), dev Debug unless noted: the app builds with the
warnings policy (only third-party libarchive warnings); the composition library
builds tests-off in `app-release`; clang-format 22 check on the six touched C++
files; `verify_documentation.py` (DOCUMENTATION_OK); the contract's standalone
inventory/digest mode; and 12 ctest rows, all passing: `architecture_boundaries`,
`architecture_boundary_contract`, `application_composition_contract`,
`application_review_round_trip_composition_contract`, the six
`application_anonymous_public_preview_*` rows (the live-candidate dry run runs
the moved self-check), `application_appearance_theme_customization_contract`
and `application_appearance_older_theme_contract`.

Not run: Windows and Linux builds. The Windows file-handle code moved
verbatim with its own `NOMINMAX`/`WIN32_LEAN_AND_MEAN` guard; the POSIX headers
left the composition root after a search found no other use. Neither
`qt_composed_github` nor the heavier composition courses were run.
