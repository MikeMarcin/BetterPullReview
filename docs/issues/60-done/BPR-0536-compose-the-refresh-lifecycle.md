---
id: BPR-0536
title: Compose the device-authorization refresh lifecycle
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0450, BPR-0526]
---

# Compose the device-authorization refresh lifecycle

## Why

The independent review of the closeout head `28fa0741` ([[BPR-0285]],
2026-09-03) found that `bind_refresh_authority`, `resume_refresh`, and
`refresh_observation()` have no caller outside
`device_authorization_application_host` and its tests. Without a bound
refresh authority `authorize()` parks the refresh token in `PendingRefresh`,
`schedule_refresh()` returns at once, and nothing resumes at restart. In
the shipped product the user token therefore expires after about eight
hours with no renewal and no typed `reenrollment_required` ever reaching
Qt, while the long-lived refresh token stays resident in process memory for
the session. The ledger's claim-matrix row 9 ("refreshes before expiry and
atomically rotates access/refresh material under existing vault
authority") was verified only at the host level, which the ledger's own rule
forbids counting, and three documents state the product behaviour the
composition lacks.

The review also found a latent defect the composition would expose
([[BPR-0285]] residual R1): cancelling during a re-enrollment's refresh
store leaves `RefreshStoreContext` outstanding, so a later
`on_refresh_stored` sets `RefreshDurable` and arms a renewal from a
cancelled flow, and a failed store reports `reenrollment_required` after a
cancel. No test covers it.

## Required final state

- On the authenticated `/user` observation for the device-enrolled profile,
  composition binds the refresh authority; on profile resume it resumes
  refresh; `refresh_observation()` reaches the panel so `reenrollment_required`
  is presented.
- Cancel forgets the outstanding refresh-store context; a store completing
  after a cancel neither arms a renewal nor reports re-enrollment; tests
  cover both.
- A composed course proves a refresh before expiry rotates material under
  the existing vault authority through the real graph, and the tests-disabled
  Release composes the same binding.
- The ledger row, the RFC's token-handling disposition, and the three
  documents describe the composed behaviour.

## Non-goals

- Changing the host's refresh contract ([[BPR-0450]]) beyond the cancel fix.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, both dependencies terminal. A scout confirmed the
review's reading before any edit: the host's lifecycle is complete and
tested, composition never calls `bind_refresh_authority` or
`resume_refresh`, the six-field presentation is a closed list so the
refresh observation needs a second projection method, `perform(cancel)`
lacks the store-context reset `shutdown()` already has, and the composed
clock has no testing seam, so a composed rotation needs a short-lived
documented grant or a clock seam. The slice runs on `bpr-0536-work` in a
worktree, after [[BPR-0537]] so the two do not edit `main.cpp` at once;
the three documents and ledger row 9 are the integrator's at close.

## Part one landed — 2026-09-03

At `5fc62071`. The three refresh observation types moved from the host
header to `device_authorization_projection.hpp`, and the projection port
gained a pure `publish_refresh(const device_authorization_refresh_observation&)`;
five implementers were updated, so a projection that drops renewal no
longer compiles. The host publishes on change from every lane turn
(`deliver_refresh()` wraps `post()`), including from `bind_refresh_authority`
and `resume_refresh`. `perform(cancel)` now cancels and forgets the
outstanding refresh-store context exactly as `shutdown()` does; three
refresh tests cover cancel-then-late-store through a held-store seam on
the vault double, and the sharper mutation — keeping the vault cancel and
deleting only the context reset, the exact R1 defect — fails at "a store
that answered after the cancel arms no renewal".

`github_application_host::accepted_credential_authority()` exposes the
accepted session's credential authority key, gated on the accepted
session so a key from a session that later failed is never handed out.
Composition binds it to the device host for `Profiles.front()` on an
accepted session while the device stage is `authorized` this run, and
resumes on a later different key for that profile; a self-test sink
counts bindings, resumes, and publications. The sign-in panel renders one
line (`bpr-device-authorization-renewal`) for `reenrollment_required` and
nothing for the other states.

Reproduced on the merged tree: build clean, 23/23 device selectors in 253 s, boundaries, the
touched files tidy and format clean; both composition pins held at the
worker's values.

Not yet delivered, and why part two exists: no composed course can see
the binding (removing it leaves every selector green), and after a
process restart the device stage is not `authorized`, so a persisted
device-grant session never resumes renewal — the composed app calls
`resume_profile` nowhere, and the stage rule alone cannot tell a persisted
grant from a pasted token. Part two adds the renewable signal from the
GitHub host, the resume on it, the `--device-authorization-refresh-course`
with a course-only clock offset over the fixture's real refresh slot, and
the composed-host anchors.

## Part two landed — 2026-09-03

At `2e8594ab`. The restart half of the review's B2: the GitHub host now
probes its own refresh slot once on every session acceptance that
carries a credential authority (`start_refresh_probe()` issuing
`retrieve_refresh` under the accepted authority) and answers
`accepted_credential_renewable()`. Presence is the whole answer, because
only a device grant ever writes that slot, and the device stage cannot
say so after a restart. Nothing is retained: the completion destroys the
retrieved material on the vault's own thread and only a bool-equivalent
crosses to the decision lane; only an affirmative answer posts, so every
existing turn ordering is byte-identical; the probe has its own context,
so the single-active-vault-context invariant holds. The accepted session
is republished once the answer arrives, and composition's rule is now:
stage `authorized` this run → bind; renewable persisted session → resume;
anything else → nothing, so a pasted token never earns a false "get a
code" line. The old "later different key" heuristic is gone.

