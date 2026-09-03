---
id: BPR-0285
title: Anchor RFC-BPR-0016 capability-critical verification
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0035, BPR-0275, BPR-0276, BPR-0277, BPR-0278, BPR-0279, BPR-0280, BPR-0281, BPR-0282, BPR-0283, BPR-0284, BPR-0310, BPR-0311, BPR-0318, BPR-0516, BPR-0526, BPR-0515, BPR-0535, BPR-0536, BPR-0537, BPR-0539, BPR-0540, BPR-0542, BPR-0543]
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

## macOS final-head replay — 2026-09-03

Run on the integrator host (`bpr-main-mac`, macOS, Apple Clang, Qt 6) at
main `28fa0741`, which is the head this closeout replays on every platform:
every RFC-BPR-0016 card is terminal, BPR-0515's dev-tree posture is
resolved, and the RFC-BPR-0017 work that shares the transport and the
composition file is in the tree, so the replay proves the device flow
under the code that now surrounds it. No selection was mutated.

```text
python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-closeout --expected-device-selection enabled --fresh
```

A build root created fresh; exit 0 in 147 s:

```text
RFC0016_PLATFORM:macos
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
RFC0016_RELEASE_ARTIFACT:build/rfc0016-macos-closeout/release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
RFC0016_ELAPSED_SECONDS:146
RFC0016_PLATFORM_COURSE_OK
```

The staged artifact: 10,609,248 bytes, sha256
`bf9e671611ead6972a3ab9166572a194b5fa72eb2467ad9237d8752614436f1f`. Against
it, the identity admission with `--require-complete --validate-existing`
printed `RFC0016_PRODUCTION_GATE:rfc0016-production-app-registration-v001`,
`RFC0016_PRODUCTION_RECORD_COMMIT:c82e51c2…`,
`RFC0016_PRODUCTION_CLIENT:Iv23liitiYN1WSSbag50`,
`RFC0016_PRODUCTION_PERMISSIONS:9`, `RFC0016_PRODUCTION_COURSE_PROBES:10`,
`RFC0016_PRODUCTION_COURSE:complete`, `RFC0016_PRODUCTION_EVIDENCE_FILES:11`,
`RFC0016_PRODUCTION_IDENTITY_OK`; the inventory's release scan
`RFC0016_TEST_INVENTORY_OK`; and the composed-host audit
`COMPOSED_HOSTS_OK: profile=production required=29 forbidden=7`. As on
every enabled course of this RFC, the release probe reached GitHub once,
was answered with a device code, and cancelled. The proof of that reach is
the stage: `cancelled` is reachable only after acquisition succeeded and
the flow advanced past `requested`, and the same probe under local denial
lands on `failed`. `RFC0016_DEVICE_REQUESTS:0` beside it is not a request
count — it prints the unregistered refusing transport's `reached()`, which
a registered build never references, so it is the negative control that no
enabled build fell back to refusing, exactly as `bpr-linux` read it. The
dev-tree gate no longer reaches GitHub at all since BPR-0515.
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

## The `unavailable` selector keeps its name — 2026-09-03

This card's 2026-09-02 section asked the final-head replay either to rename
`application_device_authorization_unavailable`, which now proves the
registered selection under a name that says `unavailable`, or to record
why the name stays. Decided: it stays, and the reason is on the
registration itself. The name is a registration identity that six gates
bind by string — RFC-BPR-0016's test inventory (`SELECTORS`,
`REQUIRED_ARGUMENTS`), its platform course (`COURSE_SELECTOR_NAMES`),
RFC-BPR-0017's inventory (three selectors anchored on it), the
network-isolation manifest's pinned set, and both RFCs' ledger `-R`
patterns — plus the closed cards that recorded it. Renaming it at closeout
would move every one of those pins for a cosmetic gain and make the closed
cards' recorded commands stop matching the tree, which is the kind of
drift [[BPR-0524]] and [[BPR-0525]] spent three station round trips
repairing. What the name now means is documented where the selector is
registered: the dev-tree device-authorization gate under whatever selection
the platform ships, denied locally since [[BPR-0515]], with `unavailable`
being the stage only an unregistered platform reaches. A rename can be its
own slice after the RFC closes, if anyone still wants it.

