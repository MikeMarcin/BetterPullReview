---
id: BPR-0315
title: Verify anonymous preview remains unavailable on Windows x64
status: Ready
rfc: RFC-BPR-0017
depends_on: [BPR-0036, BPR-0311, BPR-0306]
---

# Verify anonymous preview remains unavailable on Windows x64

## Required final state

- A real supported Windows 10/11 x64 host builds the tests-disabled Release
  application and selects typed public-preview unavailability for GitHub.com
  and GHES.
- Exact counters prove zero Credential Manager, Authorization header,
  REST/GraphQL, Git smart HTTP, browser, mutation, child-process, and private-
  content effects.
- PE/runtime/dependency, Qt presentation, fixture-seam absence, documentation,
  and hygiene gates pass target-native. No enablement claim is made.
- This card consumes and validates RFC-BPR-0016 BPR-0311's existing native
  `windows-app-release` tests-disabled Release preset; it does not create a
  competing preset or alter that preset's test-seam policy.
- This card's first target-native course is provenance for the unavailable
  selection; BPR-0309 reruns the same idempotent course at immutable final head
  after cross-platform simplification and documentation reconciliation.

## Deferring — 2026-08-23

The Windows session that cleared the rest of this lane checked this card's
dependencies: they sit in `10-backlog/`, so nothing here can start. The
card stays Ready, deferring, and is not part of the lane's completed set.

## Target-native course — 2026-09-02, at `6bfb358d`

The dependencies cleared: `main` at `91873924` carries [[BPR-0305]] and
[[BPR-0306]], merged into this lane as `6bfb358d`. The course ran there.
**The card stays Ready.** The platform course itself is green, but two of
the focused selectors the ledger's matrix row names fail on this host, and
both are recorded below rather than closed over.

### Host and toolchain

```text
Windows 11 Home 10.0.26200 (x64), the JaegAW host
Visual Studio 18 Community, MSVC 14.51.36231, x64 Native Tools (vcvars64)
CMake 4.4.2   Python 3.14.7 (py -3)   Qt 6.10.3 (msvc2022_64)
```

Under the [[BPR-0493]] procedure: Smart App Control off, every command from
the MSVC environment, the build root deleted whole rather than trusted to
`--fresh`.

**The [[BPR-0036]] PATH addendum is still load-bearing.** The first attempt
died in 34 seconds at
`RFC0017_PLATFORM_COURSE_FAILED:configure-failed:windows-dev:missing: PERL_EXECUTABLE`
— the verified-KSyntaxHighlighting nested configure wants `perl`, and the
only one on this host is inside Git for Windows, which is not on the
persisted `PATH`. Appended, as that addendum prescribes, never prepended:
Git's `usr/bin` also ships a `link.exe`, and the symbol reader behind
`check_platform_surfaces` shells `link /MAP`. The run logged both, in the
order that matters:

```text
link: .../MSVC/14.51.36231/bin/Hostx64/x64/link.exe
link: C:/Program Files/Git/usr/bin/link.exe
```

### The course

```text
py -3 scripts/verify_rfc0017_platform_course.py --platform windows
  --dev-preset windows-dev --release-preset windows-app-release
  --build-root build/rfc0017-windows-enablement
  --expected-preview-selection unavailable --fresh
```

Exit 0:

```text
RFC0017_PLATFORM:windows          RFC0017_SELECTION:unavailable
RFC0017_COURSE_SELECTORS:15       RFC0017_PREVIEW_STAGE:unavailable
RFC0017_PREVIEW_REQUESTS:0        RFC0017_ELAPSED_SECONDS:1136
RFC0017_PLATFORM_COURSE_OK
```

