---
schema: "implement-rfc.card.v1"
id: "BPR-0535"
title: "Bind the device-authorization capability to the profile it was composed for"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0526"]
---

# Bind the device-authorization capability to the profile it was composed for

## Why

The independent capability-critical review of the RFC-BPR-0016 closeout head
`28fa0741` ([[BPR-0285]], 2026-09-03) falsified the acceptance claim that
every GHES profile returns `device_flow_unavailable` with zero requests.
`shipped_device_authorization_capability()` is per build, true on all three
platforms, and carries no profile; composition composes one device host
with a wire bound to `Profiles.front()`; [[BPR-0526]] mounts the panel bound
to whichever profile the picker selects and its begin dispatches
`begin_device_authorization_intent{Profile}` straight to the host; the core
copies the profile unchecked; `perform(begin)` checks only
`Capability.Registered` and calls `acquire_device_code`; and success calls
`Enrollment->enroll(aContext.Profile, token)`. So a reviewer who selects a
GHES profile and presses "Get a code" starts the GitHub.com device flow
with the GitHub.com client and, on authorization, enrolls a GitHub.com
token under the GHES profile. The documented "structural" guarantee — route
admission refusing non-GitHub.com contracts — binds the transport's
profile, not the intent's. `begin_public_preview_sign_in` is a second route
into the same host. No selector drives the case: the dev-tree gate's
`--ghes-host` run begins the flow for `Profiles.front()`.

## Required final state

- `application::device_authorization_capability` carries the profile it was
  composed for; composition sets it to the GitHub.com profile's identity.
- `perform(begin)` refuses a begin whose context profile is not the
  capability's with a typed `device_authorization_unavailable_event`
  before any transport call, for both the panel route and the
  public-preview sign-in route.
- A host test proves a begin for a foreign profile makes zero transport
  calls (today it reaches the transport), and a composed course under
  `--ghes-host` drives the mounted panel bound to the GHES profile and
  proves `device_flow_unavailable` with zero requests — the GHES negative the
  `unavailable` selector's `--ghes-host` argument was always meant to be.
- The RFC's GHES acceptance row, the ledger's platform table, and the two
  documents that state the guarantee describe what the composed product
  does.

## Non-goals

- Renaming the `unavailable` selector — decided on [[BPR-0285]].
- Any change to enrollment for the profile the capability was composed for.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its one dependency terminal; a worker starts in a
worktree at `f39b6243`. It runs first among the three closure cards because
it is the security defect: until it lands, the mounted panel can begin the
GitHub.com flow for a GHES profile.

## What this slice delivered — 2026-09-03

**The capability names its profile.** `application::device_authorization_capability`
gains `core::host_profile_id Profile` beside `Registered`; the shipped
selection sets it to `Profiles.front().profile()` next to the wire binding,
and the fixture's registered course sets it the same way, guarded by
`!Profiles.empty()`.

**The host refuses a foreign begin before anything else.** `perform(begin)`
in `device_authorization_application_host.cpp` refuses when
`!Capability.Registered || aRequest.Context.Profile != Capability.Profile`,
posting the existing `core::device_authorization_unavailable_event` for the
request's context — no new enumerator, no Qt-visible change — before
`++Outstanding` and before `Transport->acquire_device_code`, so no
transport, vault, or enrollment call occurs. Both routes reach that one
check: the mounted panel's `dispatch_device_authorization_gesture` and
`begin_public_preview_sign_in` dispatch the same
`begin_device_authorization_intent{Profile}` to the same host, so the
public-preview route needed no separate fix.

**Two host tests and a composed course.**
`test_a_begin_for_another_profile_is_refused_before_the_transport` asserts
zero code and token requests, zero vault calls, no enrollment, no timer, no
transient material, and the `unavailable` stage and presentation for a begin
whose profile (`host_profile_id{9}`) is not the capability's;
`test_the_composed_profile_still_begins_after_a_foreign_refusal` proves the
composed profile still begins. Under `BPR_BUILD_TESTING`,
`--device-authorization-ghes-course` selects the picker row that reads
"GHES … Native HTTPS", presses the mounted panel's begin for it, and prints
`DEVICE_FLOW_STAGE:unavailable`, `DEVICE_FLOW_REQUESTS:0`,
`DEVICE_FLOW_GHES_BEGIN_REFUSED:true`, `DEVICE_FLOW_GHES_REFUSAL_OK`;
`application_device_authorization_ghes_refusal` registers it beside the
device rows under `--ghes-host ghe.example.invalid` and `--network-denied`,
pinned uncontained, allocated in the RFC-0016 inventory with both flags
required and the course flag forbidden in a shipped build, and named in the
ledger's evidence-selector pattern. The discriminator is the stage: the
worker's mutation reduced the guard to `Registered` alone and the course
landed on `failed` — the foreign begin reached the denied loopback wire —
while the host test failed by name; restored, both pass.

**What the docs now say.** The host implementation guide's GHES paragraph,
the build guide's GHES bullet, and the ledger's platform-table row state the
profile-bound refusal and name the selector; the RFC's acceptance criterion
stands as written and is now true in the composed product. The course
selects the GHES row by label text, so a label rename would surface as
`GHES_BEGIN_REFUSED:false` rather than as a false pass; Linux and Windows
run the row in the final replays.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The slice's graph builds clean | `cmake --build --preset dev` in the slice worktree | exit 0, zero `error:` or `warning:` lines | this commit |
| Every device-authorization row including the new GHES negative, the three Qt scales, the composed-host audit, the inventory, the manifest gate, and both composition contracts | `ctest --preset dev -R '^(application_device_authorization_(host\|refresh\|ghes_refusal\|unavailable\|release_contract\|public_path\|registered_course\|browser_handoff\|browser_absent\|panel_course\|panel_absent)\|qt_device_authorization_scale_(100\|125\|200)\|composed_application_hosts\|rfc0016_test_inventory\|test_network_isolation_manifest\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 19/19 passed | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the touched files | exit 0, no output | this commit |
| The inventory names the new selector and the ledger agrees | `python3 scripts/verify_rfc0016_test_inventory.py --build-dir <slice tree>` | `RFC0016_TEST_INVENTORY_OK` after the ledger's pattern gained the name (the inventory refuses a selector the ledger does not name) | this commit |
| The foreign begin is refused, load-bearing | The worker reduced the guard to `Registered`, rebuilt, ran the host suite and the course; restored byte-exact | host: `FAILED: a begin for a profile the capability was not composed for reaches the unavailable terminal`; course: `DEVICE_FLOW_STAGE:failed`, `GHES_BEGIN_REFUSED:false`, `GHES_REFUSAL_FAILED`; restored, 17/17 | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |

Merged at `9e526804`.
