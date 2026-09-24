---
schema: "implement-rfc.card.v1"
id: "BPR-0644"
title: "Renew expired device grants at launch instead of forgetting them"
status: "Parked"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P1"
metadata: {"owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-0644-grant-policy-v001","decision":"3c46d164-e405-4c6c-bcd3-2f3b0c3b757c"},"supersedes_for_these_faults":"BPR-0541 fail-closed erasure","related_cards":["BPR-0541","BPR-0884","BPR-1048"]}
acceptance: [{"criterion":"A fixture course restores a binding whose access lifetime elapsed while the app was closed by rotating with the refresh token, without a prompt","outcome":"pass","revision":"75fab2d448fd6b4d2b092ed0385db435de446520","evidence":{"kind":"command","detail":"application_device_binding_launch_renewal passes at the integrated head (19.99 s, build/r2-logs/dsp2-int9-courses.log) and fails with the product change reverted (dsp2-0644-negative-control.log). The rotated grant resumes only the binding's exact account; the account-guard unit case fails without the guard (dsp2-0644c-control-*.log). Rerun at 1347f776: application_device_binding_launch_renewal passes (36 s) with its criterion 1 phase unchanged."}},{"criterion":"A refresh-expired or rejected binding offers Begin. Amended 2026-09-24 by the owner's rule that a remembered grant is removed only when the person removes it in Settings: such a binding is no longer erased on its own; it stays, says that it needs signing in again, and offers Begin until the person removes it or a new sign-in replaces it","outcome":"pass","revision":"1347f776","evidence":{"kind":"command","detail":"Built at 1347f776. A refresh-expired binding is held and failed at once as authentication, spending nothing; a refused refresh is kept the same way. Both keep both slots and a live row, publish GrantKept, and the Settings card says it needs signing in again, offers Begin and Sign out. application_device_binding_launch_renewal: refresh-expired kept at two launches, Sign out on the card erases both slots, the next launch is signed out; a refused renewal kept at two launches, then Begin replaces it with a fresh grant. application_github_host covers refused, refresh-expired and the backward/expired lifetime, each kept and removed by Sign out. No security reason to erase a refused, spent or expired refresh token was found (recorded on the card); the one exception kept, material naming another account, is the owner question.","path":"docs/issues/40-parked/BPR-0644-renew-expired-device-grants-at-launch.md#the-keep-grant-policy-built-mac-batch-7-2026-09-24"}},{"criterion":"Restoration across an orderly quit and SIGTERM (Further observation)","outcome":"pass","revision":"1347f776","evidence":{"kind":"command","detail":"application_device_binding_relaunch at 1347f776 (10.9 s): real processes over one data root run seed, orderly quit, quit, SIGTERM, verify, then a locked-keychain launch that keeps the grant (no delete attempts, both slots retained, live row, kept reason vault_unavailable) and a relaunch after it that restores the session. The recorded mechanism of the observation (a locked Keychain or a /user network failure sending restore cleanup into erasure) is what the owner's policy removes; with the old delete condition restored the locked phase fails at 'a locked vault erases neither slot of the restored grant'."}},{"criterion":"The tests-disabled Mac app relaunched more than eight hours after a device sign-in resumes the session with no credential prompt (attended)","outcome":"pass","revision":"7b591faae6c1a746998ade8b49f87d88145fcf6b","evidence":{"kind":"observation","detail":"Fingerprint-valid owner accept at attended session cbd48b91000b00f1e72e1ad62c7c384c event 405, 2026-09-20T03:52:48.224084+00:00; candidate rfc0013-r17-7b591faa-ad50fe5d, fingerprint a7aeb7b9c836181406cf1b45c8ee0465c67612639a5759345c149724ebc90cbb. Accepted tests-disabled r17 relaunch criterion only; preserved through r18, not rebound to its new diff/loading bundle. Other pending restoration observation unchanged."}},{"criterion":"Owner policy, 2026-09-24: a launch whose Keychain answers locked or denied, or whose network is down, including an offline launch after the access token expired, keeps the remembered device grant, shows the failure, and retries at the next launch; the grant's material is removed only when the person removes it in Settings","outcome":"pending","revision":"1347f776","evidence":{"kind":"command","detail":"Built at 1347f776 for every fault the criterion names: locked, denied, missing or unreadable access slot, network, /user refusal, an offline launch after access expiry (the launch renewal no longer tombstones before its first attempt), refresh-expired and refused refresh. Each fails visibly, keeps both slots and a live row, and the next launch retries; Sign out or a new sign-in removes it. Negative controls: the old delete condition fails application_github_host, the renewal course and the relaunch course; the launch tombstone restored fails application_github_host and the renewal course at 'the next launch, online, renews the kept grant'. Pending on one owner question, prepared on the card and not asked: material that signs in as a different account than the remembered one is still erased at once (security reason recorded); keep that exception or keep even that until Sign out? The in-process renewal's erasure is carded as BPR-1116.","path":"docs/issues/40-parked/BPR-0644-renew-expired-device-grants-at-launch.md#the-one-automatic-erasure-left-and-why-the-owners-question"}}]
parked_on: "human-review"
---

