---
schema: "implement-rfc.card.v1"
id: "BPR-0524"
title: "The Windows platform course audits two fewer selectors than macOS and Linux, and nothing says which"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# The Windows platform course audits two fewer selectors than macOS and Linux, and nothing says which

## Why

`COURSE_SELECTOR_PATTERN` in `scripts/verify_rfc0016_platform_course.py` is
platform-independent and enumerates sixteen names (seventeen since
[[BPR-0523]] added `composed_application_hosts`). The recorded rows agree
on macOS and Linux (`RFC0016_COURSE_SELECTORS:16` on [[BPR-0318]],
[[BPR-0310]], [[BPR-0516]]) and disagree on Windows (`:14` on
[[BPR-0311]] and [[BPR-0511]]). `check_course_selectors` requires 100 %,
zero failures, and a total above zero; the total is printed and never
compared, so the Windows course passes while auditing two fewer things and
no output says which two. The pattern's comment promises that "a name that
is not registered cannot be quietly skipped", which a positive-count check
does not deliver. Noticed by `bpr-linux` on 2026-09-02 while [[BPR-0523]]
was editing that function.

## Required final state

- The two selectors absent from the Windows Debug tree are named, with the
  registration condition that excludes them and whether that exclusion is
  legitimate.
- The course compares the registered set against the pattern's enumerated
  names: a name missing on a platform is a refusal by name unless a
  documented per-platform exclusion table lists it with its reason, and
  the exclusion table is the only way a platform audits fewer names.
- Every platform's recorded count is explained by the pattern and the
  table alone.

## Verification

On the Windows station first:

```text
ctest --test-dir build\windows-dev -C Debug -N -R "<COURSE_SELECTOR_PATTERN>"
```

listed against the sixteen names the pattern enumerates; then the course's
own contract (`rfc0016_platform_course_tests.py`) gains cases for a missing
name and for a documented exclusion, and each platform course passes with
its count explained.

## Windows — 2026-09-02

Station `bpr-win` at `3c62036c`, windows-dev Debug tree, build clean (exit 0,
137 projects, zero errors, zero warnings). Nothing was changed: not the course,
not the pattern, not any registration.

```text
ctest --test-dir build/windows-dev -C Debug -N -R "<COURSE_SELECTOR_PATTERN>"
Total Tests: 15
```

Fifteen of the pattern's seventeen enumerated names are registered here. The
count matches the `:14` recorded on [[BPR-0311]] and [[BPR-0511]] plus
`composed_application_hosts`, which [[BPR-0523]] added after those runs — so
nothing has changed on this platform; the pattern grew.

| # | Enumerated name | Windows Debug tree |
|---|---|---|
| 1 | `core_device_authorization` | Test #69 |
| 2 | `application_device_authorization_host` | Test #97 |
| 3 | `application_device_authorization_refresh` | Test #98 |
| 4 | `application_device_authorization_public_path` | Test #181 |
| 5 | `application_device_authorization_unavailable` | Test #182 |
| 6 | `application_device_authorization_release_contract` | Test #186 |
| 7 | `application_device_authorization_registered_course` | Test #183 |
| 8 | `application_github_credential_vault` | Test #76 |
| 9 | `github_device_authorization_contract` | Test #162 |
| 10 | `github_device_authorization_refresh_contract` | **ABSENT** |
| 11 | `native_https_device_authorization_contract` | Test #157 |
| 12 | `native_https_device_authorization_fixture` | **ABSENT** |
| 13 | `qt_device_authorization_scale_100` | Test #117 |
| 14 | `qt_device_authorization_scale_125` | Test #118 |
| 15 | `qt_device_authorization_scale_200` | Test #119 |
| 16 | `rfc0016_test_inventory` | Test #12 |
| 17 | `composed_application_hosts` | Test #235 |

## The two absent names, and what excludes them

Both are registered in `tests/github_contract/CMakeLists.txt` inside one block
opened at line 175:

```cmake
if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(BPR_TEST_NATIVE_HTTPS_OPENSSL NAMES openssl REQUIRED)
```

- `native_https_device_authorization_fixture` — line 242, loopback fixture
  driven by `verify_native_https_device_authorization.py`.
- `github_device_authorization_refresh_contract` — line 268, the same driver
  and the same executable with `--suite refresh`, registered separately
  because [[BPR-0450]] needed the ledger's exact name countable.

The condition is a **platform guard**, not a capability check: the block names
Apple and Linux and so never includes Windows. The guard carries no comment,
which is the half of this card's complaint that is about nobody being able to
tell.

## Whether the exclusion is legitimate: no, on the evidence here

The stated reason for the guard is the `openssl` binary the loopback fixtures
need for certificate generation. That reason does not hold on Windows, and the
repository already knows it doesn't. Immediately below, at line 290, an
`if(WIN32)` block added by [[BPR-0446]] says so outright:

```text
The wire fixture's only extra dependency is an openssl binary for
certificate generation; Windows does not put one on PATH, but Git for
Windows -- already a hard dependency of the product on this platform --
ships OpenSSL.
```