## Independent capability-critical review of `28fa0741` — 2026-09-03

Produced by an independent `rfc-critical-reviewer` at max effort over the
head the three replays ran against, read-only, no network. The record is
verbatim; the integrator's reproduction and adjudication follow it.

Role: rfc-critical-reviewer | Model: claude-fable-5-1 | Effort: max

###### Scope and head

Head `28fa07418ebee5eb79ecb2a74b2126313dd20093` (clean tree). Read in full: the RFC; `docs/verification/RFC-BPR-0016.md`; the BPR-0285 card; cards BPR-0282, 0284, 0310, 0311, 0317, 0318, 0515, 0526 (0274–0281, 0283, 0516 skimmed head/tail only); core model + transition; host header + implementation; projection; GitHub contract + transport adapters; Qt panel; `system_browser_opener`; composition regions of `github_application.cpp` (1640–1900, 3100–3320, 3985–4140, 4330–4360, 4540–4760, 7340–7470, 8040–8135); `main.cpp` 3200–3320, 4690–4830, 6400–6720, 7095–7170; `workbench_window.cpp` mount/rebind; `native_https_transport` binding, device-route and lease code; `tests/end_to_end/CMakeLists.txt` 1–490; `verify_rfc0016_platform_course.py` in full; the inventory, production-identity verifier and attended runner by structural grep (bodies not read line by line); both security-document sections; the production packet records, course evidence and attended verdict; digest bindings recomputed locally. Two `rfc-utility` children ran the committed-evidence secret scan (clean: no token prefixes, bearer headers, or populated `access_token`/`refresh_token`/`device_code`/`client_secret` fields; fixtures list them under `omitted_transient_fields`) and the `BPR_BUILD_TESTING`/forbidden-set inventory. Not done: no build, no ctest, no network, no browser; Qt scale tests, native hostile fixtures, sanitizer output, and the offline review package internals were not read; the pending platform replays were not assessed.

#### Reproduction commands

All local and read-only.

```sh
git rev-parse HEAD                                   # 28fa0741…
# B1: the device begin path compares no profile anywhere
sed -n '361,392p' src/application/src/device_authorization_application_host.cpp
sed -n '1768,1776p;3210,3224p;4017,4060p;4609,4628p' src/app/src/github_application.cpp
sed -n '951,972p;2566,2575p' src/adapters/qt/src/workbench_window.cpp
sed -n '93,131p' src/core/src/device_authorization_transition.cpp | grep -n Profile
# B2: refresh lifecycle has no production caller
grep -rn 'bind_refresh_authority\|resume_refresh\|refresh_observation' src/ --include='*.cpp' --include='*.hpp' | grep -v device_authorization_application_host   # expect: no output
# V1/V2: ledger vs tree
grep -n '^status' docs/rfcs/RFC-BPR-0002-github-host-auth-capability-layer.md; sed -n '23p;170,172p;208,209p' docs/verification/RFC-BPR-0016.md
# V3: stale BPR-0515 posture in delivered documents
grep -n 'BPR-0515' docs/architecture/security-threat-model.md docs/development/build-and-test.md
# V4: the marker's meaning
sed -n '9,11p' tests/end_to_end/CMakeLists.txt; sed -n '8058,8063p' src/app/src/github_application.cpp; sed -n '211,217p' docs/issues/60-done/BPR-0310-verify-linux-device-authorization.md
# Evidence hygiene and digest bindings
grep -rnoE '\b(ghu|ghr|gho|ghp|ghs)_[A-Za-z0-9]{8,}|github_pat_[A-Za-z0-9_]{8,}|"(access_token|refresh_token|device_code|client_secret)"\s*:\s*"[^"]{4,}"' docs/verification/reviews docs/verification/rfc0016-live-capture-v001 tests/fixtures/github/device-authorization   # expect: no output
shasum -a 256 scripts/run_rfc0016_production_course_attended.py docs/verification/reviews/rfc0016-production-app-registration-v001/read-only-course/registration.json   # 7a27a176…, f995a880… match course-execution.json
# R3: Release flag surface vs forbidden set
sed -n '6583,6604p' src/app/main.cpp; sed -n '163,171p' scripts/verify_rfc0016_test_inventory.py
```

