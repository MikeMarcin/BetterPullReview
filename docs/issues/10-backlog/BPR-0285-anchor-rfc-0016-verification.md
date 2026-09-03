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

## Linux final-head replay — 2026-09-03

The RFC's `Immutable final-head platform replay` row, run target-native on the
Linux station against `main` at **`28fa0741`** — the exact merged head this
lane fast-forwarded to before running, and the head these numbers describe.
Nothing was mutated: no selection flip, no source change, no card move. This is
RFC-BPR-0016's closeout replay, not RFC-BPR-0017's course.

Host: Ubuntu 26.04 LTS, `Linux 6.18.33.2-microsoft-standard-WSL2 x86_64`.
Toolchain: GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), CMake 4.4.2, Ninja 1.13.2,
Qt 6.10.3, Python 3.13.15.

Precondition, read before anything else: the Secret Service `login` collection's
`Locked` property read `(<false>,)` at 2026-09-03T08:35:41Z. Recorded because a
locked collection makes the vault-touching selectors skip rather than fail, and
a skipped selector reports the same green as a passing one.

**The replay**, from a build root deleted whole rather than trusted to
`--fresh`:

```
rm -rf build/rfc0016-linux-closeout
python3 scripts/verify_rfc0016_platform_course.py --platform linux \
    --dev-preset dev --release-preset app-release \
    --build-root build/rfc0016-linux-closeout \
    --expected-device-selection enabled --fresh
```

Exit 0. Every marker:

```
RFC0016_PLATFORM:linux
RFC0016_SELECTION:enabled
RFC0016_COURSE_SELECTORS:17
RFC0016_DEVICE_STAGE:cancelled
RFC0016_DEVICE_REQUESTS:0
RFC0016_RELEASE_ARTIFACT:build/rfc0016-linux-closeout/release/src/app/better_pull_review
RFC0016_ELAPSED_SECONDS:255
RFC0016_PLATFORM_COURSE_OK
```

The artifact it built and interrogated:

```
build/rfc0016-linux-closeout/release/src/app/better_pull_review
size    11,537,792 bytes
sha256  500d6c00d2f3f80d0329c600079fa44bc49527fc879baa452eaeaa16e093d138
ELF     64-bit LSB pie executable, x86-64, dynamically linked, for GNU/Linux 3.2.0
```

The seventeen course selectors, listed from this tree rather than assumed:
`rfc0016_test_inventory`, `core_device_authorization`,
`application_github_credential_vault`, `application_device_authorization_host`,
`application_device_authorization_refresh`,
`qt_device_authorization_scale_(100|125|200)`,
`native_https_device_authorization_(contract|fixture)`,
`github_device_authorization_(contract|refresh_contract)`,
`application_device_authorization_public_path`,
`application_device_authorization_unavailable`,
`application_device_authorization_registered_course`,
`application_device_authorization_release_contract`,
`composed_application_hosts`.

### Identity and inventory admissions against the staged artifact

```
python3 scripts/verify_rfc0016_production_identity.py \
    --registration-observation docs/verification/reviews/rfc0016-production-app-registration-v001/registration-observation.json \
    --course-evidence-root docs/verification/reviews/rfc0016-production-app-registration-v001/read-only-course \
    --require-complete --validate-existing --release-binary <artifact>
```

```
RFC0016_PRODUCTION_GATE:rfc0016-production-app-registration-v001
RFC0016_PRODUCTION_RECORD_COMMIT:c82e51c2c933887632fb0bb35db876375a151de9
RFC0016_PRODUCTION_CLIENT:Iv23liitiYN1WSSbag50
RFC0016_PRODUCTION_PERMISSIONS:9
RFC0016_PRODUCTION_COURSE_PROBES:10
RFC0016_PRODUCTION_COURSE:complete
RFC0016_PRODUCTION_EVIDENCE_FILES:11
RFC0016_PRODUCTION_IDENTITY_OK
```

```
python3 scripts/verify_rfc0016_test_inventory.py --release-binary <artifact>
RFC0016_TEST_INVENTORY_OK

python3 scripts/verify_composed_hosts.py --profile production <artifact>
COMPOSED_HOSTS_OK: profile=production required=29 forbidden=7 inspector=nm | c++filt
```

The artifact authenticates as the recorded distributable identity
`Iv23liitiYN1WSSbag50`, not the private owner-only verification client, and the
course's own `check_no_private_identity` reads that prohibition from the
committed registration record rather than carrying a copy of it.

### `DEVICE_REQUESTS:0` does not mean no request was sent

This is the one line in the block above that a closeout reader will misread, so
it is written down rather than left to the marker's name.

`DEVICE_FLOW_REQUESTS` prints `UnregisteredDeviceTransport.reached()`. On an
unregistered platform that transport *is* the composed device transport and
every submission increments it, so zero proves the capability gate held. Linux
has been registered since [[BPR-0310]], so the composition swaps in the real
credential-free wire and the unregistered transport is never referenced at all
— its count is zero **by construction**, and would be zero no matter how much
traffic the real wire carried. It remains a meaningful negative control (a
non-zero value would mean an enabled build fell back to the refusing
transport), but it is not a request count, and no closeout claim of "zero
requests on Linux" may rest on it.

What the enabled probe actually did is `RFC0016_DEVICE_STAGE:cancelled`, and
that stage is the evidence: `cancelled` is reachable only after acquisition
succeeded and the flow advanced past `requested`. The same shipped probe under
the course's network isolation lands on `failed` instead — that is the measured
value [[BPR-0310]] pinned into the dev-tree selector gate for this platform.
So the enabled course reached GitHub once, minted a real device code, and
cancelled it at the run budget without anyone authorizing. That is the course's
own design, it is what every enabled course of this RFC has done, and it is
expected here.

### Left open by this replay

The section above this one asks the final-head replay to rename
`application_device_authorization_unavailable` — which proves the *registered*
selection under a name that says `unavailable` — or record why the name stays.
This replay does neither: it is a no-mutation idempotent rerun, and renaming a
registered selector is a source change outside its authority. The question is
unresolved and still belongs to this card before it closes; it is recorded here
so the replay's green is not mistaken for having settled it.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Immutable final-head Linux replay, fresh closeout root, selection enabled and unmutated | the course command above | exit 0 in 255 s; the eight markers above | this commit |
| The head replayed is the one the ledger names | `git merge --ff-only origin/main` then `git rev-parse HEAD` | `28fa0741` | this commit |
| The keyring precondition was unlocked, so no vault selector skipped | `gdbus call … /org/freedesktop/secrets/collection/login … Locked` | `(<false>,)` | this commit |
| The artifact authenticates as the recorded distributable identity, course complete | `verify_rfc0016_production_identity.py --require-complete --validate-existing --release-binary` | `RFC0016_PRODUCTION_IDENTITY_OK`, client `Iv23liitiYN1WSSbag50`, 11 evidence files | this commit |
| No evidence selector or fixture seam survived into the shipped artifact | `verify_rfc0016_test_inventory.py --release-binary` | `RFC0016_TEST_INVENTORY_OK` | this commit |
| The artifact composes every production host and no forbidden one | `verify_composed_hosts.py --profile production` | `required=29 forbidden=7` | this commit |
| The enabled probe started the flow rather than refusing it | the course's shipped-artifact probe | `DEVICE_STAGE:cancelled`, not `unavailable` | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
