---
schema: "implement-rfc.card.v1"
id: "BPR-0283"
title: "Simplify the device-authorization implementation"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0281","BPR-0282","BPR-0310","BPR-0311","BPR-0318"]
---

# Simplify the device-authorization implementation

## Required final state

- A focused simplifier pass removes duplicated policy, broad service access,
  unnecessary serialization, stale comments, whole-state refreshes, and test
  seams that can bypass production selection.
- Core, host, adapter, Qt, and composition ownership remains explicit; security
  checks, erasure, cancellation, and bounded-state code are not collapsed into
  clever or unauditable helpers.
- The pass occurs after macOS, Linux, and Windows production-selection changes,
  including `windows-app-release`, so it covers the complete implementation.
- Scoped format, strict tidy, focused tests, Release contracts, documentation,
  and `git diff --check` pass after simplification.

## Scouted inputs — 2026-09-02

Every dependency is done since the three platform flips landed
([[BPR-0318]], [[BPR-0310]], [[BPR-0311]]). A read-only scout of the
implementation found these, each with the smallest change and the selector
that proves it stays correct; they are the pass's worklist, not its
conclusion.

1. **Command legality is re-derived in Qt.** The core publishes which of
   begin/cancel is legal and why in `device_authorization_transition_output::Commands`
   (`src/core/src/device_authorization_transition.cpp`), but
   `device_authorization_presentation` carries no such field, so the host's
   `deliver()` drops it and `device_authorization_panel.cpp`'s `live()`
   re-derives "is a flow active" from `Stage`. Thread the command projection
   through the presentation and delete `live()`; proven by
   `qt_device_authorization_scale_(100|125|200)` and `core_device_authorization`.
2. **The device host takes the whole vault port and uses three methods**
   (`store_refresh`, `retrieve_refresh`, `cancel`); `store`, `retrieve`,
   `delete_credential`, `delete_refresh` are reachable but unused, so nothing
   stops the device host from touching the base-token slot. Extract a
   refresh-only port the full vault also satisfies; proven by
   `application_device_authorization_host`, `application_device_authorization_refresh`,
   `application_github_credential_vault`.
3. **The verification authority is two independent literals** —
   `expected_verification_authority` in the adapter's contract parser and
   `default_device_verification_authority` in the host's handoff — for one
   deliberate double check. Keep both checks, one declaration; proven by
   `github_device_authorization_contract` and `application_device_authorization_host`.
4. **The composition comment "no browser capability is composed in this
   slice" predates the flips.** `github_application.cpp` composes
   `device_authorization_browser_handoff{ .Browser = nullptr }` on every
   platform, so "Open browser" always resolves `unavailable` where the flow
   can now start. Confirmed a missing port, not a stale comment: nothing in
   `src/` opens a browser. Carded as [[BPR-0516]]; this pass only rewrites
   the comment to say so.
5. **Confirmed not a defect:** the `RegisteredDeviceCourse` fixture override
   is compile-time gated on `BPR_BUILD_TESTING` end to end and cannot reach
   the tests-disabled Release.

Not confirmed, so not on the list: the per-poll copy of `DeviceCode` is the
port's stated owned-material contract; `deliver()` republishing the whole
presentation is the atomic-publication rule; the ledger's "initial
production selection" table is headed as historical.

Focused verification for the pass: `core_device_authorization`, the three
host selectors above, `github_device_authorization_(contract|refresh_contract)`,
`native_https_device_authorization_(contract|fixture)`,
`qt_device_authorization_scale_(100|125|200)`,
`application_device_authorization_(public_path|unavailable|release_contract|registered_course)`,
the macOS platform course with `--expected-device-selection enabled --fresh`
in a new build root, `verify_rfc0016_test_inventory.py` if any selector name
changes, and the two app-source digest pins.

## What this slice delivered — 2026-09-02

Six removals and the reasoning for what stayed; the full record, including
every non-removal, is the new `## Simplification` section of
`docs/verification/RFC-BPR-0016.md`.