# Renew expired device grants at launch instead of forgetting them

Current attended pointer, 2026-09-20: the owner accepted the device-relaunch
candidate `rfc0013-r17-7b591faa-ad50fe5d`, fingerprint
`a7aeb7b9c836181406cf1b45c8ee0465c67612639a5759345c149724ebc90cbb`, at
`2026-09-20T03:52:48.224084+00:00` (session event 405). That verdict remains
bound to its r17 source and bundle; it is not reinitialized or re-asked by the
[v005 attended-review packet](../../verification/reviews/rfc-bpr-0013-attended-v005/operator-packet.md),
which refreshes only the RFC-0013 source/anchor question and retains the positive
v004 loading observation. The separate
restoration/fail-closed policy observation below remains open; this card is not
closed by unrelated RFC-0013 repairs.

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

## Owner decision, 2026-09-24: keep the grant (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0644-grant-policy-v001`, revision
`75fab2d4`, fingerprint `0416486e…` equal to the prepared candidate's,
decision `3c46d164-e405-4c6c-bcd3-2f3b0c3b757c`, sequence 567,
2026-09-24T18:39:17Z, `actor: human`. The question was item 2 of "Pending,
human" above. The owner chose **select**, verbatim:

> Keep the material: fail visibly and retry at the next launch

Note, verbatim:

> Shouldn't remove until the user removes it in the settings menu.

**What changes.** For the faults the question named, this replaces
[[BPR-0541]]'s fail-closed erasure. A launch whose Keychain answers locked or
denied, or whose network is down, keeps the remembered device grant. The
same holds for an offline launch after the access token expired. The launch
says what failed and retries at the next launch. The note states the rule
behind that: **a remembered grant is removed only when the person removes it
in Settings.** A new sign-in that replaces the grant is also the person's own
action.

**How it lands on this card's criteria.**

- The new policy criterion (last in the acceptance list) is pending. The
  course's locked-vault phase, which asserts erasure today, has to assert
  retention, a visible failure and the retry instead. The critical review's
  "an offline launch after access expiry loses the grant after about three
  attempts, because the tombstone precedes the first attempt" is part of this
  work, not a separate limit.
- Criterion 2 is amended and pending. Read literally, the note also covers a
  refresh-expired or server-rejected grant. Such a binding no longer erases
  itself. It stays, says that it needs signing in again, and offers Begin
  until the person removes it or a new sign-in replaces it. If the
  engineering round finds a security reason that a rejected or spent refresh
  token must still be erased, it records the reason and asks the owner that
  one question. It does not read the answer into this decision.
- Criterion 3, the restoration observation, is now answerable. The
  mechanism recorded for it (a locked Keychain or a `/user` network failure
  sending cleanup into erasure) is exactly what the policy removes. Rerun the
  quit and SIGTERM relaunch course with those faults once the policy is
  built.
- Criteria 1 and 4 stand as recorded.

[[BPR-0884]]'s store change works with this answer: a store that no longer
latches is what lets an in-process retry reach the database. The card leaves
human-review for **Ready**. The policy is engineering now, and nothing on it
waits for the owner. Owned paths are unchanged: the application host's device
restore and renewal, the device authorization host, their tests, and the
architecture note's device-binding section.

## The keep-grant policy, built (Mac batch 7, 2026-09-24)

The owner's rule (decision `3c46d164`) is now the launch's behaviour: **a
remembered grant is removed only when the person removes it (Sign out) or a
new sign-in replaces it.** A launch that cannot restore it fails visibly with
its own reason, erases nothing, leaves the row live, and the next launch tries
again.

### What changed