#### Blocking findings

**B1 — A GHES profile can start the GitHub.com device flow and enroll its token.** Claim falsified: RFC acceptance "Every profile without a registered application identity, including all GHES profiles, returns `device_flow_unavailable` with zero requests sent"; BPR-0285 row 10; ledger platform table line 172; `docs/architecture/github-host-implementation.md:1234-1243`; `docs/development/build-and-test.md:2388-2390`. Evidence: `shipped_device_authorization_capability()` is per build, true on all three platforms (`github_application.cpp:1768-1776`); one device host is composed with `Registered=true` and a wire bound to `Profiles.front()` (`4017-4060`, `4113-4133`); `--ghes-host` is parsed unconditionally (`main.cpp:6410-6413`) and appends profiles 3/4 (`github_application.cpp:3210-3239`); BPR-0526 binds the mounted panel to whatever the picker selects (`workbench_window.cpp:951-958`, `2566-2575`) and its begin dispatches `begin_device_authorization_intent{Profile}` straight to `DeviceHost->dispatch` (`github_application.cpp:4609-4628`); the core copies the profile without checking it (`device_authorization_transition.cpp:113-122`); `perform(begin)` checks only `Capability.Registered` and calls `acquire_device_code` (`host.cpp:361-392`); success calls `Enrollment->enroll(aContext.Profile, token)` → `enroll_profile(3, github.com token)` (`host.cpp:709-714`, `github_application_host.cpp:740-763`). The documented "structural" guarantee (route admission refusing non-GitHub.com contracts) binds the transport's profile, not the intent's. `begin_public_preview_sign_in` is a second route (`4711-4720`; `PublicPreviewProfile` is set before the enterprise refusal at `github_application_host.cpp:831`). No selector drives this: `application_device_authorization_unavailable` uses `select_device_authorization()` = `Profiles.front()` (`4633-4641`). Smallest fix: add `Profile` to `device_authorization_capability`, set it to `Profiles.front().profile()` in composition, and in `perform(begin)` post `device_authorization_unavailable_event` when `aRequest.Context.Profile != Capability.Profile` (before `acquire_device_code`); add a host test "a begin for a profile the capability was not composed for refuses with zero transport calls" (today it reaches the transport) and a composed course with `--ghes-host` pressing the panel bound to profile 3.

**B2 — The refresh lifecycle is never composed; the shipped product neither refreshes nor stores refresh material.** Claim falsified: ledger claim-matrix row 9 / BPR-0285 row 9 ("refreshes before expiry and atomically rotates … under existing vault authority"); RFC "Token handling" (GitHub App must own refresh); `github-host-implementation.md:1200-1221`; `build-and-test.md:2380-2384`; `foundation-security.md:87-90` ("the access and refresh tokens reach the platform vault and nothing else"). Evidence: `bind_refresh_authority`, `resume_refresh` and `refresh_observation()` have no caller outside the host and its tests (grep above); without `RefreshAuthority`, `authorize()` parks the refresh token in `PendingRefresh` (`host.cpp:692-699`), `schedule_refresh()` returns immediately (`843-849`), and nothing resumes at restart. Result in the product: the ~8 h user token expires with no renewal and no typed `reenrollment_required` ever reaches Qt, while the long-lived refresh token stays resident in process memory for the session (excluded from `holds_transient_material()`, `1276-1281`). The ledger's own rule forbids verifying a row "by a linked component"; row 9 is verified only that way. Smallest fix: on the authenticated `/user` observation for the device-enrolled profile, composition calls `DeviceHost->bind_refresh_authority(key)`; on profile resume, `resume_refresh(key)`; surface `refresh_observation()` to the panel; add a composed course. Alternatively, narrow row 9, the three documents, and the RFC disposition explicitly to host-level and card the product gap — but the claims as written are false.

#### Verification failures

