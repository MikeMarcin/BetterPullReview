---
schema: "implement-rfc.card.v1"
id: "BPR-0979"
title: "Implement bounded Windows profile activation and New Window handoff"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0710", "BPR-0711", "BPR-0813", "BPR-0932"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
---

# Implement bounded Windows profile activation and New Window handoff

## Scope

Complete RFC-0024 AC-06/09's native Windows single-owner launch contract.
Keep the existing exclusive profile runtime lease. Add a same-user local,
bounded activate/New Window handoff to that owner; never transport credentials
or arbitrary commands. Validate identity, request size/version, timeouts,
endpoint ownership and lifecycle; distinct profiles remain independent.
Preserve the existing POSIX short-rendezvous implementation and evidence.

Owned surfaces: profile activation adapter/contract and native Windows
implementation, minimal composition wiring if needed, adapter/public-path
tests and native candidate evidence. Never edit live station checkouts.

## Acceptance

- Native Windows repeated ordinary launch activates the owner profile's MRU
  window; explicit New Window creates another window without a second owner.
- Concurrent profiles and owner shutdown/restart remain isolated; malformed,
  cross-profile, untrusted endpoint and timed-out requests fail closed within
  explicit bounds, with no credential or arbitrary-command transport.
- Focused native Windows adapter/public tests and POSIX regressions pass.
  Unrun native acceptance remains pending and cannot be inferred from compile
  success or another platform.

## Evidence

Baseline: the Windows adapter returned unavailable; runtime leases alone did
not satisfy activation or explicit New Window.

### Implementation — 2026-09-22

Candidate: working tree over claim `9b9a267e`, macOS build only. The Windows
transport in `src/adapters/sqlite/src/profile_activation.cpp` is an
owner-private local named pipe (`\\.\pipe\bpr-activate-<digest>`, digested
from `canonical_windows_root`) bound under the same runtime lease, with the
same 16-byte frame, the same two-byte acknowledgement and the same
queued/refused/unavailable/indeterminate outcomes as the POSIX socket. The
descriptor is the adapter's existing owner-private one (user SID, plus the
package SID when contained, protected DACL); the first instance is created
with `FILE_FLAG_FIRST_PIPE_INSTANCE`, so an existing name refuses the owner
rather than being joined; a second instance keeps the name bound while a
delivered request travels with its reply, so the name is never free while an
owner lives. Message mode delimits the frame: a short, long or split message
is refused, not reassembled. Both ends check the peer process's token user;
the client additionally requires the owner-private descriptor on the pipe
and opens with `SECURITY_IDENTIFICATION` so the owner cannot impersonate it.
Server-side polling is overlapped and non-blocking on the effect lane; a
stalled peer expires after the same two-second bound; replies are written
with a bounded wait and the instance closed without discarding them. No
credential, provider command or general operation crosses the pipe. The
shared frame/digest helpers are hoisted; the POSIX section is otherwise
unchanged.

Windows tests, registered but not run here: the adapter contract's Windows
branch (`tests/sqlite_contract/profile_activation_tests.cpp`: authority and
lease refusals, duplicate listener, drained-name rebind, missing owner, two
independent client processes routed by exact root, refused New Window,
timed-out and closing-owner indeterminacy, six hostile frames, a split
frame, a stalled peer) and the public course
`application_profile_activation_public_path`, whose CMake gate is no longer
`UNIX`-only; `profile_activation_contract.py` gains a Windows branch that
narrows the root with the existing helper and skips only the POSIX endpoint
unlink/`unavailable` step, which has no named-pipe equivalent. Both rows and
`sqlite_profile_activation` are pinned `WINDOWS_UNCONTAINED`
(`dos-volume-name`, reasoned from the lease and profile rows, not measured).

POSIX regression on macOS (`build/bpr0979-focused.log`):
`sqlite_profile_activation` 4.49 s, `sqlite_profile_runtime_lease` 0.06 s,
`application_profile_activation_public_path` 5.61 s,
`application_profile_owner_admission_public_path` 2.66 s and
`test_network_isolation_structure_contract` pass; `test_network_isolation_manifest`
fails only on main's pre-existing [[BPR-0962]] rows. `check_boundaries`
passes with the existing exact local-listener exception
(`build/bpr0979-boundaries.log`); format check, `git diff --check` and the
documentation gate pass (`build/bpr0979-docs.log`); clang-tidy with
warnings as errors passes on the adapter and its contract
(`build/bpr0979-tidy.log`). Architecture, threat
model, release-readiness G9 and the authority inventory record the Windows
transport.

Not claimed: any Windows compile, test or native observation. The Windows
adapter branch, its contract and the public course need the Windows station
to build the branch head, run `sqlite_profile_activation` and
`application_profile_activation_public_path`, then observe a repeated
ordinary launch activating the owner's MRU window and an explicit
`--new-window` launch opening another window on the native desktop. Compile
success or another platform's pass cannot substitute; the `GetSecurityInfo`
read of the pipe's descriptor on the client side is an assumption the
station run will confirm or refute (a refutation surfaces as `unavailable`,
the pre-existing behavior, and fails the contract by name).