**Fifteen, not macOS's seventeen, and that is the inventory's rule working
rather than two selectors missing.** `rfc0017_credential_free_native_https`
and `native_https_anonymous_read_fixture` are both anchored to
`native_https_transport_fixture` and gated to the APPLE/Linux wire block, so
`selector_expected` owes them zero here — "graph builds
native_https_transport_fixture outside the APPLE/Linux wire block". They are
neither required nor expected, and a Windows graph that registered one
anyway would have been refused as `course-selector-extra`.

The staged artifact, absolute path first:

```text
C:/code/git/bpr-windows/build/rfc0017-windows-enablement/release/src/app/better_pull_review.exe
7,755,264 bytes
sha256 bd6b7f51525d87818e3a4097708af467dbb3e779c8f99794acd337cec695abd5
```

### Against that artifact

```text
py -3 scripts/verify_rfc0017_test_inventory.py
  --build-dir build/rfc0017-windows-enablement/debug --build-config Debug
  --release-binary <artifact>              -> RFC0017_TEST_INVENTORY_OK

py -3 scripts/verify_composed_hosts.py --profile production <artifact>
  -> COMPOSED_HOSTS_OK: profile=production required=29 forbidden=7
     inspector=link /MAP | undname
```

The shipped probe, run directly under the launch environment the dev tree's
inventory declares — the Qt runtime directory
`C:/code/git/bpr-windows/build/.cpm-cache/bprqt/6.10.3/6.10.3/msvc2022_64/bin`
prepended to `PATH`, `QT_QPA_FONTDIR=C:/WINDOWS/Fonts`, offscreen — both
with and without an enterprise host. Identical output, exit 0 each time:

```text
                                       github.com   --ghes-host ghe.example.invalid
PUBLIC_PATH_OK                              yes                 yes
PUBLIC_PREVIEW_COMPOSED                   false               false
PUBLIC_PREVIEW_STAGE                unavailable         unavailable
PUBLIC_PREVIEW_REQUESTS                       0                   0
PUBLIC_PREVIEW_HEADER_EFFECTS                 0                   0
PUBLIC_PREVIEW_VAULT_EFFECTS                  0                   0
PUBLIC_PREVIEW_SIGN_IN_REQUESTS               0                   0
PUBLIC_PREVIEW_UNAVAILABLE_OK               yes                 yes
```

The enterprise answer is `unavailable` rather than `refused`, which is the
branch `check_ghes_refusal` documents for a build under the shipped
selection: it never reaches the enterprise refusal because the selection
already denied it. Either is a refusal; a request is not, and there were
none.

### The focused selectors — 16 of 18, and the two that fail

`build/windows-dev`'s cache predated the merged head, so it was reconfigured
and rebuilt at `6bfb358d` before the run rather than trusted:

```text
ctest --preset windows-dev -C Debug -R '^(core_anonymous_public_read|...)$'
  --output-on-failure          -> 16/18 passed, 2 failed, 45.25 s
```

Every RFC-0017 product selector passed: `core_anonymous_public_read`,
`github_anonymous_read_contract`, `native_https_anonymous_read_contract`
(20.18 s), `git_anonymous_public_read`, `sqlite_anonymous_epoch_authority`,
`application_anonymous_read_host`, `application_anonymous_diff_authority`,
the three `qt_anonymous_public_preview_scale_100/125/200`, all four
`application_anonymous_public_preview_*`, `rfc0017_test_inventory`, and
`rfc0017_sanitizer_course_contract`.

The two failures are **not** course selectors — the course's own fifteen are
green — and both reproduce identically in the course's freshly built dev
tree, so neither is an artifact of a reused build root.

**`rfc0017_platform_course_launch_contract`** — `47 tests, FAILED (failures=4,
errors=2)`, every one of them the wire block:

```text
ERROR: test_a_complete_registration_passes
       CourseFailure: course-selector-extra:macos:native_https_anonymous_read_fixture
ERROR: test_an_unrelated_registration_is_not_an_extra
FAIL:  test_the_inventory_names_every_owed_selector_and_no_other
FAIL:  test_the_same_absence_is_a_missing_selector_inside_the_wire_block (macos)
FAIL:  test_the_same_absence_is_a_missing_selector_inside_the_wire_block (linux)
FAIL:  test_a_full_graph_owes_every_inventory_selector_plus_two
```

