---
id: BPR-0285
title: Anchor RFC-BPR-0016 capability-critical verification
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0035, BPR-0275, BPR-0276, BPR-0277, BPR-0278, BPR-0279, BPR-0280, BPR-0281, BPR-0282, BPR-0283, BPR-0284, BPR-0310, BPR-0311, BPR-0318, BPR-0516, BPR-0526]
---

# Anchor RFC-BPR-0016 capability-critical verification

RFC base: `b969554fb45d838b2a4ea11811363bd7e158e47e`.

## Run objective and membership

This is the terminal anchor for the first RFC in the requested sequence:
RFC-BPR-0016, then RFC-BPR-0017, then remaining unattended macOS-ready work.
Its planned membership is exactly BPR-0274 through BPR-0285, target-native
platform cards BPR-0310/BPR-0311, distributable-identity card BPR-0318, plus
pre-existing RFC-BPR-0002 closeout BPR-0035/BPR-0036. Any newly discovered
required work is added explicitly to this dependency closure before closeout.

Historical runs are provenance only. Every row requires final replay at the
immutable closeout head and an independent capability-critical review.

| State | Acceptance criterion | Direct verification surface | Expected result | Gate |
|---|---|---|---|---|
| [ ] | GitHub.com device enrollment reaches the existing vault with no settings/SQLite copy. | Tests-disabled public composition plus vault/settings/SQLite inspection against recorded contracts | Exactly one existing-vault enrollment; no alternate copy or partial profile | external-contract + automated |
| [ ] | Core, Qt, and projection contain no client identifier, service endpoint, secret device code, or token material; the host display projection contains only user code, validated verification URI, and expiry. | Compile-time boundary tests plus source/symbol/type inventory | Forbidden facts cannot cross; exact allowed display fields reach Qt | automated |
| [ ] | Pending, denied, expired, rate-limited, and disabled outcomes are typed and clean. | Host replay of every sanitized real-service fixture | Distinct terminal/continuation behavior; zero retained transient material | automated |
| [ ] | Polling honors interval, backoff, total bound, cancellation, and shutdown. | Controlled-clock/manual-executor host tests | No early/late/extra poll; deterministic erasure and quiescence | automated |
| [ ] | Certificates and redirects fail closed on both endpoints. | Native HTTPS hostile loopback contracts | Zero forwarded credential/body on certificate, unknown-authority, or query redirect | automated |
| [ ] | Off-authority verification addresses never open. | Address-validation and browser-capability cut test | Typed refusal and zero browser calls | automated |
| [ ] | Browser-open failure leaves authorization live and completable elsewhere. | Qt/host failing-opener public course | Code/address remain; later authorization succeeds | automated |
| [ ] | Manual token enrollment remains functional for every profile. | Public composition manual-before/device/manual-after courses | Existing vault path remains first-class and unchanged | automated |
| [ ] | A selected GitHub App refreshes and rotates vault material safely. | Controlled-clock host, native HTTPS refresh, and vault rotation/restart contracts | Refresh precedes expiry; stale/canceled results cannot rotate; failure becomes typed re-enrollment with no leaked prior material | automated |
| [ ] | Unregistered and GHES profiles refuse with zero requests. | Tests-disabled public counters and hostile capability selection | Exact `device_flow_unavailable`; zero constructed/sent requests | automated |
| [ ] | Evidence is local/target-native and secret-free. | Clean matrices, Release/runtime/dependency scans, evidence audit | No GitHub Actions evidence and no secret/transient material | automated + target-native |

Every completed row records its exact command or attended interaction, result,
commit, and durable evidence in `docs/verification/RFC-BPR-0016.md`. Registered
selectors must be enumerated there before final replay; invented future command
names do not count.

The seeded exact commands, proof conditions, pending results/evidence, and
commit fields live in
`docs/verification/RFC-BPR-0016.md#final-command-ledger`; BPR-0282 replaces
planned selectors only by updating that ledger and this anchor atomically.

BPR-0282 exercised that clause once, on 2026-08-25. The planned selector set
had no row on which the first acceptance criterion below — one existing-vault
enrollment with no settings or SQLite copy — could be observed: all three
planned application selectors run against a build with no registered identity,
where nothing enrolls and the claim is vacuously true.
`application_device_authorization_registered_course` was added to the focused
matrix and to the ledger in the same commit. It composes the recorded BPR-0276
grant against the real host, the real enrollment boundary, and the real vault,
then reads every file under the application data root looking for the enrolled
token. The tests-disabled equivalent still requires a distributable identity
and remains BPR-0318's to enable and this card's to replay.

## Phase checklist

- [ ] The immutable planning snapshot, complete live issue graph, verification
      ledger, and attended packets are committed and revalidated at closeout.
- [ ] Eligibility decisions and external contracts are committed and every RFC
      gate is satisfied or narrowed by an exact accepted exception.
- [ ] Core, GitHub adapter, host, Qt, and production composition slices are
      terminal in dependency order.
- [ ] Security/hostile/public/Release and every claimed supported-platform
      target-native matrix is terminal; unavailable platforms remain explicit.
