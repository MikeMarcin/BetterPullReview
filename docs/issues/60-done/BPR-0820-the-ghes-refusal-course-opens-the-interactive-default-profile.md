---
schema: "implement-rfc.card.v1"
id: "BPR-0820"
title: "The GHES refusal course opens the interactive default profile"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-002 integration run at the BPR-0622 slice (a) merge","found_at":"9767145d304bb0d20016fcf6b2ba113a1ca9ca9d","run":"run-20260916-stations","dispatch":"dsp-002"}
acceptance: [{"criterion":"AC1 application_device_authorization_ghes_refusal passes on macOS while an interactive BetterPullReview holds the default profile; any remaining failure named","outcome":"pass","revision":"866c5b906309f7b45cf9ed46b04d07662d8fcb75 (resealed df235fa1)","evidence":{"kind":"command","detail":"With the owner's app PID 88792 alive (start time unchanged before and after), ghes_refusal and the other three default-root rows pass with their own verdicts; 13 seam rows plus structural and restart rows 20/20 at the integrated head (build/r2-logs/dsp3-0820-int-batch.log, dsp3-0820-int-owner.log); no remaining failure; the dsp-002 cleanup_failed=1 did not reproduce and its root setup was unrecorded."}},{"criterion":"AC2 no automated course in the dev preset opens the default application data root, shown by a list or structural gate","outcome":"pass","revision":"866c5b906309f7b45cf9ed46b04d07662d8fcb75","evidence":{"kind":"artifact","detail":"docs/verification/BPR-0820-dev-preset-data-root-courses.md: 55 rows, none reaching the default root, with a control that flags runtime_process_surface without its switch; in-app gate prints AUTOMATED_PROFILE_PRIVATE_FAILED for an automated run without a root (seam-removed control dsp3-0820-control-seam-removed.log); runtime_process_surface passes with --private-data-root 3/3 at the integrated head; independent review found no uncovered path."}},{"criterion":"AC3 no GitHub or GHES network access, credential or live device authorization","outcome":"pass","revision":"866c5b906309f7b45cf9ed46b04d07662d8fcb75","evidence":{"kind":"observation","detail":"All runs were network-isolated ctest rows or fixture/private-root launches; no credential or device authorization was used; the default root was never read."}}]
---

# BPR-0820: The GHES refusal course opens the interactive default profile

## Defect

`application_device_authorization_ghes_refusal`
(`tests/end_to_end/CMakeLists.txt`, the [[BPR-0535]] registration) fails
on macOS in 0.10 s at `9767145d` with one line of output:

    PROFILE_ALREADY_OPEN

That is `github_application_startup_failure::owner_busy`. During the run, a
person's BetterPullReview was open from `build/dev` on the default profile.
The course passes no fixture and no `--test-application-data-root`, so the
tests-enabled application composes the default application data root and
competes for that profile's owner lease. Running it with `HOME` redirected
changed nothing (`build/r2-logs/dsp2-int1-ghes-refusal-private-home.log`).

Tests-disabled builds already give `--self-test` and `--render-evidence`
runs a private temporary profile (`src/app/main.cpp`, the `!BPR_BUILD_TESTING`
block before the argument parsing, from `ce6f1836`). The tests-enabled
build has no such guard, so an automated course reaches the reviewer's real
profile. At best the course fails whenever the app is open. At worst it
reads or writes that profile's stores.

[[BPR-0773]] and [[BPR-0774]] recorded this course red with a longer marker
set, 33 lines. That was a different failure, and whether it still exists is
unknown until this one is out of the way.

**More rows of the same class (dsp-002, the BPR-0802 slice).** Three more
rows stop at `PROFILE_ALREADY_OPEN` while a person's app holds the default
profile:

- `application_composition_capabilities_inside_boundary`;
- `application_composition_capabilities_product_run`;
- `application_device_authorization_public_path`.

Rerun with a private `--test-application-data-root`, the first printed
`inside / recording / unavailable / (unavailable)` and the second
`outside / system / available / uses your gh sign-in`. Both reported
`cleanup_failed=1` under that private root, which was not investigated
(`build/r2-logs/dsp2-0802-private-root-rows.log`).

## Scope

- Establish which tests-enabled courses compose the default data root: every
  self-test without a fixture or explicit test root. Decide whether the seam
  belongs in `main.cpp`, beside the tests-disabled one, or in the
  registration.
- Give those runs a private root that the run owns and removes. Do not
  weaken the profile owner lease.
- Then find out whether the course's own refusal still passes.

## Acceptance

- `application_device_authorization_ghes_refusal` passes on macOS while an
  interactive BetterPullReview holds the default profile, and the cause of
  any remaining failure is named here.
- No automated course in the dev preset opens the default application data
  root. Show this with the list of courses checked, or with a structural gate.
- No GitHub or GHES network access, credential or live device authorization.

## Fix (dsp-003)

**The seam is in the application, beside the tests-disabled one.**
`automated_run_uses_private_profile` in `src/app/main.cpp` gives every
`--self-test` or `--render-evidence` run a private temporary profile, in every
build. The exceptions are a tests-enabled run with a fixture or an explicit
`--test-application-data-root`, and a tests-disabled attended designated-live
approval, which keeps the default root as before.

- The run owns a `QTemporaryDir` (`bpr-automated-profile-XXXXXX`) that
  outlives the desktop. Its parent is narrowed with the stores' own rule,
  `make_storage_root_owner_private`, so the seam adds no platform refusal:
  `mkdtemp` is already private on POSIX, but a Windows temporary directory
  inherits its parent's ACL. The `data` leaf is created by the product's
  ordinary rule.
