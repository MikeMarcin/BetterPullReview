# BPR-2050 clipped reference ink rows

Mandate release55-sparse-reference-mask-r1, RFC-BPR-0038. Frozen55source
f42e747cf9c067040052a51c6bccca9188ffef91/cohort3224f51bc0860489d7f9b8c5d652cc046165c6bb4d8738f333f18c3612a78bdf
is unchanged. Only tests/adapters_qt/diff_workbench_tests.cpp changes.

## Cause and distinguishing evidence

Original normal Linux55earlyfocus70/72passed;100failed4.00s and125failed5.88s
the painted-number span assertion,200passed15.16s. A single approved private
failure-only diagnostic reproduced both failures under original120s/containment.
It saved already captured frames only after the original predicate was false.

The reference32mask has sparse rows. At100%, referencey24has2qualifying pixels,
y25..31have0,y32has4. Shift-8andclip18leave onlyy24visible, exactly matching
current24..24; clamping the bounding box instead invents firstink18. At125%,
referencey30has1pixel,y31..39have0,y40has4. Shift-10andclip23leave30..30,
exactly matching current30..30, while bboxclamp falsely expects23..30.
Native and production row bounds/baselines agree. This is a fixture oracle
assumption, with no product or Qt defect inferred and no threshold waiver.

## Correction

Private source9a29c6a769b26a7981ad6bdd24339ee1804a280d, main source
96d8d57724ddc0ec4ce24ce326f419fd790adf02, TU SHA256
`602d98cfd963d6e796094d497cbeb1725dc902a47270f4beafe4ec97a619b77c`.
Retain qualifying reference y rows during the existing scan. Translate each
with the existing native displacement/qRound and clip to the same heading/image
bounds before computing expected first/last. Both color classifiers and the
one-device-pixel predicate are byte-identical. All590current/575original named
assertions, original skips, captures, gestures and clipped-row source picking
remain. Unexpected empty visible reference ink fails closed.

Same-code controls cover the two observed sparse masks, dense/unclipped masks,
fractional translation, empty/fully clipped masks, missing sole visible ink,
two-pixel displacement/loss rejection and unchanged +/-1allowance. Original55
and diagnostic failures supply the old-oracle negative; none was rerun to green.

## Exact-source engineering verification

Linux: one private TU compile0/16.030s and link0/.524s. Actual normal compiler
/usr/bin/c++ resolves /usr/bin/x86_64-linux-gnu-g++-15, Ubuntu15.2.0-16ubuntu1;
SHA256e6718f7e0c7d057c3ff77b550c603da9bc4030e3ede3c053705acce1293dbe4d
matches cohort. Pure preflight verified complete helper coverage, exact reverse
compile/module/link tokens and three configured rows. Only privateEXE and the
strictly output-only PNG namespace differ; original working directory,
environment, containment,120s and all oracles remain. Approved native runtime
root,16GiBforecast and reserve/identity guards are unchanged.

- Linux100PASS8.904150s;125PASS12.211301s;200PASS14.911322s, each once.
- Mac100PASS7.9617s;125PASS8.15264s;200PASS10.0024s, each once. Independent
  mechanical peer used only exact602d98payload, one normal targetbuild0/6.370s,
  equivalent configured rows/compiler flags/Qt6.12.0 and a fresh quiet window.

All six anchored pass markers were reached. Linux2372protected inputs and
candidate hashes remain exact; Mac4671protected producer inputs remain exact.
Source55clean, Linux owned forests empty, actualtaskReady/result0 and idle12
empty native process list. Both native leases and producer borrows returned;
no remaining dependency on mutable55normal build trees. Rawfailed55results
retain their meaning. These are engineering proofs, not release qualification.
Fresh common-source all-native gates remain root's responsibility.

LLVM22format/full-TUtidy0, native/WX compilation, static message/gesture/classifier
audit, explicit authored-plan binding, board, documentation and diff checks
cover this narrow slice. implement-rfc and installed frontend-design guidance
were loaded; no product visual design changed.

## Provenance and retained evidence

Actual issue2050 was allocated normally on main085cf45 after counter-only
repair293ecfec2048to2050. Historical6bc29a5 reserved foreign2048/2049; its
immutable receipt is retained as provenance only, with no foreign cards or
product imported. NEXT2051 remains monotonic. Own frozen55branch has no alternates.

External root:
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer55-linux-diff-workbench-triage-r1`.
Original142files/19.23MB, full2failure blocks, paired-mask-analysis.json and
47diagnosticfiles/11.36MB remain sealed. positive-r1/terminal retains133files/
19.81MB including receipt, JUnit/rawcourses, exact rows, private image/object
and producer/candidate seals. Corrected manifestSHA256
`4d58c9f7eebc2951b5ee9e1bffb00f3585e52d6fa9e7358924a2cd02497b1f1f`.
Macpeer evidence is sibling implementer2050-macos-cross-native-r1/receipt.json.
No transient binary, log or screenshot is committed. The external owned-commit
manifest gives the exact selective release integration order.
