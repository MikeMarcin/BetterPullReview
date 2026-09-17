---
schema: "implement-rfc.card.v1"
id: "BPR-0644"
title: "Renew expired device grants at launch instead of forgetting them"
status: "Parked"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P1"
parked_on: "human-review"
acceptance: [{"criterion":"A fixture course restores a binding whose access lifetime elapsed while the app was closed by rotating with the refresh token, without a prompt","outcome":"pass","revision":"75fab2d448fd6b4d2b092ed0385db435de446520","evidence":{"kind":"command","detail":"application_device_binding_launch_renewal passes at the integrated head (19.99 s, build/r2-logs/dsp2-int9-courses.log) and fails with the product change reverted (dsp2-0644-negative-control.log). The rotated grant resumes only the binding's exact account; the account-guard unit case fails without the guard (dsp2-0644c-control-*.log)."}},{"criterion":"A refresh-expired or rejected binding tombstones and offers Begin","outcome":"pass","revision":"75fab2d448fd6b4d2b092ed0385db435de446520","evidence":{"kind":"command","detail":"Unit cases in application_github_host (refresh-expired, failure, abandonment by Sign out after the store, replacement, other account) pass; a rejected renewal erases both exact slots behind the tombstone and starts the deferred session (dsp2-int9-units.log). Existing device courses, the restart course and qt_device_authorization_scale_* pass."}},{"criterion":"Restoration across an orderly quit and SIGTERM (Further observation)","outcome":"pending","revision":"75fab2d448fd6b4d2b092ed0385db435de446520","evidence":{"kind":"observation","detail":"application_device_binding_relaunch covers quit and SIGTERM with real relaunches and passes 3/3 (dsp2-int9-repeat.log), but also passes without the change, so it does not reproduce the observation. The candidate mechanism (locked Keychain or /user network failure sending restore cleanup into erasure) is recorded; keeping grants through such failures is an owner policy question (BPR-0541 fail-closed)."}},{"criterion":"The tests-disabled Mac app relaunched more than eight hours after a device sign-in resumes the session with no credential prompt (attended)","outcome":"pending","revision":"75fab2d448fd6b4d2b092ed0385db435de446520","evidence":{"kind":"freeform","detail":"Requires an attended relaunch more than 8 hours after a real device sign-in on a tests-disabled build containing 3ad87ee4b93b59ab324727f3ce3890b2859ac164."}}]
---

# Renew expired device grants at launch instead of forgetting them

Observed on the Mac private alpha candidate (`50f364b8`): a GitHub.com device
sign-in at 01:57 local produced a durable binding with an 8-hour access
lifetime and a 6-month refresh lifetime. The reviewer closed the app at 12:18
and relaunched at 12:44. The launch showed `Not authenticated`, Connect opened
the pasted-credential prompt, and the device-binding row was tombstoned at
launch. The owner's expectation: a launch either resumes the stored login or
offers the OAuth device sign-in; it never asks for a pasted credential when a
grant is stored.

Mechanism, from `github_application_host.cpp` `restore_device_profile`: when
`remaining_access_lifetime` is zero the binding is erased and the function
returns; a tombstoned row also returns without `start_deferred_session()`.
The renewal timer in `device_authorization_application_host.cpp`
(`schedule_refresh`, `begin_refresh`) only runs while the process is up, and
`begin_refresh` itself requests re-enrollment once the access lifetime has
elapsed, so any restart after the access token expires loses the session even
though the refresh token is valid for months.

## Scope

At launch, an access-expired binding whose refresh lifetime remains must run
the existing rotation path (refresh token to new access and refresh material,
stored through the same vault boundary) before the deferred session starts;
only a refresh failure, a refresh-expired binding, or a server rejection
tombstones the row. A tombstoned or absent binding must still start the
deferred session so the device panel's Begin command is offered. Connect on a
device-capable profile with no stored credential should lead to the device
flow, not the pasted-credential prompt. Owned paths: the application host's
device restore and renewal, the device authorization host, their focused
tests, and the RFC-BPR-0016 course inventory.

## Acceptance

- A fixture course restores a binding whose access lifetime elapsed while the
  app was closed by rotating with the refresh token, without a prompt.
- A refresh-expired or rejected binding tombstones and offers Begin.
- The tests-disabled Mac app relaunched more than eight hours after a device
  sign-in resumes the session with no credential prompt (attended).

Further observation, 2026-09-06 20:34 to 20:37 UTC: a launch restored the
session from a durable binding (viewer and capability succeeded), the process
was then ended with SIGTERM, and the next launch found the row tombstoned
although its access lifetime had hours left. So restoration is also lost
across an ordinary restart before expiry, through the shutdown or the next
launch's cleanup path; reproduce with an orderly quit and with SIGTERM and
include both in this card's acceptance.

## Fixture criteria implemented; attended criterion and one owner question pending (dsp-002)

An `rfc-worker` slice implemented this card, and main took it as
`1925d0cc231cd6d10fa709c6783b70d281d31724` and the post-review correction `3ad87ee4b93b59ab324727f3ce3890b2859ac164`. Slice evidence:
`build/r2-logs/dsp2-0644-*` and `dsp2-0644c-*`. Integrated run at
`75fab2d448fd6b4d2b092ed0385db435de446520`: `build/r2-logs/dsp2-int9-units.log`, `dsp2-int9-courses.log` and
`dsp2-int9-repeat.log`.

