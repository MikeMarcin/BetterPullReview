---
schema: "implement-rfc.card.v1"
id: "BPR-0820"
title: "The GHES refusal course opens the interactive default profile"
status: "Ready"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-002 integration run at the BPR-0622 slice (a) merge","found_at":"9767145d304bb0d20016fcf6b2ba113a1ca9ca9d","run":"run-20260916-stations","dispatch":"dsp-002"}
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