- [ ] BPR-0318 records a distributable production identity before any platform
      becomes available; the private owner-only verification identity is never
      treated as distribution authority.
- [ ] Simplification is terminal before the final documentation impact review.
- [ ] Every documentation-ledger row in
      `docs/verification/RFC-BPR-0016.md` is updated or has an exact no-impact
      disposition.
- [ ] Final clean replay and independent capability-critical review accept the
      immutable closeout head.
- [ ] The idempotent `verify_rfc0016_platform_course.py` final-head course passes
      with `--expected-device-selection enabled --fresh` on macOS, Linux, and
      Windows after BPR-0318 and every implementation, simplification,
      documentation, and platform card is terminal; one-time flip records are
      provenance only.
- [ ] Only after every row is satisfied, the RFC status, index, verification
      ledger, and issue board transition atomically to `Auto-Verified`/terminal.

## Closeout checklist

- [ ] BPR-0035 records RFC-BPR-0002 Auto-Verified after BPR-0036's real Windows
      x64 evidence; no available-platform substitute is counted.
- [ ] BPR-0275 through BPR-0284 and BPR-0318 are terminal and the documentation
      impact ledger is resolved against the immutable base.
- [ ] Clean `dev`, `core-only`, sanitizer, tests-disabled Release, focused native
      HTTPS/Qt/application, format, strict tidy, documentation, dependency,
      runtime, secret-scan, and diff gates pass at the closeout head.
- [ ] The verification ledger records exact commands, interactions, results,
      artifact paths, and commits for every acceptance row and claimed
      supported platform; macOS alone does not prove Windows or Linux.
- [ ] An independent `rfc-critical-reviewer` falsifies the exact claim matrix,
      application-selection path, packet provenance, and negative controls with
      no unresolved blocker.
- [ ] RFC status, verification ledger, issue board, and all claimed commits and
      artifacts agree before the RFC is marked `Auto-Verified`.

## BPR-0311 dispatched to bpr-win-jaegaw — 2026-08-29

[[BPR-0311]] is the last non-terminal target-native card in this closure. It
sits in `21-ready-win/`, so it is not startable on `bpr-main-mac`: every row it
owns is Windows-native, and the card itself excludes macOS, Wine,
cross-compilation, mocks, and hosted workflows as substitutes. Its
`windows-app-release` preset is not separable into a portable half either —
its only proof is a target-native configure — so the board's split rule
declines that split.

The Windows host came up on `windows-lane` through
`scripts/launch_remote_station.py win --machine jaegaw --read-only`. Two facts
about that launch bound this dispatch:

- The station is `read-only-proof`. It builds, tests, and reports; it makes no
  commits, pushes, or tracker edits. So the dispatch covers pre-flip evidence
  only. BPR-0311's production-selection flip and its post-flip Release/public/
  native/hygiene rerun stay with a lane-commit station or the integrator, and
  the card cannot become terminal on this dispatch.
- The station has not yet appeared under its deterministic name in a peer agent
  list. `docs/development/remote-stations.md` treats an unbound name as
  undispatchable, because dispatch to one resolves to a silent auto-variant and
  misroutes instead of failing. Until the name binds, the envelope goes to the
  station by hand-paste, which the envelope contract explicitly permits.

Integrator prep committed on `main`: reservation `idr-bpr-20260829-babf`,
`BPR-0483..BPR-0490`, reservation commit `acfb6fef`. The block is not required
for a `read-only-proof` dispatch — that station cannot mint cards at all — and
is reserved against the station gaining push credentials; its IDs are burned if
that does not happen. Cards proposed by the reply are created by the integrator
on `main`.

`bpr-main-mac` holds no other startable RFC-BPR-0016 work: this closure's only
open cards are BPR-0311 and the backlog anchor itself.

## Two platforms now prove registered selection under the `unavailable` name — 2026-09-02

[[BPR-0318]] recorded for macOS that `application_device_authorization_unavailable`
proves the registered selection under a selector whose name says
`unavailable`, and left the rename to this card. [[BPR-0311]] makes it two
platforms (Windows answers `cancelled` there, for the reason [[BPR-0515]]
adjudicates), and [[BPR-0310]] three. The final-head replay should rename
the selector, or record why the name stays, before it closes.

## Windows final-head replay — 2026-09-03

`main` at `28fa0741` merged into `windows-lane` as **`bc4db3fc`**, which is
the head this replay ran at. No selection was mutated and no source changed:
this is RFC-BPR-0016's closeout replay, run idempotently over the head the
ledger anchors.

### Host and toolchain

```text
Windows 11 Home 10.0.26200 (x64), the JaegAW host
Visual Studio 18 Community, MSVC 14.51.36231, x64 Native Tools (vcvars64)
CMake 4.4.2   Python 3.14.7 (py -3)   Qt 6.10.3 (msvc2022_64)
```

