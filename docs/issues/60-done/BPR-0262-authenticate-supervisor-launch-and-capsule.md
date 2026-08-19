---
id: BPR-0262
title: Authenticate supervisor launch and retain capsule identity
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0357, BPR-0351, BPR-0352, BPR-0353, BPR-0354]
completed: 2026-08-18
---

# Authenticate supervisor launch and retain capsule identity

The BPR-0256 critical review proved that any tests-disabled shipped app accepts
the attended approval flag directly. The app cannot distinguish the verified
capsule supervisor from another parent, while the approval subject and runtime
bootstrap bind neither supervisor nor inventory. The same review identified a
check/use gap for nested capsule entries after path-based verification.

The read-only-image and kernel-audited Mach launch topology was approved for
implementation and hermetic verification only through the exact operator
statement recorded in
[designated-live-macos-containment-v001](../../verification/reviews/designated-live-macos-containment-v001/operator-packet.md).
BPR-0262 remains Backlog only because BPR-0261 must land first. The accepted
subject grants no live GitHub, credential, publication, package,
signing/notarization, or RFC-closeout authority.

## Required final state

- The shipped app refuses attended authority unless it can authenticate that
  the exact bound supervisor launched it through the verified capsule path.
- The attended approval subject binds the supervisor and capsule inventory
  identities that the operator approves; a direct GUI/app launch cannot forge
  or bypass that provenance through argv, environment, settings, or a generic
  inherited pipe.
- Supervisor provenance is established before `QApplication`/GUI creation and
  before the in-app public observation or grant bootstrap.
- The supervisor retains and revalidates the capsule identities needed to close
  the verification-to-launch window, including nested executable dependencies
  and resources; substitution at an injected post-verification barrier refuses
  launch.
- The opt-in CTest manifest still names only the supervisor. Default graphs
  remain zero-exception and tests-enabled applications remain fail-closed.

## Non-goals

- Do not grant authority from parent PID alone, a forgeable nonce, argv,
  environment, or persistent Qt/application state.
- Do not weaken signature, inventory, public-target observation, approval, or
  credential separation checks.
- Do not execute the designated live lane.

## Verification

- A production-shaped hermetic launch course proves the exact app plus exact
  approval refuses when invoked directly or by an alternate launcher and
  succeeds only through the bound supervisor protocol.
- A deterministic barrier mutates/replaces a nested capsule entry after static
  verification and requires refusal before GUI resume.
- Architecture, capsule, manifest, bootstrap, production-symbol/source,
  format/tidy, documentation, and diff gates pass.

## Tree state as of BPR-0261 closeout

Surveyed 2026-08-15 so the implementing session starts from facts. This is a
ground-up build, not an extension of partial scaffolding.

- **Nothing Mach exists.** `SERVICE_SLOT`, `bootstrap_check_in`,
  `bootstrap_look_up`, `audit_token`, `xpc_*`, and `mach_msg` appear nowhere in
  the tree outside the operator packet itself. Every element of fact_3 and
  fact_4 is new code with no regression to extend.
- **The attended launch passes argv and nothing else.** `launch_attended_application`
  in `src/app/src/designated_live_macos_supervisor.cpp` spawns the app suspended,
  compares static and dynamic signing identity by PID, revalidates the retained
  executable identity, and resumes — but hands the child only
  `--attended-designated-live-approval <path>`. The child therefore has no
  parent-provenance signal at all. `launch_harmless_probe` in the same file does
  have a `socketpair` challenge on fd 3; the two functions are not unified, and
  whether to unify them is an open design seam.
- **The grant bootstrap runs after `QApplication`, not before.** `main` calls the
  preflight probe before constructing `QApplication`, but the attended approval
  is not parsed until well after, and `designated_review_authority_bootstrap::run`
  is reached only from the `github_application::implementation` constructor,
  which `main` builds after Qt setup. Satisfying fact_4's "before QApplication or
  public observation" is an ordering change to `main`, not a local edit.
- **The approval subject binds no supervisor or inventory identity.**
  `designated_review_approval_subject` binds schema, statement, application and
  source digests, approval ID, build configuration and testing flags, the three
  replay fields, and the fixed target identity. `designated_review_bootstrap_request`
  has no supervisor, capsule, or inventory field. fact_5 requires adding both.
