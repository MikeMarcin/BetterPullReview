# BPR-1551 current repair checkpoint

Same sole rfc-implementer / gpt-6.1-sol / xhigh; mandate
`authorized-release-continuation-final-gates-r3`, bounded coordinator amendment
for Windows0.2.2 private-library release proof. BPR-1527 stays Done. Allocation
main32e9bcc0b is adopted as b72e320d3; claim d4a802d86. Owned branch incorporates
exact release4c118 plus prior docs-only333cc904a; no unrelatedmain is merged.

Native baseline4c118 is clean, no native workload. Actual signed/extracted MSI
fails undeclared-pe-producer; source-built44,032-byte bpr_xdiff.dll has no full
map and is NotSigned. App/helpers/MSI valid timestamped signatures retain their
actual scope. Original artifacts stay immutable. Root explicitly transfers PC
engineering/audit slot, retaining all package/sign/publication and credentials.

Plan: emit exact declared package-to-producer library path in trusted build
inputs; validate fixed declared set, native paths and exact producer bytes;
inspect private DLL through full map/public/static-symbol checks rather than
upstream filename allowance. Emit MSVC shared-library maps, sign the declared
library with existing app/helper path before CPack, and require its exact staged
bytes/complete offline receipts. Unknown DLL/missing map/mismatch/hidden seam
remain failures. Exercise portable fixtures and actual native built DLL controls.
Root then rebuilds/signs a new output; actual expanded-package audit/signature
coverage must pass before canonical finish. No application705 rerun for unchanged
runtime source, and no auth/settings/installer/publication work by this owner.

Private actual baseline and scripts/evidence: `.mask/sessions/BPR-1551`.
No human review criterion or server. Commands currently quiescent; next is narrow
implementation and meaningful script/native producer checks.

Current narrow implementation includes exact CMake target declaration, fixed-set
validation/build containment, DLL full-map/type/byte proof, shared-map emission,
and existing signing/offline receipt coverage of all four source-built images.
Portable71cases:69pass/2explicit native skips. No actual native or package pass
is inferred. Source review is SELF_REVIEW.md; native candidate commit/build next.

## Verified source handoff, native R1

Committed code23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d builds the actual
44,032-byte xdiff DLL and46,203-byte native full map. Exact byte/map/type/
section/address proof yields824symbols; ASLR/DEP and CFG declaration pass. Actual
DLL controls reject changed installed bytes, identical unknown DLL, missing map,
wrong map identity and a hidden fixture name in the full table. Nothing is signed
by this owner. Native71script cases are68pass/3explicit platform skips; Mac
71cases are69pass/2explicit native skips, with complementary Windows/Bash/POSIX
coverage. The Linux-native CPack fixture remains unexecuted; no Linux release is
selected. [Native proof](native-proof-r1.json) retains actual hashes/scopes.

At06:55:45Z the task is Ready/exit0, ownedPIDs[], nativeWindows[], WSLnative[],
actual inventoryexit0; producer remains clean23447 on its owned branch. Root may
freeze/adopt this source for its authorized new-output rebuild/sign/audit. This
owner releases PC native slot now and will run no further PC command until root
returns it. Independent docs gate passes. BPR1551 remains InProgress: actual new
expanded-MSI audit and all4source-built image signatures must pass beforeAC5 and
canonical finish. Post-artifact Done notes stay on this owned development branch;
they never rebind the frozen artifact/tag source or force a docs-only rebuild.
