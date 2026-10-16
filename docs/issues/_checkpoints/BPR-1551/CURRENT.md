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