- **V1** Ledger eligibility row 23 says "RFC-BPR-0002 Auto-Verified | Pending" while `docs/rfcs/RFC-BPR-0002-github-host-auth-capability-layer.md:4` is `Auto-Verified` and BPR-0035 is Done — the row must be closed before the RFC transitions.
- **V2** Ledger platform table lines 170–171 still record Linux and Windows as `device_flow_unavailable, zero requests`; the command ledger (208–209) and `github_application.cpp:1771` say enabled. Internally inconsistent.
- **V3** Documentation ledger rows 222 and 224 claim delivery, but `security-threat-model.md:1094-1103` and `build-and-test.md:661,712` still say BPR-0515 "is open"/"undecided" and that Windows "reaches `cancelled` by actually reaching GitHub"; BPR-0515 closed 2026-09-03 with `--network-denied` and `failed`. The task's "posture is what the ledger now says" holds for the ledger (line 209), not for the documents.
- **V4** BPR-0310 lines 211–217 ("`DEVICE_FLOW_REQUESTS:0` … the enabled artifact still sent nothing") is wrong: the marker is `UnregisteredDeviceTransport.reached()` (`github_application.cpp:8062-8063`; `CMakeLists.txt:9-11`), and a `cancelled` stage implies GitHub minted a code (BPR-0515 line 76). The Linux enabled course did send a device-code request.
- **V5** Ledger rows 141–142 say the courses now run "against the composed system opener"; every suite binds `recording_device_authorization_browser` (`github_application.cpp:4091-4096`, `main.cpp:6594-6599`). `system_browser_opener::open` (`cpp:25-46`) has no test at all (`grep -rl system_browser_opener tests/` finds only CMake/pins); its only evidence is the three owner observations on BPR-0516.
- **V6** The GHES and refresh documentation rows (B1/B2) describe behaviour the composed product lacks.

#### High-risk residuals

- **R1 (defect, latent because of B2)** Cancel during a re-enrollment's refresh store: `perform(cancel)` (`host.cpp:394-413`) leaves `RefreshStoreContext` outstanding; `on_refresh_stored` (`788-821`) then sets `RefreshDurable` and `schedule_refresh()` arms a renewal (`843-867`) that later enrolls a token from a cancelled flow, and a failed store reports `reenrollment_required` after a cancel (`823-841`). No test covers it (host and refresh test lists). Recommend: cancel/forget the store context on cancel and add the course; adjudicate explicitly if deferred.
- **R2 (accepted traffic)** The final-head replays run the tests-disabled artifact with no denial (`verify_rfc0016_platform_course.py:600-657`), so each enabled platform POSTs `/login/device/code` with the production client and mints a code unattended; `DEVICE_FLOW_REQUESTS` cannot observe it. Recommend recording this as bounded accepted traffic in the replay row, renaming the marker (e.g. `DEVICE_FLOW_REFUSING_TRANSPORT_REACHED`), and noting that `expected=enabled` also accepts `failed` (`653-656`), so an offline station passes without proving the flow reaches GitHub.
- **R3 (Release surface)** The RFC-0016 forbidden set (`verify_rfc0016_test_inventory.py:163-171`) omits `--network-denied`, `--device-authorization-browser-absent`, `--device-authorization-panel-absent`, `--public-preview-*`, `--test-application-data-root`; only the RFC-0017 inventory forbids `--network-denied`, and the platform course runs only the RFC-0016 scan (`443-456`). All are structurally absent today (`main.cpp:6416-6581`, `6595-6625`, `6637-6685`). Separately, `--device-authorization-registered-course` and `--device-authorization-panel-course` are parsed unconditionally (`6586-6604`): in a Release, the first suppresses the probe's cancel (`4727-4735`) so `--self-test --device-authorization-probe --device-authorization-registered-course` polls GitHub until the budget expires. Recommend gating both like `--device-authorization-browser-probe` and adding the flag strings to the RFC-0016 list.
- **R4** `application_device_authorization_unavailable` proves registered selection and its `--ghes-host` argument proves nothing about GHES (card already notes the rename); after B1's fix it should become the GHES negative.
- **R5 (residual uncertainty, unverified)** What `enroll_profile(GHES, github.com token)` leaves behind (partial vault entry vs. clean refusal) was not traced. The dev graph now defines `BPR_NATIVE_HTTPS_TESTING`, so the credential-free binding answers session-matched reads there (`native_https_transport.cpp:3022-3031`); Release returns `false` and the smart surface refuses it (`3228-3231`) — the BPR-0317 authenticated-context requirement (`3137-3160`) is not bypassable by the device or anonymous bindings as read, but I did not execute the contracts. Packet provenance replays by digest (all seven recomputed hashes match the validation record and `course-execution.json`); the private client `Iv23lil08fH3933w9MkW` check depends on `tests/fixtures/github/device-authorization/v1/registration.json` remaining the record.

