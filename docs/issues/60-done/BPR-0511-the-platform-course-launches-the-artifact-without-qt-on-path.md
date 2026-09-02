---
id: BPR-0511
title: The platform course launches the shipped artifact without the Qt runtime directory on Windows
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The platform course launches the shipped artifact without the Qt runtime directory on Windows

## Why

`bpr-win` ran [[BPR-0311]]'s exact ledger command on the JaegAW host on
2026-09-02, with Smart App Control off and both [[BPR-0492]] and [[BPR-0494]]
landed, and reached step nine of ten. `verify_runtime_surface.py` refused
with `application exited before runtime inspection`; launched by hand, the
shipped artifact exits at once with `0xC0000135` (`STATUS_DLL_NOT_FOUND`) and
prints nothing.

The repository already solved this for every registered test:
`cmake/BprTestIsolation.cmake` prepends the directory of Qt6::Core's DLL to
`PATH` and names a font directory for each one, because nothing deploys Qt
beside the executables and a Windows loader finds a shared Qt only on `PATH`
([[BPR-0430]], [[BPR-0412]]). The platform course launches the shipped
artifact **outside** CTest, twice — `check_runtime_and_dependencies` and
`check_device_selection` — and neither launch inherits that declaration. It
is the third consumer the course missed on Windows, after the multi-config
tree shape ([[BPR-0492]]) and the symbol reader ([[BPR-0494]]).

With the directory prepended by hand the station measured every assertion
the tenth step makes: `RUNTIME_SURFACE_OK`, and the self-test probe at
`DEVICE_FLOW_HOST_COMPOSED:true`, `DEVICE_FLOW_PRESENTED:true`,
`DEVICE_FLOW_STAGE:unavailable`, `DEVICE_FLOW_REQUESTS:0`,
`DEVICE_FLOW_UNAVAILABLE_OK`. That is the product working and the launch
environment wrong, which is why the station recorded it as a hand measurement
and not as the course's own row.

## Required final state

- Both direct launches run the shipped artifact with the same `PATH` prefix
  and font directory the registered tests get, taken from the tree's own
  declaration rather than from a second resolution of Qt.
- A Windows tree that declares no Qt runtime directory fails the course by
  name, not with an artifact that exits before `main`.
- macOS and Linux launches are unchanged: their trees declare no such prefix
  and the environment stays the ambient one.
- A registered test pins the derivation and shows both launch steps receive
  it.
- [[BPR-0311]]'s exact ledger command reaches `RFC0016_PLATFORM_COURSE_OK` on
  the Windows station without a hand-repaired environment.

## Non-goals

- Deploying Qt beside the shipped artifact. That is packaging, outside
  RFC-BPR-0016.
- The Visual Studio developer-environment requirement the same run surfaced
  (Ninja for `windows-app-release`, `undname` for the symbol reader). That is
  a procedure, recorded under [[BPR-0493]].

## Verification

```text
python3 tests/scripts/rfc0016_platform_course_tests.py
ctest --preset dev -R '^rfc0016_platform_course_launch_contract$' --output-on-failure
py -3 scripts/verify_rfc0016_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0016-windows-enablement --expected-device-selection unavailable --fresh
```

The last command is BPR-0311's and runs on the Windows station; this card is
done when it reaches the tenth step with the course's own environment.

## Repair — 2026-09-02

`environment_from_inventory` in `scripts/verify_rfc0016_platform_course.py`
reads the Debug tree's `ctest --show-only=json-v1` inventory, which the course
already trusts for its selector check, collects every
`PATH=path_list_prepend:` and `QT_QPA_FONTDIR=set:` entry the registered
tests declare, and applies them to the ambient environment the way CTest
would. On Windows an inventory with no prefix is the named refusal
`qt-runtime-directory-undeclared`. `check_runtime_and_dependencies` and
`check_device_selection` take that environment; `verify_runtime_surface.py`
copies its own environment into the artifact's, so the prefix reaches the
loader on both steps.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The declaration is applied, once, in front of the ambient `PATH`; an undeclared Windows tree is refused by name; other platforms are unchanged; both launch steps receive the environment | `python3 tests/scripts/rfc0016_platform_course_tests.py` | 9/9 | this commit |
| The test is registered and isolated | `ctest --preset dev -R '^rfc0016_platform_course_launch_contract$' --output-on-failure` | Recorded target-native below | `3d2726ba` |
| The ledger command reaches the tenth step with the course's own environment | BPR-0311's exact command on `bpr-win` from a checkout of this commit | Recorded target-native below | `3d2726ba` |

## Target-native results — 2026-09-01, station `bpr-win` at `3d2726ba`

Both pending rows are measured, and **nothing was prepended by hand**. The
build root was removed whole, the run was made under `vcvars64.bat`, and the
only environment the shipped artifact got is the one the course derives.

| Claim | Command | Result |
|---|---|---|
| The derivation, the named refusal, the unchanged other platforms, and both launch steps | `py -3 tests/scripts/rfc0016_platform_course_tests.py` | **9/9** |
| The test is registered and isolated | `ctest --test-dir build/windows-dev -C Debug -R '^rfc0016_platform_course_launch_contract$' --output-on-failure` | **Passed, 0.17 s**, label `bpr_network_isolated` |
| The ledger command reaches the tenth step with the course's own environment | BPR-0311's exact command, `--expected-device-selection unavailable --fresh` | **`RFC0016_PLATFORM_COURSE_OK`, exit 0, 941 s** |

The tenth step's own markers, which is the row this card exists for:

```text
RFC0016_PLATFORM:windows
RFC0016_SELECTION:unavailable
RFC0016_COURSE_SELECTORS:14
RFC0016_DEVICE_STAGE:unavailable
RFC0016_DEVICE_REQUESTS:0
RFC0016_ELAPSED_SECONDS:941
RFC0016_PLATFORM_COURSE_OK
```

`check_runtime_and_dependencies` and `check_device_selection` both launched the
shipped artifact and both got answers, where at `6fff0a53` the first refused
with `application exited before runtime inspection` and the artifact died at
`0xC0000135` before `main`. The station's earlier hand-prepended measurement is
superseded by this one: the course now produces the row itself, which is
exactly the distinction that kept it off the ledger last time.

Release artifact the run produced:

```text
build/rfc0016-windows-enablement/release/src/app/better_pull_review.exe
7,543,296 bytes
sha256 23be95d78756b48f85e126717f497d928c1f4b75c658a7d1f449ec8deefa5f4f
```

### One note on the Verification block

It names `ctest --preset dev`, which is not a Windows preset — `dev` has a
non-Windows host condition. The Windows reading is the `windows-dev` tree with
an explicit `-C Debug`, which is what was run. The integrator is correcting the
wording on `main` at integration; it is recorded here so the command in this
card is not read literally by the next Windows station.

The card is terminal.