Worklist item 4 needed no edit: [[BPR-0516]] rewrote the browser-handoff
comment in the same commit that made the scouted text stale, and the current
text already describes the composed opener, the fixture split, and the
construction cut. `src/app/src/github_application.cpp` is therefore unchanged,
so neither `bpr_expected_full_application_digest` nor
`bpr_expected_production_app_manifest_digest` in
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`
moved and no repin pass was needed;
`application_review_round_trip_composition_contract` passes against the
committed pins. No selector name changed, so
`verify_rfc0016_test_inventory.py` needed no separate run beyond the
registered `rfc0016_test_inventory` suite, which passed.

One behaviour correction, deliberately: the panel used to offer "Get a code"
in the `cancelling` stage, which the core refuses with
`cancellation_in_progress`. Reading the core's published commands is what
makes the two agree, and that stage is the whole observable difference.

`check_boundaries` was red at the repository head before this slice began, on
a doc comment in `src/app/src/system_browser_opener.hpp` that spells a
forbidden Windows shell API in prose; the gate's pattern cannot tell a
sentence from a call. Reworded, no code change, gate left strict.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The core's command legality reaches Qt and is not re-derived there; `live()` is gone | `ctest --preset dev -R '^(core_device_authorization\|qt_device_authorization_scale_(100\|125\|200))$'` | Passed: 4/4 — `core_device_authorization` 0.06 s, scales 100/125/200 at 5.01 s, 4.91 s, 4.95 s | this commit |
| Negative for that: a panel that enables a button the core forbids fails by name | Replace the begin-button lookup with `setEnabled(true)`, rebuild, rerun the three scale suites | Fired: all three failed on `and a live flow is cancellable rather than restartable`; restored byte-exact, md5 `5f17c6d65850f6f26637c87d5e1cde77` before and after | this commit |
| The device host holds a refresh-only vault port and cannot name the base-token slot | `ctest --preset dev -R '^(application_device_authorization_(host\|refresh)\|application_github_credential_vault)$'` | Passed: 3/3 — host 0.20 s, refresh 0.20 s, vault 0.17 s | this commit |
| Negative for that: a base-slot call from the device host does not compile | Change `Vault->retrieve_refresh(...)` to `Vault->retrieve(...)`, build `better_pull_review_application` | Fired: `error: no member named 'retrieve' in 'bpr::application::github_credential_refresh_vault_port'`; restored byte-exact, md5 `9b1092b0635130bd7cc0039c88005a7a` before and after | this commit |
| One declaration of the verification authority, both checks kept | `ctest --preset dev -R '^(github_device_authorization_(contract\|refresh_contract)\|application_device_authorization_host)$'` | Passed: 3/3 — contract 0.17 s, refresh contract 5.24 s, host 0.20 s | this commit |
| The whole focused matrix, including the composed hosts, the inventory, and the pinned composition contract | `ctest --preset dev -R '^(core_device_authorization\|application_device_authorization_host\|application_device_authorization_refresh\|application_github_credential_vault\|github_device_authorization_(contract\|refresh_contract)\|native_https_device_authorization_(contract\|fixture)\|qt_device_authorization_scale_(100\|125\|200)\|application_device_authorization_(public_path\|unavailable\|release_contract\|registered_course\|browser_handoff\|browser_absent)\|composed_application_hosts\|rfc0016_test_inventory\|application_review_round_trip_composition_contract)$' --output-on-failure` | Passed: 20/20 in 89.4 s; slowest `application_review_round_trip_composition_contract` 33.52 s against the unchanged pins | this commit |
| Architecture and core boundaries | `cmake --build build/dev --target check_boundaries` | Passed after the comment repair; red at the pre-slice head | this commit |
| macOS production selection still enabled, fresh Debug and Release, on the simplified head | `python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-simplify --expected-device-selection enabled --fresh` | Passed on a fresh root: `RFC0016_PLATFORM_COURSE_OK`, `RFC0016_SELECTION:enabled`, `RFC0016_COURSE_SELECTORS:17`, `RFC0016_DEVICE_STAGE:cancelled`, `RFC0016_DEVICE_REQUESTS:0`, 148 s | this commit |
| Format, documentation, and diff hygiene | `clang-format --dry-run --Werror` per touched file; `python3 scripts/verify_documentation.py --root .`; `git diff --check` | Passed: exit 0 on all twelve touched C++ files, `DOCUMENTATION_OK`, and a clean `git diff --check` | this commit |

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0), ran `cmake --build build/dev --target
check_boundaries` (exit 0), and the twenty RFC-0016 selectors with
`application_composition_contract` (116.42 s), `application_mcp_context_helper`,
and `rfc0016_platform_course_launch_contract`: 23/23. `clang-format` clean by
exit code on the twelve touched C++ files. The one behaviour correction
(the panel no longer offers "Get a code" while cancelling, which the core
refused) is accepted as alignment with the core. The comment repair in
`system_browser_opener.hpp` is folded here and noted on [[BPR-0516]]: that
slice landed with `check_boundaries` red on a doc comment, which the
integrator's reproduction had not run.