### Reproduction and adjudication — 2026-09-03

The integrator ran the review's read-only commands at `28fa0741` and each
reproduced: the capability constant carries no profile and the begin path
compares none; the refresh lifecycle has no production caller; the ledger's
eligibility row for RFC-BPR-0002 still reads Pending against an
Auto-Verified RFC; the platform table still lists Linux and Windows as
unavailable; the threat model and the build guide still call [[BPR-0515]]
open; and one course flag is parsed outside `BPR_BUILD_TESTING`.

Adjudicated as follows, and the closure of this card widened to match
before any closeout claim:

- **B1 is a blocking defect** and becomes [[BPR-0535]]: the capability
  carries its profile, the host refuses a foreign begin before any
  transport call on both routes, and a composed GHES course proves it.
- **B2 is a blocking gap** and becomes [[BPR-0536]], together with residual
  R1: the refresh lifecycle is composed on the `/user` observation and on
  resume, `reenrollment_required` reaches the panel, cancel forgets the
  outstanding store, and a composed course proves rotation.
- **R2, R3, and the untested opener validation (V5)** become [[BPR-0537]]:
  every tests-enabled flag gated and forbidden by string, the enabled
  course requiring `cancelled`, the replay row naming its one bounded
  request, and a validation test for `system_browser_opener`. The marker's
  name stays; its meaning is recorded on this card and on the registration.
- **V1–V4** are corrected in the same commit as this record: the RFC-0002
  eligibility row, the platform table, the threat model's and build guide's
  BPR-0515 sentences, and a correction note on [[BPR-0310]].
- **V5's ledger wording** on rows 141–142 is corrected now to say the suites
  bind the recording opener and the composed opener is proven by linkage,
  by the composed-host anchor, and by the owner's three observations.
- **R5** is answered by B1's fix (a foreign begin never reaches enrollment)
  and by the reviewer's own reading that the BPR-0317 authenticated-context
  requirement is not bypassable by the device or anonymous bindings.

Consequence for the closeout: RFC-BPR-0016 stays `Deferred`. The three
replays recorded above are provenance for `28fa0741`; the immutable final
head is the one at which [[BPR-0535]], [[BPR-0536]], and [[BPR-0537]] are
terminal, and the replays, the gates, and a second independent review run
again there. [[BPR-0515]] is added to this closure explicitly because the
Windows posture it decided is now part of what the replay proves.
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

## The review's blocking findings are closed; one more evidence defect found — 2026-09-03

[[BPR-0535]] (B1: a GHES profile can no longer begin a GitHub.com device
flow), [[BPR-0537]] (the Release flag surface, the `cancelled` expectation,
and the opener's observed refusals), and [[BPR-0536]] (B2 and R1: the
refresh lifecycle composed, resumed after a restart, proven by one rotation
through the real graph) are done, and [[BPR-0534]] made shadowing an error
on every compiler after the C4456 that only the Windows station caught.
Strict tidy is clean over `src/` and `tests/` at this head.

The trace that composed the refresh course found that the ledger's
registered-contract vault course counts the fixture's own connect
credential as its one store, and that the grant's enrollment aborts on a
delete the fixture refuses where a real vault would not; that row's claim
holds by coincidence. [[BPR-0539]] corrects the course and the row before
the final-head replays run, so this anchor now depends on it. The replays
on macOS, Linux, and Windows, the second independent review, and this
card's closeout all wait for that head.

## The platform course's own table had drifted — 2026-09-04

