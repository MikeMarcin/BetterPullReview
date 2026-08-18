---
id: BPR-0262
title: Authenticate supervisor launch and retain capsule identity
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0261]
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