The suite is hermetic only on a host already inside the wire block.
`selector_expected` reads the real `sys.platform`, not the platform name
being audited, and `tests/scripts/rfc0017_platform_course_tests.py` has an
`as_windows` helper that empties `WIRE_BLOCK_PLATFORMS` for its Windows
cases but no inverse for its macOS and Linux cases — those inherit the
host's answer. On the integrator's macOS host that is accidentally right,
which is why [[BPR-0306]] recorded "47 tests, OK". On Windows the same
synthetic macOS listing is judged by Windows' owed set and its wire
selectors read as extras.

**This does not touch the course itself.** The course requires
`arguments.platform == host_platform()` and fails `platform-mismatch`
otherwise, so a real run never audits a platform it is not on, and the
Windows run above is internally consistent. What is broken is the contract
that is supposed to prove the audit correct, on this platform only.

**`test_network_isolation_manifest`** — 39 `TEST_NETWORK_ISOLATION_FAILED`
lines over thirteen registrations. The manifest's pinned Windows
uncontained set has drifted from what the tree registers:

```text
rfc0017_release_absence_negative_control
  is pinned as bpr_windows_uncontained but is registered contained;
  retire it from the pinned set
native_https_device_authorization_fixture, and eleven application_agent_*
  chat and finding contracts
  use bpr_windows_uncontained but are not in the pinned Windows set
```

The pinned set carries `application_agent_invocation_*`; the
`application_agent_chat_*` and `application_agent_finding_*` families that
landed since are not in it. This is a Windows-only gate — the uncontained
label exists only here — so an integrator on macOS would not see it, and it
is not RFC-0017 product behaviour. It is still a hygiene gate this card's
required final state names, so it is recorded, not waived.

### Claims

| Claim | Command | Result |
|---|---|---|
| Typed public-preview unavailability, target-native, from a root deleted whole | the ledger's exact Windows course command above | exit 0, 1136 s, `RFC0017_PLATFORM_COURSE_OK`, `SELECTION:unavailable`, `COURSE_SELECTORS:15`, `PREVIEW_STAGE:unavailable`, `PREVIEW_REQUESTS:0` |
| Zero Credential Manager, header, REST/GraphQL, smart-HTTP, browser, mutation, child-process and private-content effects | the shipped probe under the declared launch environment, with and without `--ghes-host ghe.example.invalid` | `COMPOSED:false`, `STAGE:unavailable`, and `REQUESTS`, `HEADER_EFFECTS`, `VAULT_EFFECTS`, `SIGN_IN_REQUESTS` all `0` both ways; `PUBLIC_PREVIEW_UNAVAILABLE_OK` |
| The inventory validates the shipped artifact | `verify_rfc0017_test_inventory.py --build-dir <dev tree> --build-config Debug --release-binary <artifact>` | `RFC0017_TEST_INVENTORY_OK` |
| The production graph is whole in the shipped artifact | `verify_composed_hosts.py --profile production <artifact>` | `COMPOSED_HOSTS_OK: required=29 forbidden=7` |
| PE, runtime and dependency gates target-native | inside the course, under the declared launch environment | passed; no embedded browser engine |
| The focused matrix-row selectors | `ctest --preset windows-dev -C Debug -R '^(…)$'` at the merged head | **16/18** — every RFC-0017 product selector green; `rfc0017_platform_course_launch_contract` and `test_network_isolation_manifest` fail as recorded above |
| No enablement claim | — | none made; the Windows selection stays unavailable and nothing was flipped |

Nothing was flipped, no browser was opened, no device flow was started, and
the course sends no request for public preview. The two failures above are
the card's remainder.
