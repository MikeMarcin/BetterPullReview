# BPR-1971 native launch admission checkpoint

Mandate: coordinator's release-blocker diagnosis, amended to pure-public
causal trace r2 on 2026-11-09. Operator `/root/windows_native_diagnosis`.
Own branch `codex/bpr1971-native-admission` in the existing exclusive managed
worktree. Allocation `ad032464a`, plan/claim `b8a7ef564` / `010968227`,
authorized amendment `7503ed4cc`. No production correction is made.

## Exact candidate and original failure

Native private checkout source `aa7d2395cab140f7317162dce986d9477425b572`.
Original invocation test TU SHA256
`c5599dd821db11fcb35b3d13cf048d2b9061d84efc349b075fa79b6482dbd24f`.
Normal VS18 x64 Release build of thirteen selected targets and normal
transitive dependencies passes in 571.63 seconds. Required selected image
MZ/PE/AMD64/header/hash checks pass. Eleven of twelve registered functional
rows pass once, including the six residual representatives, Git contract,
invocation host, epoch host and two diagnostic rows.

The original native-session wrapper fails in 86.01 seconds: first four
courses pass; transcript replay fails `unsupported-no-silent-fallback` with
`Not started: side chat refused: launch_refused`. Sixth course is unreached.
Original 400-second registered watchdog and 180-second child-course limits
remain unchanged. This actual functional failure remains unresolved and
gating. No diagnosis pass replaces it.

## Bounded observations

The failure-detail-only r1 diagnostic passes all six courses in 45.41
seconds, leaving its failure getters unreached. R2 captures pre/post public
snapshots around the same synchronous send. Its single instrumented image
has SHA recorded in the external receipt; exactly three authorized attempts
pass in 46.30, 45.31 and 45.74 seconds. No fourth attempt is executed.

All three pre/post snapshots show native loading false, Send enabled, three
process starts and the explicit native-continuation-unavailable mode.
Immediately after send, refusal is false and both refusal strings are empty.
Legacy conversation Loaded is not publicly exposed and is recorded as
`unexposed`, never as an inferred Boolean. No failed-admission reason was
captured. The suspected independent legacy/native-load race is unproven.
A unique loading refusal maps to `window_agent_chat.cpp` lines 502–503,
file SHA `e0081f25cef5511b5c49867b57a929da57d1fddae1178e90b6cf479b25dd4584`,
but that reason was not observed. No unrelated executor hold is repurposed.

Both temporary diagnostics are retired by restoring exact source bytes and
normal target rebuilds. Final r2 restoration passes in 66.11 seconds. No
restored-image course is rerun or given pass credit. Own native source is
clean at aa7; guarded source/cache/images and frozen producer hashes remain
unchanged. The normal About dirty/clean build identity rebuilds
`about_desktop.cpp` and changes the composition-library hash; all build logs
and original/diagnostic/restored images and PDBs are separately retained.

## Evidence and remaining work

External root:
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate43/windows-native-diagnosis-r1/BPR-1971/`.
It contains sealed inputs, both receipts, the unique-branch source binding,
all attempts' XML and both streams, target manifests, source/EXE/PDB copies,
plan binding and durable checkpoint. The sibling `prefreeze-aa7-windows-r1`
retains the original twelve-row failure and normal build/PE evidence.
Prior 8396 inputs are archived and member-verified before aa7 reconciliation:
14,384 members, 2,194,370,560 bytes, SHA256
`6c9735e58413d7b5c4cb189fa4ff85ec3d362875f3c6eaac88c87c583207b664`.

AC1's bounded observations and AC2's restoration are supplied. AC3's actual
cause/classification remains pending; the card stays In Progress. No source
fix, oracle broadening, timing waiver, full-suite qualification or new
acceptance is claimed. Further native attempts need a new bounded causal
control/scope from the coordinator. Final common-source compilation,
Windows/Linux native suites, optimized close/resource and release/trust gates
remain separate. Mac46 and Windows43 frozen sources were not mutated.

## Superseding controlled evidence and approved correction

The exact sealed r3 control captures the actual unique refusal on both hosts.
Windows wrapper exit8/86.16 seconds, held38ms; Mac exit8/65.59 seconds,
held15ms. Both report legacy composed/Loadedfalse, native loadingfalse,
Sendenabled and unchangedthree process starts. The immediate public refusal
names saved conversation still loading; the gate is explicitly released
before the unchanged original outcome wait. This identifies the shared
readiness mismatch on exactoldAA7 app bytes, without relabelling earlier
inconclusive attempts. Both operators restore allfour original source hashes
and normally rebuild; raw/XML/images/PDB/manifests remain external. Windows
r3 library deltas include logging and composition; no all-libraries-unchanged
or About-only claim applies to that phase.

Current correction mandate r4 is approved, source reconciled to
c76340d4ebd6cfbe44bfc708888be827e9275c31 including accepted1972large-project
policy andNEXT1974. The seven-path scope and three acceptance criteria are
recorded in the live plan/card. Product preseal v3 is independently reviewed
before content reseal; focused positive/negative native proofs and normal
main integration remain pending. The earlier AC3-pending text describes
that earlier checkpoint, not the now-confirmed controlled reason.