**Launch renewal (criterion 1).** Before this change, `restore_device_profile`
tombstoned any access-expired binding. With composition's renewal sink, a
binding whose refresh lifetime remains now holds its restore activation. The
device host renews it through the existing rotation: tombstone first, then
spend the refresh token, store the new refresh token, and enroll the new
access token. The session resumes only if `/user` names the binding's exact
account; a token for any other account is refused before it reaches the
vault. `application_device_binding_launch_renewal` passes (19.99 s) and
fails with the product change reverted.

**Refresh-expired, rejected or abandoned (criterion 2).** A refresh-expired
or tombstoned binding now starts the deferred session, so Begin is offered.
A renewal that is rejected, runs out of attempts, is abandoned (Sign out, a
preview, another enrollment) or names another account erases both slots of
its exact authority through the ordered path. No spent or rotated refresh
token stays behind the tombstone. Each case has a unit test, and removing the
guard or either erasure fails its case.

On the card's Connect sentence: since [[BPR-0771]] and [[BPR-0773]]
(RFC-BPR-0032), the account row's Sign in, and the device panel's Begin, lead
to the device flow; Connect is the separate pasted-token way in. The
composed course shows Begin reaching a new grant with no pasted token.

**Orderly quit and SIGTERM (Further observation).**
`application_device_binding_relaunch` covers an orderly quit, SIGTERM and a
real relaunch after each, and passes 3 of 3. Both phases also pass without
the change, so they are regression coverage. **They do not reproduce the
observation.** The worker's candidate mechanism was a relaunch whose Keychain
answers `locked`: for example a rebuilt, re-signed bundle
(`kSecUseAuthenticationUIFail` gives `errSecInteractionNotAllowed`), or a
`/user` network failure. Either sends the next launch's cleanup into
`fail_session(..., true)`, which tombstones and erases a live grant
(`dsp2-0644-scratch-locked-vault.log`, a reverted scratch run). Keeping the
grant through those failures would change BPR-0541's fail-closed rule, so it
was reverted, and the course's locked-vault phase now asserts fail-closed
erasure. This is the owner question below.

**Independent critical review** of `1925d0cc231cd6d10fa709c6783b70d281d31724` found no blocking defect: no false
grant, no wrong-account session, no replay yielding two grants, no secret
exposure. Its policy conflicts, the account guard and the leftover erasure
were resolved in `3ad87ee4b93b59ab324727f3ce3890b2859ac164` as described above. Recorded and not fixed:

- an offline launch after access expiry loses the grant after about three
  attempts, because the tombstone precedes the first attempt;
- a `/user` network failure right after a rotation erases the rotated pair;
- every renewal ending is reported as `authentication`;
- a failed renewal session offers no Sign out.

**Round-trip pins resealed in this commit** for the reviewed range
`7e42643f..fe629c90`. It contains:

- this card's `1925d0cc231cd6d10fa709c6783b70d281d31724`, the renewal sink in composition, critically reviewed
  above;
- [[BPR-0826]]/[[BPR-0827]]'s `d9ea1cbc`, the window session accessor and
  the liveness order, reviewed by the integrator;
- [[BPR-0823]]'s `10f30f04`, a `main.cpp` course step;
- [[BPR-0824]]'s `5875ce4f`, the ledger probe counter, reviewed by the
  Linux station's `rfc-reviewer`;
- [[BPR-0822]]'s `de528ce3`, nested types named with behaviour unchanged,
  reviewed the same way.

New values:

- whole file `6866135fef0c5412d1ed7547d64c80a3f9f2a233c24f5fe8a5ab0b90cb4b8502`;
- manifest `3b8b0ce0d34b8fdd801918b25caf8fd78c3a1bc9c74781550a1ee1143cf1dc01`;
- mutation branch unchanged.

The round-trip and composition contracts pass with them.

**Pending, human.**

1. Criterion 3 (attended): the tests-disabled Mac app, relaunched more than
   8 hours after a device sign-in, resumes the session with no credential
   prompt.
2. Owner question: after a restart, should a remembered device grant survive
   a launch whose Keychain answers locked or denied, or whose network is down
   (keep the material, fail visibly, retry at the next launch)? Today such a
   launch erases it, as BPR-0541's fail-closed rule says. The same question
   covers an offline launch after the access token expired.

## Attended preparation at main `94f6f885` (dsp-003)

A tests-disabled bundle was built from `94f6f885`, signed with the stable
Developer ID identity, and passed `codesign --verify --deep --strict`
(`BUILD_TESTING=OFF`). It is at
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-dsp3/src/app/BetterPullReview.app`,
with executable SHA-256
`96fa16fea8983347303c6ce18095f147871b7d45ef51537d48097ac6004e1bfa`. A
`--self-test` run printed `AUTOMATED_PROFILE_PRIVATE_OK` and did not touch
the Default profile.

The BPR-0794/0795 focused set was rerun at that source and passed 13 of 13
(`build/r2-logs/dsp3-bundle-focused-ctest.log`).
`application_epoch_portable_occurrence_two_catalogues` also passed
(`dsp3-bundle-portable-ctest.log`).

[the rfc-bpr-0013-attended-v001 packet](../../verification/reviews/rfc-bpr-0013-attended-v001/operator-packet.md) holds three ready, protected questions in the checkout's attended-review
session: `BPR-0794-cli-selected-diff`, `BPR-0795-loading-visible` and
`BPR-0644-device-relaunch-8h`. The human part is open. The owner must quit
their own running BetterPullReview before launching the bundle.
