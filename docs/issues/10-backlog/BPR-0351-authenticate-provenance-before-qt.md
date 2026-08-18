---
id: BPR-0351
title: Establish supervisor provenance before QApplication and public observation
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0350]
---

# Establish supervisor provenance before QApplication and public observation

[[BPR-0262]] fact_4: the app and stage one complete a bidirectional fixed-message
handshake and validate audit trailers and peer code identity with
Security.framework **before** `QApplication` or public observation, and refuse
direct or alternate launch.

Today they do not, and the ordering is the hard part rather than the handshake.

- `main` is `src/app/main.cpp:4928`. `QApplication` is constructed at `:4936`,
  eight lines in.
- `--attended-designated-live-approval` is not parsed until `:4966-5001`, after
  Qt exists.
- `designated_review_authority_bootstrap::run` is reached only from the
  `github_application::implementation` constructor
  (`src/app/src/github_application.cpp:1750`), and `github_application` is not
  constructed until `main.cpp:5544`.
- `launch_attended_application`
  (`src/app/src/designated_live_macos_supervisor.cpp:532-621`) hands the child
  argv and a sanitized environment and nothing else. It spawns with
  `POSIX_SPAWN_CLOEXEC_DEFAULT` and passes no file actions at all, so no
  descriptor is inherited either. The child has no parent-provenance signal of
  any kind.
- `launch_harmless_probe` (`:404-530`) already has a `socketpair` fd-3
  challenge, and the app answers it at `main.cpp:4931-4934` — before
  `QApplication`. That proves the ordering is achievable; it is also explicitly
  not an acceptable mechanism here, because fact_3 forbids a generic inherited
  pipe. Whether the two launch functions unify is an open design seam.

## Required final state

- The shipped app refuses attended authority unless it authenticates the exact
  bound supervisor through the verified path, before Qt and before any public
  observation.
- A direct GUI launch, or launch by an alternate parent, cannot forge that
  provenance through argv, environment, pid, a nonce, a generic inherited pipe,
  or persistent state. Exceeding fact_3 invalidates the approval outright.
- The ~600 lines between `QApplication` and `github_application` construction
  are audited for a real dependency on Qt existing; the handshake moves ahead of
  them rather than being threaded through them.
- Re-pin the three digests and the file list in
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`,
  and revisit
  `tests/scripts/architecture_boundary_tests.py::test_designated_live_supervisor_observes_before_app_launch`,
  which pins today's ordering.

## Verification

A hermetic course proves the exact app plus exact approval refuses direct
invocation and by an alternate launcher, and succeeds only through the bound
supervisor protocol.