- `AUTOMATED_PROFILE_PRIVATE_OK` now prints in both builds.
- **The gate.** An automated run that reaches composition with no fixture and
  no root prints `AUTOMATED_PROFILE_PRIVATE_FAILED` and exits before any lease
  or store. A future start path that skips the seam then fails on every host,
  not only where a person's app is open.
- The `BPR_AUTOMATED_TEST_BOUNDARY` variable was not used: [[BPR-0802]] allows
  one reader of it.
- The profile owner lease is unchanged.

**`runtime_process_surface`** launches the idle app with no arguments, which
is an interactive launch on the default root. With a person's app open, that
launch would hand the person's app an activation. Code reading found no route
from a different root to another instance:

- the lease is `flock` on the root itself (`<root>/profile-runtime.lock` on
  Windows);
- the activation socket is `<profile root>/activate.sock`;
- `send_profile_activation` runs only when that root's lease is busy.

So:

- `scripts/verify_runtime_surface.py` has an opt-in `--private-data-root`. It
  creates an owner-private `mkdtemp` parent, passes
  `--test-application-data-root <parent>/data`, checks that the app created
  `data`, and removes the parent on every path. Without the switch the launch
  is byte-identical, so the RFC-BPR-0016 platform course and manual use
  against a tests-disabled artifact are unchanged.
- The dev-preset registration passes the switch.
- `--test-application-data-root` is now refused only with a fixture. It exists
  only in tests-enabled builds and is explicit, and restart contracts still
  pass it with `--self-test`.

**Courses checked:**
[the dev-preset data-root list](../../verification/BPR-0820-dev-preset-data-root-courses.md).
All 55 rows avoid the default root, and the control flags
`runtime_process_surface` when its switch is removed.

**The four rows.** All pass while the owner's app (PID 88792, started
Wed Sep 16 19:31:44) holds the default profile, with no remaining failure:

- `application_device_authorization_ghes_refusal` prints
  `DEVICE_FLOW_GHES_REFUSAL_OK`;
- `_composition_capabilities_inside_boundary` prints
  `inside/recording/unavailable`;
- `_composition_capabilities_product_run` prints `outside/system/available`;
- `_device_authorization_public_path` prints `DEVICE_FLOW_HOST_COMPOSED`.

The dsp-002 `cleanup_failed=1` did not reproduce under the seam, and that
run did not record how its root was made, so its cause is not named.

**Evidence** (`build/r2-logs/`).

- **Slice worktree** (`dsp3-0820-four.log`, `dsp3-0820-final.log`,
  `dsp3-0820-option-a.log`):
  - the 13 seam rows pass;
  - the private root is seen at mode 700 during a run and is gone after;
  - with the seam forced off, a direct run prints only
    `AUTOMATED_PROFILE_PRIVATE_FAILED` and `ghes_refusal` fails
    (`dsp3-0820-control-seam-removed.log`);
  - `runtime_process_surface` passes 5 of 5;
  - `runtime_audit_contract` passes, with 5 new script cases;
  - a fixture run given `--test-application-data-root` is still refused.
- **Main `2842f484` with the change** (`dsp3-0820-int-batch.log`,
  `dsp3-0820-int-surface-{1,2,3}.log`, `dsp3-0820-int-owner.log`): 20 of 20
  pass. Those rows are:
  - the 13 seam rows;
  - `architecture_boundaries`, `test_network_isolation_manifest` and
    `documentation_structure`;
  - `runtime_audit_contract`;
  - both appearance restart contracts;
  - `application_anonymous_public_preview_public_path`.

  `runtime_process_surface` passes 3 of 3. The owner's PID and start time
  are unchanged before and after. `application_composition_contract` passes
  6 of 7; its one failure is the round-trip manifest pin, which this
  `main.cpp` change moves (`dsp3-0820-int-composition.log`).

**Still open:** the independent review of this change, and the round-trip
reseal that follows it.

## Review, reseal and finish (dsp-003)

An independent `rfc-reviewer` read `866c5b90` and found **no blocker**. The
patch it read has SHA-256
`d06ce85642796ec64d952c9a92cbe51494793649d715543f4686d18335e33f57`, and its
content is identical to the commit. The reviewer found:

- **The gate.** It sits after every argument refusal and immediately before
  the only three start calls. No lease or store is touched before it. The
  reviewer's own sort of every registration and driver matched the 55-row
  list. The unattended launches without `--self-test` (`--attended-launch`,
  the native notification and appearance probes) all carry a fixture, and a
  fixture ignores `config` and requires an admitted root.
- **Release.** Only the temporary directory's prefix changed, and nothing
  references the old one. A new `AUTOMATED_PROFILE_PRIVATE_FAILED` appears
  only when narrowing fails. The Release contract still expects
  `AUTOMATED_PROFILE_PRIVATE_OK`. The attended approval and replay-reset
  paths keep the default root. A tests-disabled build never parses
  `--test-application-data-root`.
- **Lifetime.** `desktop` is destroyed before the `QTemporaryDir` on every
  exit. The restart contracts' `--self-test --test-application-data-root`
  path is unchanged. A fixture run given an explicit root is still refused.

Non-blocking findings:

- The script's usage text changed, and a lone `--private-data-root` now
  prints usage. The launch itself is unchanged.
- `make_storage_root_owner_private`'s POSIX branch follows a symbolic link
  before refusing one. It only ever sets 0700, and this path always passes
  it a fresh canonical `mkdtemp` directory.

The round-trip manifest pin was resealed in `df235fa1` for
`8a03a5be..7555cba4`. `application_review_round_trip_composition_contract`
and `application_composition_contract` (7 of 7) then pass
(`build/r2-logs/dsp3-reseal2-ctest.log`).

Windows and Linux runs of the seam rows go with the next station envelopes
as confirmation. They are not criteria of this macOS card.
