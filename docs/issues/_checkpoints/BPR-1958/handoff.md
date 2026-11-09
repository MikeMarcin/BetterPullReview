# BPR-1958 engineering handback

Assignment `/root/windows_native_diagnosis`, mandate
`owner-release-blockers-20261109-r1`; adopted plan
`bpr-1958-scaling-readiness`, milestone `settle-scaling-review`.

Source `e5e7da456` is integrated and pushed through main
`b3a45f3a292fb847dd97f5822d7bcf6ecfb945ee`. The sole behavioral change is
`tests/end_to_end/application_scaling_tests.cpp` (8 insertions, 3 deletions).
Current main's other implementer fixes and allocation counter are preserved.

## Exact engineering candidate

Frozen backend source: `97db680a3337f69e578fedab95825e84e3c72a33`.
Cohort: `29ad923f2fbe24cd2b9a354b0394494a64c97a66cc55c23dfd322094c5560c75`.
The full source export overlays only the test file; compiler/link definitions
match the generated producer project after owned-path normalization. The
export SHA is `65bcaa8afb57992bb9c2a52c1d40ba76d300e6f1686255f2d782ce00e1b302b7`.

- Test file: `dc422eb7c51aa0a3e336eccf0aee0dbce7379d17934b766a29ff793b81357f1b`.
- Corrected image: `726ed76de19cbc844f20cae3e90f9039dd2b49f8ccdac13d94daffe9099a31cb`.
- Negative image: `f1f95c7fa618a2fac2b03a438102efd81e578eb1947072c6da4d79cf5931ff78`.

The unchanged quiet baseline passes (24.62 s); corrected full course passes
(26.52 s); disabling Review produces the named refusal (14.06 s, deliberate
CTest 8). Original 180 s watchdog, fixture size, oracles, environment and
network registration remain. Windows 11 x64 / Qt 6.12.0 / offscreen / native
Limited Interactive session 3. All 201 protected files, including 190 linked
inputs, retain their hashes. This is engineering proof against frozen43,
not current-main full native or release qualification.

## Separate native artifact recovery

Nine Debug BAD_COMMAND rows used five cached EXEs with zero PE headers.
Windows creation refused representative images with error 216. Every target's
link read/write tracker contained only its two-byte UTF-16 BOM. Moving only
the EXEs did not force relinking; the normal build returned 0 without outputs.
After quarantining exactly the fifteen link command/read/write trackers, the
same normal five-target build passes (72.82 s), all five PE headers are valid
AMD64, and all nine original courses pass serially (350.03 s). Source/cache/
project hashes and the other 1,105 EXE metadata records remain unchanged.
The original corruption trigger remains unproved.

Before repair, 277 source/config/project/PDB/ILK/object/tracker/raw-log files
were archived and every member and original hash verified. The archive is
`D:/BetterPullReview-release-v042-r1/state/windows-native-diagnosis-r1/rejected43-archive/rejected43-native-inputs.tar`,
4,073,830,400 bytes, SHA
`0880bc748a894aa3d62f3095343aad5028a49bafc875b08597b1efc4cd29e21c`.
Old EXEs and trackers remain in the sibling `quarantine-r2` and
`link-trackers-r3` evidence roots. Original Release 21 / Debug 37 failures
remain retained; repaired engineering rows do not rewrite that qualification.

## Verification, ownership and continuation

Format 22, documentation structure, 67 documentation contract tests, scoped
board validation and explicit own-plan binding pass. The generic checkpoint
recovery scan rejects an unrelated historical plan's `amendments` field;
that raw refusal is retained without changing history or claiming helper
acceptance. The explicit plan binding and this handback cover every criterion.

All task-owned native commands ended; every observed forest is empty. Eight
scheduled diagnostic tasks were removed by exact name/action/principal.
The negative temporary root was removed after creation-interval, no-reparse,
alternate and linked-worktree checks. Only external evidence/candidate roots
and the private implementer worktree remain intentionally retained. The
initial contended reproduction's failed fixture is retained as diagnostic
material; no unrelated qualification temporary root was removed.

External evidence is under
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate43/windows-native-diagnosis-r1`.
There is no attended-review or independent human criterion for this test-only
correction, and no review server is running. Remaining startup/Review/shutdown,
optimized Release budget and security/native/trust gaps stay open. The next
coordinator-authorized action is a current-main private invocation/canary build
and actual native-session contained measurement using the adopted wrapper;
old43 images cannot qualify that new registration.