The first final-head macOS replay at `0bd39349` passed with
`RFC0016_COURSE_SELECTORS:17` — the same count as at `28fa0741` — while
the inventory it required had grown to 26. `scripts/verify_rfc0016_platform_course.py`
hand-listed its selectors, and its registration check (BPR-0524) could
refuse a listed name missing from the tree but never a registered
evidence selector nobody had added to the list; seven device rows
([[BPR-0516]]'s browser handoff and cut, [[BPR-0526]]'s panel course and
cut, [[BPR-0535]]'s GHES refusal, [[BPR-0536]]'s refresh course, and
[[BPR-0537]]'s opener test) were therefore unaudited by every platform's
replay. The Linux station measured the same gap independently before
reporting, and named the four capture-tooling security rows as the part a
naive sync would have swept in.

The course now derives its owed set from the inventory the way
RFC-BPR-0017's does: owed is the inventory minus five reasoned
`FAMILY_EXCEPTIONS` (the four `rfc0016_*_security` rows the ledger runs
under its isolated capture-tooling row, and the course's own hermetic
contract) plus the two course-only names; a registered family name that
is neither is refused as `course-selector-extra`, and a stale exception is
refused. The enumeration and the anchored pattern are computed from that
set (24 on every platform), and four hermetic cases pin the derivation, the
extra refusal, the registered-but-not-owed exception, and the stale
exception. Both stations' first replays were held and discarded as claims;
the immutable final-head replays below ran at the head that carries the
derived table.

## Second independent capability-critical review of `0bd39349` — 2026-09-04

Run read-only at the head that closed BPR-0535, BPR-0536, BPR-0537,
BPR-0534, and BPR-0539, with the first review's findings as its brief. The
review's text, verbatim:

Role: rfc-critical-reviewer | Model: claude-fable-5-1 | Effort: max

Head `0bd393490bc8fc626c96bb36041ed83b848778da`, clean tree. Read-only; no build, no ctest, no network. Read in full: the ledger, BPR-0285/0535/0536/0537/0539, the device host, the GitHub host's probe and activation paths, the composition's device regions, the fixture vault, the refresh tests, the RFC. Ran only `nm`/`strings` on the two existing images plus `verify_rfc0016_test_inventory.py` and `verify_composed_hosts.py` in check mode.

## Blocking findings

**B1 — A pasted token for an account that once completed a device grant is silently replaced by a device-refreshed token; this is the only production path that reaches `resume_refresh`.** Claim: RFC acceptance "Manual token entry remains available and functional on every profile, including profiles where device flow succeeded" (ledger row 143). Trace: no product code ever calls `delete_refresh` (`grep -rn 'delete_refresh(' src` finds only the five implementations), while the composition's `delete_retained_on_shutdown` policy deletes the base credential on every orderly shutdown (`github_application_host.cpp:1369-1379`, `3548-3550`). The keychain keys the refresh slot by the authority (`macos_keychain.cpp:1329-1360`, service `github-native-refresh`), and `credential_key` (`github_application_host.cpp:281-292`) is deterministic per profile+account. So after a grant and a quit, the keychain holds an orphaned 6-month refresh token. On the next launch, pasting a PAT for the same account → `/user` → same key → acceptance → `start_refresh_probe` finds the orphan (`2214-2305`) → `republish_accepted_session` → `bind_device_refresh_authority`: stage is not `authorized` this run, `accepted_credential_renewable()` is true → `resume_refresh` (`github_application.cpp:4897-4905`) → `begin_refresh` immediately → the orphan is spent at GitHub → `Enrollment->enroll(profile, device access token)` (`device_authorization_application_host.cpp:867-873`) supersedes and deletes the PAT session. The panel shows nothing (`scheduled` renders no line). Reproduction without network: a fixture course variant that pre-seeds the fixture refresh slot, connects with the fixture credential instead of the grant, and expects `DEVICE_FLOW_REFRESH_RESUMES:1` with `VAULT_STORES:2` and the last stored digest equal to `fixture-access-token-0276-renewal-1` — a passing run demonstrates the defect. Smallest fix: `delete_refresh` under the same authority wherever the base credential is exact-deleted (shutdown, sign-out, superseding enrollment from a manual source), so "presence is the whole answer" becomes true.

**B2 — Refresh material has no erasure rule, and the shutdown-cleanup evidence cannot see it.** Claim: RFC "Token handling" ("refresh token storage under the same vault rules", "no separate erasure rule"); `foundation-security.md:87-90`; the ledger's "under existing vault authority" (row 144/203). Before BPR-0536 nothing ever reached the refresh slot; now it does and nothing ever removes it. The fixture's retained-secret check reads only the base slot (`github_public_path_fixture.cpp:2178`, `SecretRetained = Credential.has_value()`), and `RefreshStores`/`Refresh` are never reported, so `application_device_authorization_refresh_course` passes with the refresh token left in the fixture vault after the orderly shutdown that deleted the access token. Stronger row: print `DEVICE_FLOW_REFRESH_RETAINED:<bool>` from `state->Refresh.has_value()` at evidence time and pin `false`.

## High-risk non-blocking findings

- **R1 — The "restart resume" is unreachable in the composed product.** `resume_profile` has no caller in `src/` outside the host and its tests; `github_credential_binding_catalog` is process-local (`github_application_host.hpp:62-90`) and the documentation says so ("no … process-restart recovery", `github-host-implementation.md:1366-1373`). Without a binding an activation has neither enrollment nor resume and fails with `vault` (`github_application_host.cpp:1696-1706`). So "a persisted grant resumes renewal after a restart" (ledger row 203, `github-host-implementation.md:1216-1222`, `build-and-test.md:2401`, BPR-0536 claim row 1) describes a path no launch can take; `REFRESH_RESUMES:0` is pinned and the branch is exercised only by B1's wrong case.
- **R2 — Immediate refresh on resume turns a transient failure into `reenrollment_required`.** `resume_refresh` calls `begin_refresh()` at once (`host.cpp:1293-1303`); any `transport_failed` becomes a typed re-enrollment prompt with no retry (`1031-1037`, `1060-1070`) while the access token still works. Latent until R1 is composed.
- **R3 — Audit anchor strength.** `bind_device_refresh_authority` is a local `t` symbol in Release (`nm`); the audit proves presence, not the call. The dev course proves the call; acceptable but note it.
- **R4 — Rotation evidence.** The refresh course pins neither the second store's identity nor `RefreshStores`; `ROTATIONS:1` (a host counter read through the projection) plus `VAULT_STORES:2` is the whole proof of refresh-slot rotation.

## Verification failures

- **V7 — BPR-0537's second required-final-state bullet is undelivered:** the replay row (ledger line 210) still does not record the one device code each enabled platform mints per replay; only the card sections do.
- **V8 — The Release image at `build/app-release` is further behind the head than the brief states:** `git diff --stat 9af4d58d..HEAD -- src` shows BPR-0539's `main.cpp`/fixture edits and BPR-0532's new `agent_realignment.{hpp,cpp}` (326 lines in `src/application`). Its scans (`RFC0016_TEST_INVENTORY_OK`, `COMPOSED_HOSTS_OK required=30`) are provenance, not final-head evidence.
- First-review items, at this head: V1 resolved (ledger line 23; RFC-0002 front matter `Auto-Verified`). V2 resolved (lines 170-171). V3 resolved (`security-threat-model.md:1087`, `build-and-test.md:661,712`). V4 resolved (BPR-0310 line 241). V5 resolved (rows 141-142 reworded; `application_device_authorization_browser_address` registered at `tests/end_to_end/CMakeLists.txt:833`, in the inventory; `substitute_browser_launch` absent from Release, present in dev). V6 resolved for GHES; for refresh see R1. R2 resolved in the course (`verify_rfc0016_platform_course.py:664-672` requires `cancelled`) but not in the ledger row (V7). R3 resolved: all fourteen forbidden strings are 0 in Release and 1 in dev; the Release carries only `--self-test`, `--device-authorization-probe`, `--public-preview-probe`, `--ghes-host`, `--automated-run-budget-ms`. R4 resolved by `ghes_refusal`. R5 resolved by the guard.
- Fixture honesty by row: registered course — `VAULT_STORED_TOKEN:f5505a4b89c07e21` is an external digest and `STORED_IS_GRANT` compares to the transport's issued token; skipping enrollment fails it (card's control). Refresh course — the markers read `DeviceRefreshObservation` (`github_application.cpp:8522-8524`), so losing `publish_refresh` or the binding yields `idle`/`ROTATIONS:0`; see R4. GHES refusal — `GhesBegan && Stage == unavailable` under `--network-denied`; a missing guard or a panel not bound to the GHES row lands on `failed` (card's mutation). R1 tests — `test_a_store_that_answers_after_a_cancel_arms_no_renewal` holds the store across `perform(cancel)`; deleting only the context reset leaves `RefreshStoreContext` matching and `schedule_refresh` arms (lifetime set at `authorize`), so the mutation fails as claimed. B1 (first review) — the guard at `host.cpp:401-428` precedes `++Outstanding`; both routes dispatch the same intent (`4926-4945`, `5028-5037`); `bind`/`resume` are gated to `Profiles.front()` (`4872`); no bypass found.

## Residual uncertainty

B1 is traced, not executed. Whether GitHub honours an unused refresh token from a superseded session is documented behaviour, not observed here. I did not read the Linux/Windows vault implementations for the same slot separation.

## Unsupported suspicions

A panel rebound to a GHES profile keeps rendering the GitHub.com host's renewal line (cosmetic). `perform(begin)` for the composed profile does not cancel an armed renewal timer; a new grant simply supersedes it.

**Verdict:** RFC-BPR-0016 may not move to Auto-Verified at this head — B1 and B2 stand until refresh material shares the base credential's erasure path and a row can observe it.

### Adjudication

- **B1 and B2 are accepted as blocking** and carded as [[BPR-0540]]: the
  refresh slot shares the base credential's erasure path (shutdown,
  sign-out, a superseding manual enrollment), the resume answers only for
  device-grant provenance and never for slot presence, the fixture reports
  the refresh slot and both device courses pin it absent after shutdown,
  and an orphan course proves a pasted token never resumes. The closeout
  waits for it; the ledger's refresh row and the three documents were
  reworded to the truthful scope in the same commit as this record.
