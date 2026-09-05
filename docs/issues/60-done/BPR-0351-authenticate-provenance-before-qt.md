---
schema: "implement-rfc.card.v1"
id: "BPR-0351"
title: "Establish supervisor provenance before QApplication and public observation"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0357"]
completed: "2026-08-18"
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

## Result — 2026-08-18

The shipped application now refuses attended authority unless the exact bound
supervisor launched it, settled before `QApplication` exists.

`src/app/src/designated_live_macos_provenance.{hpp,cpp}` is the channel, shared
by the two executables that have to agree on it. [[BPR-0357]] proved the
mechanism in a test binary; this moves it into the product, and the contract
that exercises it links the shipped source rather than a copy.

- Stage one claims an empty `SERVICE_SLOT`, publishes a send right, spawns
  suspended, and restores its exact prior snapshot immediately — whether or not
  the spawn succeeded, so no later spawn from that task inherits a channel it
  was not meant to have. The destructor restores too, because a right left
  published outlives the launch it was for.
- The application recovers the right before Qt, greets, and requires the
  supervisor's fixed reply from a peer whose code identity equals the
  supervisor image inside its own capsule. It derives that path from its own
  location rather than being told, because being told would be an argv-carried
  claim and fact_3 forbids one.
- Identity is taken from the kernel audit trailer on both sides, keyed by
  `kSecGuestAttributeAudit`. The launch path previously asked
  `guest_signing_digest(process)` — a pid-keyed question, which a pid reused
  between the spawn and the asking would answer wrongly. That check remains as
  a pre-resume gate; the authority now rests on the trailer.

### The audit shrank the card

BPR-0351 asks for "the ~600 lines between `QApplication` and
`github_application` construction" to be audited for a real dependency on Qt.
Most of that span is `BPR_BUILD_TESTING` scaffolding that never compiles into
the shipped application, including one 344-line block. The production Qt
dependencies in it are three: the Qt metadata setters and window icon
(`main.cpp:5023-5031`), `applicationDataRoot` via `QStandardPaths` (`:5074`),
and `config.RunningExecutable` via `QCoreApplication::applicationFilePath()`
(`:5099`).

`designated_review_authority_bootstrap::run` needs none of them — its request is
plain data and its observation port is an abstract interface implemented by the
native HTTPS transport. So the handshake did not have to be threaded through
anything. It sits beside the existing pre-`QApplication` preflight probe, which
already proved that position was reachable.

### Evidence

The wiring is production-only: a tests-enabled build rejects the approval flag
outright, so no tests-enabled runtime can observe it. Three surfaces cover it.

**The channel, hermetically.** `macos_supervisor_provenance` runs the product's
two halves against each other, ad-hoc signed exactly as the capsule signs. It
proves the supported course, that a second claim on an occupied slot refuses,
that the restored snapshot leaves the slot free for the next launch, that an
application inheriting no published right refuses outright, and that a reply
carrying the right word from the wrong peer is refused. Three consecutive
passes.

**The ordering, statically.** `architecture_boundaries` now requires
`authenticate_supervisor(` to appear before the `QApplication` construction in
`main.cpp`, requires the provenance component to contain the audit-trailer and
registered-port machinery, and forbids it from naming `getenv`, `getppid`, or
any Qt header — fact_3's prohibitions, as a checked rule rather than a comment.

**The shipped binary, at runtime.** The tests-disabled Release graph compiles
the production branch — the dev graph preprocesses it out, so this is the only
build where that code exists at all. Launched directly with the attended flag
and an invalid Qt platform:

| Build | Exit | Output |
|---|---|---|
| without the pre-Qt refusal | 134 | Qt platform plugin error — `QApplication` had been constructed |
| with it | 1 | `STARTUP_FAILED`, no Qt output whatsoever |

The differential is the point. Running only the fixed build proves nothing:
a nonexistent approval path produces `STARTUP_FAILED` either way. The invalid
platform is what separates "refused" from "refused before Qt".

### Assertions proven by mutation

| Mutation | Result |
|---|---|
| The application trusts the launch when no right was published | fails "an application that inherits no published right refuses outright" |
| The application accepts the fixed reply from any peer | fails "a reply from a peer whose code identity is not the expected supervisor is refused" |
| The pre-Qt authentication call is removed from `main.cpp` | fails the boundary rule "must authenticate its bound supervisor before accepting attended authority" |

One mutation was discarded rather than counted: deleting the empty-slot early
return does not change behaviour, because sending to `MACH_PORT_NULL` fails
anyway. It proved nothing, so it is not evidence.

### Registrations this required

Adding a file to `src/app` moves pinned state, and all of it was updated
deliberately rather than discovered later:

- bound into `bpr_attended_review_source_surface` in `src/app/CMakeLists.txt`,
  so the approval subject covers the provenance code. Leaving it out would have
  meant an approval that authorizes an application without binding the component
  deciding who may launch it.
- added to both pinned source lists in the round-trip composition contract, and
  its manifest digest re-pinned.
- registered as a macOS code-signing API owner in
  `cmake/VerifyArchitectureBoundaries.cmake`, with the CMake target that
  declares it.

### What this does not do

No disk image and no mount — fact_1 and fact_2 are [[BPR-0353]] and
[[BPR-0354]]. The approval subject still binds no supervisor or inventory
identity; that is [[BPR-0352]], and until it lands an approval names an
application digest without saying which supervisor it was granted against.
Nothing here grants live GitHub, credential, publication, packaging, or signing
authority, and no live lane was executed.
