---
schema: "implement-rfc.card.v1"
id: "BPR-0836"
title: "The designated-live capsule image build can leave a write-once device attached"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"found_by":"dsp-003 BPR-0814 slice","found_at":"c1a795c3ed724eb0086aa4661082793f9643a679","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 after a production image build, hdiutil info lists no attachment of that image, including when concurrent creates run","outcome":"pass","revision":"8ad33698","evidence":{"kind":"command","detail":"Concurrent production image action through the CMake digest-checking preloader: unfixed control leaked (2/24 with 3 workers, 13/30 with 6, each printing a digest while attached); fixed 24/24, 30/30, 36/36 traced (7 real leaks detached without force), none attached at return (build/r2-logs/dsp4-0836-unfixed-concurrent.log, dsp4-0836-fixed-concurrent*.log). The corrections at a6b9d390/8ad33698 do not change the detach loop; critical review and narrow recheck found no blocker."}},{"criterion":"AC2 the capsule suite and the structural gates pass","outcome":"pass","revision":"8ad33698","evidence":{"kind":"command","detail":"On main at 8ad33698: architecture_boundaries, architecture_boundary_contract, test_network_isolation_structure_contract, designated_live_macos_capsule_contract and test_network_isolation_manifest 5/5 (build/r2-logs/dsp4-int2-ctest.log); serial capsule suite 49/49 (dsp4-0836-fix2-serial.log); documentation gate OK; hdiutil info afterwards lists only the two system simulator images."}}]
---

# BPR-0836: The designated-live capsule image build can leave a write-once device attached

## Defect

[[BPR-0814]] found that `hdiutil create`, while other image creates run on
the machine, sometimes exits 0 with its temporary write-once device still
attached, and the device stays attached (one was still there after 9.5
minutes). The image bytes are final. A later attach of that image fails at
once, with empty stderr under `-quiet`
(`build/r2-logs/dsp3-0814-probe-unfixed.log`,
`dsp3-0814-leftover-anatomy.log`, `dsp3-0814-leak-then-attach.log`).

BPR-0814 made the test fixture detach its own image after each create. The
production image command in `scripts/designated_live_macos_capsule.py`
runs the same `hdiutil create` and has the same exposure: it can print a
correct digest, exit 0, and leave a device attached, after which stage zero's
attach fails closed. That script's bytes are pinned by the structural gates.

## Scope

- Decide whether the production build should verify that its own image is
  not attached after create, and detach it (never forced, matched by exact
  image path), or report the leak as a refusal.
- Treat the change as security-relevant: update every pin the gates require
  and keep it minimal.

## Acceptance

- After a production image build, `hdiutil info` lists no attachment of
  that image, including when concurrent creates run.
- The capsule suite and the structural gates pass.

## Fix (dsp-004, Mac)

**Where it was built.** The work was prepared on the scratch worktree
`BetterPullReview.dsp4-worktrees/bpr-0836`, where it was reviewed, then
cherry-picked onto main after the claim: `1774ae80` became `e015c1c8`, and
`a6b9d390` became `8ad33698`.

**Behaviour.** `build_image` in `scripts/designated_live_macos_capsule.py`
now calls `detach_created_image(arguments.output, arguments.hdiutil)` right
after `hdiutil create`, even when the create fails. The call comes before the
`chmod`, the hashing, the digest file and the digest line.

The check lists `hdiutil info -plist` and keeps only attachments whose
resolved image path equals the output's. On each attempt it ordinarily
detaches one whole disk of that image, never with force, and lists again
before the next attempt, for up to 60 seconds.

The build fails, reporting no digest, when:

- the image is still attached after that bound;
- the listing cannot be read;
- the image is attached with no whole disk.

When the create itself failed, the refusal names that failure first.

**Gate pin.** `cmake/VerifyArchitectureBoundaries.cmake` now requires exactly
one such call, placed after `def build_image(` and before
`digest = image_sha256(arguments.output)`. `architecture_boundary_tests.py`
covers the call being removed, moved after the digest and repeated. No
literal pin needed changing, because the gates compute the script's digest.

**Evidence.**

- **Concurrent production builds, before the fix:** 2 of 24 (3 workers) and
  13 of 30 (6 workers) exited 0 and printed a digest while the image was still
  attached.
- **Concurrent production builds, after the fix:** 24/24, 30/30 and 36/36
  (traced, repairing 7 real leaks), with no attachment left
  (`build/r2-logs/dsp4-0836-{unfixed,fixed}-concurrent*.log`).
- **Test suites:** 3 concurrent suites x 5 rounds pass 15/15. The serial suite
  passes 49/49 after the corrections (`dsp4-0836-fix2-serial.log`).
- **Mutations:** reporting the digest before the detach, or returning quietly
  at the bound, is caught (`dsp4-0836-mutations.log`). Removing the gate
  fragment or moving it after the digest fails the gate
  (`dsp4-0836-fix2-gate-negative.log`).
- **Gates on main at `8ad33698`:** `architecture_boundaries`,
  `architecture_boundary_contract`, `test_network_isolation_structure_contract`,
  `designated_live_macos_capsule_contract` and
  `test_network_isolation_manifest` pass 5/5 (`dsp4-int2-ctest.log`).
  Afterwards `hdiutil info` lists only the two system simulator images. The
  documentation gate passes.

**Critical review.** It found no blocking issue, and a narrow recheck
confirmed the three corrections. Findings recorded as follow-up candidates,
not changed:

- a device number reused between the listing and the detach needs an outside
  actor, and `hdiutil` cannot detach by image entry;
- a case-variant attachment by a third party goes undetected, which is out of
  this card's scope;
- no timeout on `hdiutil info` or `detach`, the same as the script's other
  calls;
- the gate's fragment check is textual, like the gate's other checks;
- a possible transient entry with no `dev-entry` would refuse, not leak; it
  was not observed in 90 runs.

**Invalidates:** any operator packet or candidate bound to the old script
digest `b969b90a…`, such as the v011 packet, needs a rebuild before its next
use.