and finds it by `HINTS` into `Git/mingw64/bin`. Confirmed on this host:
`C:\Program Files\Git\mingw64\bin\openssl.exe` exists and is on this station's
`PATH`.

That `if(WIN32)` block registers exactly one mirror — `native_https_transport_fixture`,
with `WINDOWS_UNCONTAINED` for [[BPR-0362]]'s measured dos-volume-name cause —
and stops. So:

- The dependency the guard exists for is available on Windows and already
  located by name in the same file.
- The test executable both absent selectors drive,
  `test_better_pull_review_native_https_device_authorization_tests`, is built
  on Windows and registered here: `native_https_device_authorization_contract`
  is Test #157 above, from the unguarded registration at line 163.
- Of the four selectors inside the APPLE/Linux block, [[BPR-0446]] mirrored one
  and documented one omission — "RFC-BPR-0017's credential-free selector
  deliberately stays unregistered here". The two RFC-BPR-0016 names in this
  table are neither mirrored nor mentioned.

So the exclusion reads as an omission in [[BPR-0446]]'s Windows mirror rather
than a platform limitation. One omission in that block was deliberate and said
so; these two are silent, and they are the two this card is about.

**What this does not establish.** That the two selectors *could* be registered
is not the same as their passing. Their sibling needed `WINDOWS_UNCONTAINED`
for a measured reason, and these two would likely need the same; whether the
loopback TLS fixture and the refresh lifecycle course behave on Windows is
unmeasured, because registering them is a change to `tests/**` this task
forbids. The claim here is narrower and is the one the card asked for: the
condition that excludes them is a platform guard whose stated dependency is
satisfiable on this platform, and no comment anywhere says these two names are
meant to be excluded.

## For the exclusion table

If the course grows the documented per-platform exclusion table this card
requires, these two names are its first Windows entries, and on this evidence
the honest reason is not "unsupported on Windows" but "not yet mirrored by
BPR-0446, unmeasured on Windows" — which is a reason with an expiry rather
than a permanent exemption.

## The course names its set and its exclusions — 2026-09-02

`scripts/verify_rfc0016_platform_course.py` now enumerates the seventeen
names as `COURSE_SELECTOR_NAMES` (a contract case proves each matches the
pattern), reads what CTest registers with `ctest -N -R`, and refuses
`course-selector-missing:<platform>:<name>` for any enumerated name not
registered unless `PLATFORM_SELECTOR_EXCLUSIONS` lists it for that platform
with its reason. An exclusion whose name is registered after all is refused
as `course-exclusion-stale`, so the table cannot outlive the omission. The
table's only entries are the two Windows names, with the reason the
station established ("not yet mirrored into the Windows block, unmeasured")
and [[BPR-0525]] as their expiry.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Missing name refused by name; documented exclusion accepted only on its platform; stale exclusion refused; the enumeration matches the pattern; every exclusion names an enumerated selector with a reason | `python3 tests/scripts/rfc0016_platform_course_tests.py` | 15/15 | this commit |
| The macOS tree registers all seventeen and the check passes | `check_course_registration(registered_course_selectors(build/dev), "macos")` | `MACOS_REGISTRATION_OK` | this commit |
| The Windows course passes with fifteen registered and the two exclusions, and refuses once [[BPR-0525]] registers them without pruning the table | `bpr-win`'s next course run | **Pending** — target-native | Pending |

## Closed on the Windows station — 2026-09-02, `bpr-win` at `1818d6a0`

The course now enumerates its seventeen names, reads what CTest registers, and
accepts fewer only through `PLATFORM_SELECTOR_EXCLUSIONS`. Run under the
[[BPR-0493]] procedure from a build root deleted whole, with
`--expected-device-selection enabled --fresh`:

```text
RFC0016_PLATFORM:windows
RFC0016_SELECTION:enabled
RFC0016_COURSE_SELECTORS:15
RFC0016_DEVICE_STAGE:cancelled
RFC0016_DEVICE_REQUESTS:0
RFC0016_ELAPSED_SECONDS:982
RFC0016_PLATFORM_COURSE_OK
```

Fifteen registered names plus the table's two documented Windows entries
accounts for all seventeen. It refused with neither `course-selector-missing`
nor `course-exclusion-stale`.

That is the card's complaint answered on both halves: the count is now
explained rather than merely printed, and a name that disappears from a
platform is a refusal by name instead of a silently smaller number. The
Windows count moved from an unexplained `:14` to an explained `:15` without
any selector changing — [[BPR-0523]] added `composed_application_hosts` to the
pattern between the two runs.

The two entries in the table are this card's own finding, carried with the
reason the station reported and [[BPR-0525]] as their expiry. [[BPR-0525]]'s
registration landed on this lane in the same push, so those two entries are
now stale by construction and the course would refuse them; the table is in
`scripts/**` and the integrator prunes it on merge, then asks for one final
course run expecting `RFC0016_COURSE_SELECTORS:17`. That run is [[BPR-0525]]'s
closing row, not this card's.

The card is terminal.