- **A restored grant's failure no longer erases it.** `fail_session` used to
  delete any restored device binding whose session failed before acceptance
  (BPR-0541's fail-closed rule). Now it deletes a provisional credential, or
  a persistent one only when its caller asks. The access-slot branch for a
  restored binding (locked, denied, missing or unreadable) fails with `vault`
  and keeps the grant. So do a network failure and a `/user` authentication
  refusal. The credential stays selected, so **Sign out erases both slots
  through the ordered path**, and an orderly quit keeps it
  (`DeviceBindingDurable` stays true).
- **The tombstone before the first attempt is gone.** `prepare_device_rotation`
  answers a launch renewal's barrier with `true` and writes nothing. The row
  stays live until the rotated grant's binding replaces it. Before, an offline
  launch lost the grant after three attempts, and so did a launch quit while
  it retried: the row was already tombstoned. An accepted session's in-process
  rotation keeps its barrier; see [[BPR-1116]].
- **A renewal that cannot finish keeps the grant.** The enrollment port's
  `invalidate_grant` now carries the device host's failure. The GitHub host
  maps it (`transport_failed` to `network`; `vault_unavailable` and
  `storage_failed` to `vault`; `refresh_rejected`, `refresh_absent` and `none`
  to `authentication`). `end_launch_renewal` releases the device host's chain,
  selects the renewal's slot as the kept grant, and fails the held session
  with that reason. A rotated grant whose own `/user` fails, before or after
  it names the account, is kept the same way (`keep_renewed_launch_grant`).
- **Criterion 2: refresh-expired and rejected grants stay.** A refresh-expired
  binding (or one with no renewal composed) is no longer tombstoned at
  restore. It is held and failed at once with `authentication`, spending
  nothing. A refused refresh is kept the same way. Both say they need signing
  in again, offer Begin, and stay until the person signs out or a new sign-in
  replaces them. The next launch meets the refusal again and says so again.
- **The failure says the grant was kept.** `github_host_diagnostic_projection`
  gains `GrantKept`. `github_session_panel` keeps the kept reason for the
  failed session. The server card (`kept_grant`) then offers **Sign out**
  beside Sign in and says how the grant ends:
  - network or credential store: *"Your sign-in is still remembered on this
    Mac and is tried again at the next launch. Sign out forgets it."*
  - refused or expired: *"Your sign-in on this Mac needs signing in again. It
    stays remembered until you sign in or sign out."*
  Captures: [kept-grant-offline.png](../../verification/BPR-0644-kept-grant/kept-grant-offline.png)
  and [kept-grant-needs-sign-in.png](../../verification/BPR-0644-kept-grant/kept-grant-needs-sign-in.png),
  the composed Settings card as `application_device_binding_launch_renewal`
  draws it (`BPR_KEPT_GRANT_CAPTURE_DIR`). The composed Settings window takes
  the application palette offscreen, so these captures are light only.
- Composition forwards the new failure argument in its two enrollment
  wrappers. That is the only `src/app` product change, and the round-trip pins
  are resealed with a `BPR-0644 reseal` note: full
  `fcf7b4bb564b51655cfe81b4b49b50b1d6e437cf389d39bd39e339495cfe29cb`,
  manifest `2edb8d2de16ceb8a19358011113c8067718a92100f81921d9d8876e885f5da9d`,
  mutation branch unchanged. The fixture gains `OfflineDeviceRefresh`.
- The architecture notes ([github-host-implementation](../../architecture/github-host-implementation.md),
  [security-threat-model](../../architecture/security-threat-model.md)) state the rule
  and its one exception.

### The one automatic erasure left, and why (the owner's question)

The card asked for this: if the round finds a security reason to erase
anyway, record the reason and ask. **No such reason was found for a refused,
spent or expired refresh token.** Such material is dead. Keeping it lets no
one sign in, and presenting it again costs one refused request. The old
tombstone was a crash-consistency marker, not a security control. Under the
owner's rule a crash after the spend now leaves a live row, and the next
launch spends whatever the refresh slot holds: the rotated token if its store
landed, or the dead one, which asks for signing in again.

**One case still erases on its own:** material that signs in as an account
the binding never named. Two paths reach it: a launch renewal's rotated grant
whose `/user` answers another account, and a restored access slot whose
`/user` answers another account. The host refuses the session in both, as
before. It also erases both slots behind a tombstone. The reason: the slot
would otherwise keep a live credential for someone else, under a key that
names the person's own account. Nothing would ever use it, but it would stay
on the machine. The two profile-mismatch erasures in `handle_profile_now`
(the compiled profile changed under a binding) are unchanged too. Restore
validation refuses such a binding before it gets that far.

**Ask (one question, prepared for a later session, not asked):** *When the
material in a remembered sign-in turns out to sign in as a different GitHub
account than the one remembered, should BetterPullReview (a) keep erasing it
at once, as the one automatic removal, or (b) keep it until you sign out, like
every other failure?*

### Verification (macOS 26.6.2 arm64, `build/dev`)

- `application_github_host`:
  - `test_restored_binding_launch_failures_keep_the_grant` (renamed from
    `..._stay_fail_closed`): locked, denied, network and authentication each
    keep both slots and a live row, publish `GrantKept` with their own
    reason, and keep the grant through an orderly quit. A fresh host over the
    same store retries and presents it to `/user`. Sign out on the failed
    session erases both slots behind a tombstone.
  - `test_restore_device_profile_renews_an_access_expired_grant_before_its_session`:
    the launch barrier writes no tombstone, and eight endings are covered.
    Network, refused, refresh-expired and the rotated grant's offline `/user`
    are kept, then removed by Sign out. Enrollment succeeds. Another account
    still erases. Sign out and a replacing token erase as before.
  - The missing-access and backward/expired-lifetime cases now keep.
- `application_device_authorization_refresh`: the refused and refresh-expired
  renewals report `refresh_rejected`. A new offline case reports
  `transport_failed` after the bounded retries.
- `application_device_binding_launch_renewal` (36 s):
  - an offline launch after access expiry keeps the grant, and the next
    launch online renews it (rotation 2);
  - refresh-expired is kept at two launches, then Sign out on the card erases
    both slots, and the following launch is signed out;
  - a refused renewal is kept at two launches, then Begin replaces it with a
    fresh grant.
- `application_device_binding_relaunch` (the quit and SIGTERM course, rerun
  with the fault): seed, quit, quit, SIGTERM, verify, then **locked keeps the
  grant** (no delete attempts, both slots retained, live row, `vault_unavailable`
  kept reason), and **the relaunch after it restores the session**.
- **Negative controls.** With the old delete condition restored in
  `fail_session`, three rows fail: `application_github_host` at "an offline
  launch renewal keeps the grant", the renewal course at "an offline launch
  keeps the grant", and the relaunch course at "a locked vault erases neither
  slot". With the launch tombstone restored in `prepare_device_rotation`, two
  fail: `application_github_host` at "writes no tombstone" and the renewal
  course at "the next launch, online, renews the kept grant". That second
  failure is the critical review's finding. Restored, all pass.
- Also passing: the regression set (`device|github_host|composed_github|session_panel|composition_contract|restart|relaunch|two_account|sign_in|account|credential|vault|core_github`),
  56 of 59. It includes `application_composition_contract` (231.7 s) and
  `application_review_round_trip_composition_contract` with the new pins.
  The three failures are `qt_anonymous_public_preview_scale_100/125/200` at
  the pre-existing [[BPR-1023]] assertion.
- clang-format 23.1.1 passes every touched file. The project tidy script
  reports nothing on touched lines. Its four findings are pre-existing:
  `home_view_state.hpp:35` and untouched `std::endl` lines in the two courses.

### Residual, carded

[[BPR-1116]]: while the application runs, an accepted session's renewal that
gives up still deactivates and erases the grant. The in-process rotation still
tombstones before it spends. A restored session whose refresh slot is missing
is still signed out and erased. All three follow from the owner's rule but are
outside this card's launch scope.

## Linux at `45191097` (bpr-main-linux, 2026-09-24)

**The keep-grant policy's courses pass on Linux, and so do the pins
`1347f776` resealed.** No macOS or Windows run was made from this station.

The run was in the station's main checkout, with a clean tree, at
`45191097`, which contains `1347f776`. WSL2 Ubuntu 26.04, GCC 15.2, Qt
6.11.2, `build/dev` (Debug, `-Werror`), rebuilt at `9ee4ee50` with no
`warning:` line. `45191097` changes nothing under `src/`, `tests/` or
`cmake/` from it. `ctest --test-dir build/dev -j1 --output-on-failure`,
offscreen, with `DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS`
unset and fresh absolute `HOME` and XDG roots. The courses use the
fixture's synthetic vault, not the Secret Service.

| Row | Result |
| --- | --- |
| `application_github_host` | pass, 0.08 s |
| `application_device_authorization_refresh` | pass, 0.11 s |
| `application_device_binding_launch_renewal` | pass, 36.44 s |
| `application_device_binding_relaunch` | pass, 8.11 s; a verbose rerun prints `DEVICE_BINDING_RELAUNCH_OK` |
| `application_review_round_trip_composition_contract` | pass, 1.67 s |
| `application_composition_contract` | pass, 471.22 s |

The round-trip contract's pass shows that the pins resealed with the
`BPR-0644 reseal` note match the Linux checkout. The runs added nothing to
`/tmp` and no `bpr-` name to `/run/user/1000`. The negative controls were not
repeated here; they are the macOS runs above.

The acceptance record above is unchanged, and the card stays parked on its
owner question. The logs are station records under
`build/logs/bpr-main-linux-20260924/batch7-linux/` and are not committed:
`bpr-0644-rows.log` and `bpr-0644-relaunch-verbose.log`.