- **R1 and R2 are accepted** and carded as [[BPR-0541]] (a persisted
  binding across a restart, with the first refresh deferred and retried).
  They do not block: no acceptance row claims a session survives a
  restart, and the claims that implied it were reworded.
- **R3 accepted as stated**: the audit proves the binder's presence; the
  dev course proves the call.
- **R4 is folded into BPR-0540's evidence** (`REFRESH_STORES` pinned).
- **V7 is resolved by the closeout**: the replay row's closing text names
  the one device code each enabled platform mints per replay.
- **V8 is by construction**: each platform's replay builds its Release
  image fresh in a deleted root and scans that image; the integrator's
  standing `build/app-release` is provenance only.
- The first review's V1–V6 and R2–R5 are recorded resolved at this head,
  with the citations the review gives.

## Two enabled platforms have no refresh slot — 2026-09-03

Found while reviewing [[BPR-0540]]'s erasure rule, which treats a vault's
`unavailable` answer to `delete_refresh` as benign because "a platform
without a second slot answers that". Two shipped platforms do:
`linux_secret_service.cpp` and `windows_credential_manager.cpp` implement
`store_refresh`, `retrieve_refresh`, and `delete_refresh` as stubs that
answer `unavailable` without touching the store (the Windows comment says
the slot "has no Credential Manager target of its own yet" and defers it
to [[BPR-0311]], which closed without it). The registration issues
expiring user tokens with refresh tokens, so a real Linux or Windows
sign-in parks the refresh token, flushes it on the `/user` binding
([[BPR-0536]]), meets `unavailable`, and `fail_storage` reports
`reenrollment_required` (`storage_failed`) right after the sign-in
succeeded; the token then expires with nothing to renew it. Every enabled
course stops at `cancelled` before a grant and every composed course runs
over the fixture vault, which has a real slot, so no row could see it.

Adjudicated as blocking for rows 1 and 9 on those platforms and carded as
[[BPR-0542]] (Linux, `23-ready-lin/`, dispatched to `bpr-linux`) and
[[BPR-0543]] (Windows, `21-ready-win/`, dispatched to `bpr-win`). Both
are added to this closure; the final-head replays run at the head that
carries them. BPR-0540's erasure rule is corrected in the same slice to
treat every error as a failure, since after these cards `unavailable`
means only an unreachable store, exactly as it does for the base slot.
