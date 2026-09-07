---
schema: "implement-rfc.card.v1"
id: "BPR-0667"
title: "The epoch refuses any pull request whose base branch has moved"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P0"
parked_on: "human-review"
acceptance: [{"criterion":"A pull request whose base branch has advanced, and a merged one whose head branch is deleted, both reach an exact comparison","outcome":"pass","revision":"e61b6c25","evidence":{"kind":"observation","detail":"The public-path fixture now advertises a moved base branch, as a real remote does, and eighteen public-path, epoch, public-preview, defect and overview courses pass over both fixtures, including application_github_public_path_github_com, application_epoch_persistence_github-com and application_epoch_maintenance_public_path. Restoring the equality check fails the github-com course with PUBLIC_PATH_FAILED, which is the owner's empty workbench reproduced. The live shape was proven against microsoft/terminal#20607 before the change: recorded base c7572cde, refs/heads/main now 093e49e2, refs/pull/20607/head 472eb71d unchanged, and github.com serves a want for that recorded base with HTTP 200 and a packfile."}},{"criterion":"A response that advertises or delivers an object other than the wanted oids is still refused","outcome":"pass","revision":"e61b6c25","evidence":{"kind":"observation","detail":"application_git_protocol_v2 pins that a pull request head advertised at another oid is refused as oid_mismatch, and the fetch still sends want for both recorded oids while git_prepare_revision_request prepares the revision against those same two, so a substituted object is refused downstream. native_git_smart_transport_contract and native_git_smart_transport_fixture pass unchanged."}},{"criterion":"Observed in the signed bundle: a real public pull request reaches an exact comparison with protocol_acquire succeeding","outcome":"pending","revision":"e61b6c25","evidence":{"kind":"freeform","detail":"Owed, and it is the same run that answers BPR-0663 and BPR-0662. This session must not drive the app's interface, so the observation waits for the owner; the attended packet that would have carried it is itself defective (BPR-0672) and must be repaired before another session is offered."}}]
---

# The epoch refuses any pull request whose base branch has moved

The owner opened `microsoft/terminal#20607` through Public preview in the
signed bundle on 2026-09-07 at 17:47. The pull request loaded and the review
history did not. The diagnostics log:

```
epoch_stage=smart_acquire    outcome=succeeded
epoch_stage=protocol_acquire outcome=failed failure_domain=protocol failure=oid_mismatch
epoch_stage=revision_prepare outcome=failed failure_domain=epoch    failure=unavailable
```

`read_git_protocol_v2_ls_refs_response` asks the remote for the pull request's
base ref and for `refs/pull/<n>/head`, and refuses with `oid_mismatch` unless
the advertised oid of each equals the oid the pull request recorded. The head
holds: `refs/pull/20607/head` is `472eb71d08b9331ae6f43e5d0e7e6c2fc44fe6f9`,
which is the head the API reports, and it never moves. The base does not: the
pull request recorded base `c7572cde0c69733e4511787dc963eb336f17adbf`, and
`refs/heads/main` on that repository now points at
`093e49e29a9f806ff83025c49be5d0c970673b00`.

A base branch moves every time anything else merges. This is not a merged-pull-request
case or an edge: on any repository with more than one contributor, the review
history is unavailable for nearly every pull request, within minutes of it
being opened. It is why the workbench the owner reached was empty.

## Scope

- The advertisement says what the remote has now; it is not a promise about
  what the pull request recorded. Stop requiring the base ref's advertised oid
  to equal the recorded base.
- Keep the guarantee the refusal exists for, at the place that can give it:
  the fetch names the exact base and head oids it wants, and the objects
  delivered are verified to be those oids. A server that substitutes an object
  is still refused.
- Decide what the base ref advertisement is still needed for, if anything, and
  say so where the code reads.
- A composed check against a recorded pull request whose base branch has moved
  since, so the case cannot silently return, and a negative that still refuses
  a substituted object.
- Owned paths: `src/application/src/git_protocol_v2.cpp` and its header, the
  epoch host's acquire, their tests.

## Acceptance

- A pull request whose base branch has advanced since it was opened, and one
  that has been merged and had its head branch deleted, both reach an exact
  comparison; the log records `protocol_acquire` succeeding.
- A response that advertises or delivers an object other than the wanted oids
  is still refused, and a test proves it.

## Notes (2026-09-07)

- **What it was.** `parse_git_protocol_v2_ls_refs` required the base ref's
  advertised oid to equal the oid the pull request recorded, and refused
  `oid_mismatch` otherwise. The advertisement says where the branch is now;
  the recorded base is where the pull request started. They are equal only
  until the next merge on that branch. Proven live against
  `microsoft/terminal#20607`: recorded base `c7572cde`, `refs/heads/main`
  now `093e49e2`, `refs/pull/20607/head` `472eb71d` matching the recorded
  head exactly.
- **What replaced it.** The base advertisement is recorded, not judged. The
  head advertisement is still required to be the recorded head, because a
  pull request's own ref never moves and another oid there is a different
  pull request. The guarantee the refusal existed for is kept where it can
  be given: the fetch already sends `want <base>` and `want <head>` for the
  exact recorded oids, and `git_prepare_revision_request` prepares the
  revision against those same two oids, so a server that substitutes an
  object is still refused. A live probe against
  `github.com/microsoft/terminal/git-upload-pack` confirmed the remote
  serves a want for a recorded base that is no longer a branch tip: HTTP
  200 with a packfile.
- **Why no course caught it.** The public-path fixture's `refs_body`
  advertised the base at `aContext.Revision.Base`, the very oid the client
  was about to demand. Every composed course therefore agreed with the
  defect. The fixture now advertises a moved base branch, as a real remote
  does, so the case the product meets is the case the courses run.
- **Evidence.** `application_git_protocol_v2` (a moved base branch is
  reported where it is; a pull request head advertised at another oid is
  refused), `native_git_smart_transport_contract`,
  `native_git_smart_transport_fixture`, `git_anonymous_public_read`,
  `application_review_round_trip_composition_contract`, and eighteen
  public-path, epoch, public-preview, defect and overview courses over both
  fixtures. `check_boundaries`, strict tidy on the touched translation
  unit, `clang-format --Werror` and `verify_documentation.py` pass.
- **Negative mutation.** Restoring the equality check with the corrected
  fixture fails `application_github_public_path_github_com` with
  `PUBLIC_PATH_FAILED`, which is the owner's empty workbench reproduced in
  a course.
- **Not covered.** The observation in the signed bundle is owed: a real
  pull request opened through Public preview, reaching an exact comparison,
  with `protocol_acquire` succeeding in the diagnostics log. It is queued
  with the rest of the attended work, and it is the same run that answers
  [[BPR-0663]].