- **Nested capsule coverage is check-time only.** `scripts/designated_live_macos_capsule.py`
  walks the capsule recursively and its inventory covers every nested file,
  directory, and symlink, and `verify_inventory` compares a fresh recursive walk.
  But that runs in a Python subprocess that has already exited before the child
  is resumed, and the supervisor's own post-verification identity checks cover
  only the top-level application executable. Nothing revalidates a nested dylib,
  framework, or plugin between static verification and `dyld` loading it. That
  window is the check/use gap this card names.
- **No test exercises the C++ supervisor.** `designated_live_macos_capsule_contract`
  tests the Python script's functions directly; `application_attended_launch_contract`
  exercises an unrelated tests-enabled fixture flag. The only test reaching
  `designated_review_authority_bootstrap::run` is
  `application_designated_review_authority_bootstrap`. A new test surface for the
  supervisor's authentication logic is required, and whether it is a unit target
  or only an end-to-end capsule course is an open design seam.
- **Digest re-pinning is mechanically required.** Any change to
  `src/app/main.cpp`, `src/app/src/github_application.cpp`, or the `src/app` file
  set drifts the pinned digests and file inventory in
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`,
  exactly as BPR-0261 hit.
- **Signing is ad-hoc, with no entitlements.** The capsule is signed
  `codesign --force --sign - --timestamp=none` with no hardened runtime and no
  entitlements plist anywhere in the repository. The existing
  `SecCodeCopyGuestWithAttributes` guest lookup works under that signing, but
  nothing in the tree establishes whether Mach port registration and audit-trailer
  validation also do. That is an open uncertainty to probe first, not a confirmed
  blocker.
- Exceeding any of the seven approved facts — in particular fact_3's prohibition
  on any argv, environment, PID-only, nonce-only, generic-pipe, or persistent-state
  fallback — invalidates the approval outright. It is a hard non-goal, not a
  preference.

## Decomposed — 2026-08-17

This is five pieces of new work, not one slice, and the survey below understated
only how much. Split so each piece can land with its own evidence:

| Card | Fact | What it owns |
|---|---|---|
| [[BPR-0357]] | 3, 4 mechanism | Prove the SERVICE_SLOT handshake exists and works before anything is built on it |
| [[BPR-0351]] | 4 | Establish provenance before `QApplication` and public observation |
| [[BPR-0352]] | 5 | Bind supervisor and inventory identity into the approval subject |
| [[BPR-0353]] | 1 | Mount the capsule from a digest-bound read-only image |
| [[BPR-0354]] | 2 | Close the nested check/use window |

Fact_6's fail-closed conditions are not a sixth card; each belongs to the card
that owns the condition it guards. Fact_7 is a scope limit, not work.

This card stays open as the umbrella and returns to Backlog behind the five.

## The open uncertainty is resolved — 2026-08-17

The survey called ad-hoc signing "an open uncertainty to probe first, not a
confirmed blocker." Probed. It is not a blocker.

`SERVICE_SLOT` is `2` in `<mach/mach_init.h>` — an index into the task's
registered-port array via `mach_ports_register`, not a launchd service name.
That reading is what the rest of the topology depends on, so it is worth stating
plainly before it is designed around.

A throwaway probe, signed exactly as the capsule is
(`codesign --force --sign - --timestamp=none`, no hardened runtime, no
entitlements), on macOS 15.7.7 arm64 / Darwin 24.6.0:

```
PARENT slot_initially_empty=yes
PARENT register=(os/kern) successful
PARENT spawn=0 pid=98031                     # POSIX_SPAWN_START_SUSPENDED
CHILD  slot_recovered=yes port=0xb03         # after exec, via mach_ports_lookup
PARENT restore=(os/kern) successful
PARENT slot_cleared_after_restore=yes
PARENT receive=(os/kern) successful
PARENT payload=bpr-0262-probe trailer_size=52
PARENT audit_pid=98031 child_pid=98031 match=yes
PARENT sec_guest_from_audit_token=0          # errSecSuccess
PARENT sec_check_validity=0
PARENT cdhash_dynamic_present=yes static_present=yes equal=yes
```

Every element fact_3 and fact_4 need is available under this tree's signing: the
slot starts empty, a registered send right survives suspended spawn and `exec`,
the parent can restore its exact prior snapshot, the kernel audit trailer names
the child exactly, and `SecCodeCopyGuestWithAttributes` keyed by
`kSecGuestAttributeAudit` — not by pid — returns a guest whose
`kSecCodeInfoUnique` equals the on-disk binary's.

One trap worth recording. An early run returned `sec_guest_from_audit_token=100003`,
which is `kPOSIXErrorBase + ESRCH`: the child had already exited, so there was no
guest left to identify. Validation has to happen while the peer is alive, which
the real handshake gets for free by holding the child suspended. A future
implementer reading `ESRCH` as "audit tokens do not work under ad-hoc signing"
would draw exactly the wrong conclusion.

The probe was not checked in as-is. [[BPR-0357]] turns it into a contract with
its fail-closed halves.

## Survey corrections — 2026-08-17

Two details in the tree survey above are wrong in ways that would cost an
implementer time, verified against the live tree:

- `designated_review_approval_subject` is **not a struct**. It is a free function
  (`src/application/src/review_mutation_execution_authority.hpp:116-120`, defined
  `designated_review_authority_bootstrap.cpp:1461-1535`) rendering a `key=value`
  text subject. Fact_5 adds emitted fields and a `schema_version` bump, not
  struct members.
- The "opt-in CTest manifest" is not a manifest artifact. It is two CMake
  options: `BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE` (which hard-fails when
  `BUILD_TESTING` is also ON) and `BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST`,
  which registers the single `live_review_mutation_designated_target` entry.

Everything else in the survey still holds, including the check/use window, the
absent Mach machinery, the argv-only attended launch, and the pinned digests.

## Closed — 2026-08-18

All five decomposed cards are in `60-done/`, and the umbrella's own conditions
hold.

| Card | Fact | What it delivered |
|---|---|---|
| [[BPR-0357]] | 3, 4 mechanism | the SERVICE_SLOT handshake proven under the capsule's ad-hoc signature |
| [[BPR-0351]] | 4 | provenance established before `QApplication`, from the kernel audit trailer |
| [[BPR-0352]] | 5 | supervisor and inventory identity bound into the approval subject |
| [[BPR-0353]] | 1 | the capsule as a digest-bound read-only image, mounted and detached without force |
| [[BPR-0354]] | 2 | the nested check/use window closed against a retained descriptor |

fact_6's fail-closed conditions were not a sixth card and did not become one:
an occupied `SERVICE_SLOT`, a failed audit validation, a writable mount, a
detach needing force, a drifted image digest, and a substituted nested entry
each refuse in the card that owns the condition, and each is covered by a
contract that fails when the refusal is removed.

fact_7 is unchanged and unclaimed. Nothing in this program executed a live
GitHub run, used a credential, published a comment, packaged or signed
anything, or closed RFC-BPR-0014.

### The umbrella's own two conditions

- **The opt-in CTest manifest still names only the supervisor.**
  `BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST` still defaults `OFF`
  (`cmake/BprTestIsolation.cmake:12-15`) and still gates on the same four
  pre-launch-validated inputs. The default graph registers zero occurrences of
  `live_review_mutation_designated_target` — checked with `ctest -N` rather
  than assumed.
- **Tests-enabled applications remain fail-closed.** The tests-enabled binary
  answers the attended approval flag with `STARTUP_FAILED` and exit 1, exactly
  as before. The provenance handshake [[BPR-0351]] added is production-only and
  does not soften that path.

### What the survey got wrong, corrected by the work

The tree survey said satisfying fact_4's ordering was "an ordering change to
`main`, not a local edit", on the strength of ~600 lines between `QApplication`
and `github_application`. Most of that span is `BPR_BUILD_TESTING` scaffolding
that never compiles into the shipped application. The production Qt dependencies
are three, `designated_review_authority_bootstrap::run` needs none of them, and
a pre-`QApplication` hook already existed. The handshake sits beside it.

The survey was right about everything else, including the check/use window, the
absent Mach machinery, the argv-only attended launch, and the pinned digests —
which moved four times over this program and were re-pinned each time by the
procedure [[BPR-0383]] recorded.

### The limit worth carrying forward

`designated_live_macos_supervisor.cpp` is compiled by no graph that can be built
here. Its edits across [[BPR-0351]], [[BPR-0352]] and [[BPR-0354]] type-check
under `clang++ -fsyntax-only -std=c++23` with the project's include paths, and
nothing more than that. Every component it calls is compiled and exercised; the
file that calls them is not. A host that can build the capsule graph should
compile it before this topology is relied on.