The composed-host audit now requires the one composition function that
binds and resumes (`bpr::app::github_application::implementation::bind_device_refresh_authority`)
in both profiles, so an image that lost it fails the audit rather than
answering B2 again silently; it survives the Release image as a local
text symbol, and the production audit reports 30 required anchors. The
observation reader beside it is inlined into its one caller in Release
and is not required — the dev-profile selectors prove it.

Reproduced on the merged tree: build clean, 27/27 device, GitHub host, and composition rows in 264 s, boundaries, tidy
and format on the touched sources; both composition pins recomputed
(`full `29935feca0b3…`, manifest `c3c0f8fa316a…``).

Still owed, and why a third part exists: the composed course
(`--device-authorization-refresh-course`) that proves a rotation through
the real graph over the fixture's real refresh slot with a course-only
clock offset, the counters it prints being the only reader of the
composition's binding, resume, and publication counts; the negative
mutation that depends on it; and the three documents and ledger row 9.

## Part three landed, and the card closes — 2026-09-03

At `883ab6b4`. `--device-authorization-refresh-course` (tests-enabled,
implying the registered course) drives the real graph over the fixture:
the fixture vault serves a real refresh slot, `refresh_access_token`
answers from `documented-v1/` with a `-renewal-<N>` stand-in so a rotation
is observable, and a course-only clock (`composition_device_refresh_clock`)
latches a 28 800 s offset the moment composition binds — not on the
authorized stage, because the host reads `AuthorizedAt` from the same
clock and a stage latch would move both ends — so `schedule_refresh`'s
wait is zero and the real timer fires. The course prints
`DEVICE_FLOW_ROTATIONS`, `REFRESH_STATE`, `VAULT_STORES`, `DURABLE_COPIES`,
`REFRESH_BOUND`, `REFRESH_RESUMES`, and a terminal `REFRESH_OK|FAILED`;
`application_device_authorization_refresh_course` passes on
`STAGE:authorized`, `REQUESTS:0`, `ROTATIONS:1`, `REFRESH_STATE:scheduled`,
`VAULT_STORES:2` (one enrollment, one rotation), `DURABLE_COPIES:0`,
`REFRESH_BOUND:true`, `REFRESH_RESUMES:0`, `REFRESH_OK`.

Getting there took a diagnosis with a temporary trace, all instrumentation
removed. Three fixture facts stood between the grant and the binding: the
course connected before the grant, so the grant's `enroll_profile` arrived
as a second establishment; the fixture vault's delete matched the whole
operation context (request and identity generation) where a real vault
matches the authority, so the superseding delete under the new generation
answered `invalid_request` and the host published `vault_unavailable`;
and the strict recordings were one-shot. Under the course the delete
matches by authority, the recordings rewind, the capability probe repeats,
and the course does not connect first; off the course every guard is
byte-for-byte what it was. The same trace showed the registered course's
single store is the fixture's connect credential, not the grant — carded
as [[BPR-0539]] rather than changed beneath a committed row.

Deleting `DeviceHost->bind_refresh_authority(*authority)` turns the row red
(`ROTATIONS:0`, `REFRESH_STATE:idle`, `VAULT_STORES:1`); `REFRESH_BOUND`
stays `true` there because it counts composition's own attempt one line
above the call, which is why the two markers are independent.

Reproduced on the merged tree: build clean, 31/31 device, GitHub host, public-path, audit, inventory, manifest, and composition rows, boundaries, tidy
and format on the touched sources, both pins recomputed (`full `8a56faca07c1…`, manifest `8a78e04c78dc…``); a
fresh Release image carries neither the course flag nor the documented
fixture root and passes the widened inventory scan and the production
composed-host audit (0 and 0 occurrences of the flag and the fixture root; `RFC0016_TEST_INVENTORY_OK`; `COMPOSED_HOSTS_OK` with 30 required anchors). The RFC-0016 inventory allocates the
selector with `--device-authorization-probe` and the course flag, the
flag joins the forbidden set, and the row is pinned for Windows.

The three documents and ledger row 9 describe the composed behaviour.

| Claim | Command | Result | Commit |
| --- | --- | --- | --- |
| Composition binds the refresh authority on the accepted session and resumes a persisted grant after a restart | `ctest --preset dev -R '^(application_device_authorization_(host\|refresh)\|application_github_host)$'` | passed; the renewable probe's accessor tested for both kinds of material | this commit |
| The refresh observation reaches the panel and cancel forgets the outstanding store | `ctest --preset dev -R '^(application_device_authorization_refresh\|qt_device_authorization_scale_(100\|125\|200))$'` and the R1 mutation (vault cancel kept, context reset deleted) | passed; the mutation fails at "a store that answered after the cancel arms no renewal" | 5fc62071 |
| One rotation through the real graph under the existing vault authority | `ctest --preset dev -R '^application_device_authorization_refresh_course$'` | `REFRESH_OK` with the markers above | this commit |
| The binding is load-bearing | the binding call deleted, restored | the row fails with `ROTATIONS:0`; restored, passes with no repin | 883ab6b4 |
| The tests-disabled Release composes the same binding and carries no course seam | `python3 scripts/verify_composed_hosts.py --profile production <image>`; `strings -a` for the flag and the documented root; `python3 scripts/verify_rfc0016_test_inventory.py --release-binary <image>` | 0 and 0 occurrences of the flag and the fixture root; `RFC0016_TEST_INVENTORY_OK`; `COMPOSED_HOSTS_OK` with 30 required anchors | this commit |
| The device selectors, the audit, the inventory, the manifest, and the pins hold | the reproduction set on the merged tree | 31/31 device, GitHub host, public-path, audit, inventory, manifest, and composition rows | this commit |
| The documents and the ledger describe the composed behaviour | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