Under the [[BPR-0493]] procedure — Smart App Control off, every command from
the MSVC environment, the build root deleted whole rather than trusted to
`--fresh` — and with Git's `usr/bin` appended to `PATH` for `perl`, per the
[[BPR-0036]] addendum. Appended, never prepended: that directory also ships a
`link.exe`, and `check_platform_surfaces` shells `link /MAP`.

### The course

```text
py -3 scripts/verify_rfc0016_platform_course.py --platform windows
  --dev-preset windows-dev --release-preset windows-app-release
  --build-root build/rfc0016-windows-closeout
  --expected-device-selection enabled --fresh
```

Exit 0, from a root that did not exist when it started:

```text
RFC0016_PLATFORM:windows          RFC0016_SELECTION:enabled
RFC0016_COURSE_SELECTORS:17       RFC0016_DEVICE_STAGE:cancelled
RFC0016_DEVICE_REQUESTS:0         RFC0016_ELAPSED_SECONDS:1119
RFC0016_PLATFORM_COURSE_OK
```

Seventeen selectors, matching macOS and Linux since [[BPR-0523]] added
`composed_application_hosts` to the focused set.

The artifact it staged:

```text
C:/code/git/bpr-windows/build/rfc0016-windows-closeout/release/src/app/better_pull_review.exe
7,757,312 bytes
sha256 14ac0a472ba8f48842aa2fe096923ff7a6ebe0888303531acbb006753315a4f7
```

### Against that artifact

```text
py -3 scripts/verify_rfc0016_production_identity.py
  --registration-observation docs/verification/reviews/
    rfc0016-production-app-registration-v001/registration-observation.json
  --course-evidence-root docs/verification/reviews/
    rfc0016-production-app-registration-v001/read-only-course
  --require-complete --validate-existing --release-binary <artifact>
```

```text
RFC0016_PRODUCTION_GATE:rfc0016-production-app-registration-v001
RFC0016_PRODUCTION_RECORD_COMMIT:c82e51c2c933887632fb0bb35db876375a151de9
RFC0016_PRODUCTION_CLIENT:Iv23liitiYN1WSSbag50
RFC0016_PRODUCTION_PERMISSIONS:9      RFC0016_PRODUCTION_COURSE_PROBES:10
RFC0016_PRODUCTION_COURSE:complete    RFC0016_PRODUCTION_EVIDENCE_FILES:11
RFC0016_PRODUCTION_IDENTITY_OK
```

```text
py -3 scripts/verify_rfc0016_test_inventory.py --release-binary <artifact>
  -> RFC0016_TEST_INVENTORY_OK

py -3 scripts/verify_composed_hosts.py --profile production <artifact>
  -> COMPOSED_HOSTS_OK: profile=production required=29 forbidden=7
     inspector=link /MAP | undname
```

### One request left the host, and that is this course's design

`RFC0016_DEVICE_STAGE:cancelled` means the enabled release probe reached
GitHub, was answered with a device code, and cancelled from
`awaiting_authorization` — the same behaviour [[BPR-0311]]'s and
[[BPR-0516]]'s Windows courses recorded. `RFC0016_DEVICE_REQUESTS:0` is
consistent with that and not in tension with it: per [[BPR-0515]]'s
correction, that counter counts submissions reaching the refusing transport,
never HTTP requests.

[[BPR-0515]]'s `--network-denied` override does not apply here and is not
expected to. It is registered on the two dev-tree gate rows, which is where
an unattended run had no business reaching GitHub; this is the release
course, whose entire purpose is to exercise the enabled capability against
the real service. No browser was opened and no device flow was completed —
the code was minted and cancelled inside the probe's budget, unattended.

### Claims

| Claim | Command | Result |
|---|---|---|
| The immutable head still selects the enabled device capability target-native, from a root deleted whole | the ledger's exact Windows course command above | exit 0, 1119 s, `RFC0016_PLATFORM_COURSE_OK`, `SELECTION:enabled`, `COURSE_SELECTORS:17`, `DEVICE_STAGE:cancelled`, `DEVICE_REQUESTS:0` |
| The staged artifact carries the recorded production identity | `verify_rfc0016_production_identity.py … --require-complete --validate-existing` | `RFC0016_PRODUCTION_IDENTITY_OK`; client `Iv23liitiYN1WSSbag50`, 9 permissions, 10 course probes, course `complete`, 11 evidence files, record commit `c82e51c2` |
| The tests-disabled artifact admits the inventory | `verify_rfc0016_test_inventory.py --release-binary <artifact>` | `RFC0016_TEST_INVENTORY_OK` |
| The production graph is whole in the shipped artifact | `verify_composed_hosts.py --profile production <artifact>` | `COMPOSED_HOSTS_OK: required=29 forbidden=7` |
| Isolated fresh Debug and Release inventory, runtime, dependency and PE gates | inside the course, under the launch environment the dev tree declares | passed; no embedded browser engine |
| No selection mutation, no source change | — | none; the lane's only change is this record |

Windows' side of the final-head replay is complete at `bc4db3fc`. The
selector-rename question the section above leaves open — whether
`application_device_authorization_unavailable` keeps a name that says
`unavailable` while proving the registered selection — is untouched here and
still belongs to this card.
