# BPR-1551 completed engineering checkpoint

BPR-1551 is Done: all five criteria and four milestone exits are accepted under
`authorized-release-continuation-final-gates-r3`, segment `pe-private-library-r1`,
plan `bpr-1551-private-library-release-proof` / `declared-pe-library-proof`.
Sole engineering owner `/root/bpr1525_implementer`, rfc-implementer /
gpt-6.1-sol / xhigh; no subagents. BPR-1527 remains Done.

## Frozen source and final artifact

The coordinator froze producer/tag candidate
`23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d`, version0.2.2. This owned development
branch subsequently contains evidence and canonical Done notes only. They must
not enter or rebind the frozen artifact/tag source, and require no package rebuild.
Published Mac0.2.1 remains immutable. This owner issued no PC commands after its
06:55:45Z quiescent transfer and performed no package/sign/publication operation.

Coordinator packageR2 completed exit0 at2026-10-16T06:59:33Z. Actual administrative
MSI extraction and auditR4 completed exit0 at07:00:30Z. Supplied native trust rows
show MSI plus all four source-built images Valid, Michael Marcin, timestamped.
Both RELEASE_PACKAGE_OK and RELEASE_SECURITY_MECHANICS_OK cover all17PEimages;
xdiff uses byte-identical producer plus bound full linker map, not an export-only
or filename allowance. Source/SPDX/archive identities match the frozen revision.

- MSI SHA256: `b3e2a5420893604ebb6b30d7708cb2405d090a16e09dc459d049bf1155a50f85`.
- Signed xdiff SHA256: `f5a210550598896a416fce616571c5d5acc4ed27525c461a79f96e9b7b6b4ee2`.
- SPDX SHA256: `088c1778af8dec5125ded29651100a901fb8fad49ec33e30e3be2f1d7a93d039`.

[Final artifact acceptance](artifact-acceptance-r4.json) binds all supplied
receipts, complete image set, source asset and qualification limits. Originals
were copied into the owned private `.mask/sessions/BPR-1551/artifact-r4` evidence
root with exact digests. No original failed candidate or receipt was rewritten.

## Repair and verification

The preserved4c candidate failed undeclared-pe-producer; its source-built xdiff
DLL lacked a map and signature. The repair emits the fixed private-library
package identity and exact CMake target producer, refuses unknown/foreign/alias
paths, and requires DLL type, exact bytes and full map identity/address/symbols.
MSVC shared libraries emit maps while retaining existing hardening. The maintained
package path signs and checks the library with app/helpers before/after CPack;
offline manifests require all four source-built receipts. LGPL dynamic linkage,
source pin and runtime/application behavior are unchanged.

[Native R1 proof](native-proof-r1.json) records the actually relinked44,032-byte
DLL and46,203-byte map,824symbols and ASLR/DEP/CFG declarations. Altered installed
bytes, identical unknown DLL, missing map, wrong map and hidden fixture symbol
all fail. Portable71cases are69pass/2explicit native skips; Windows71cases are
68pass/3explicit platform skips, with complementary Windows/Bash/POSIX coverage.
The Linux-native CPack fixture remains unexecuted; no Linux release is selected.
No broad705application rerun was needed for this verifier/build-evidence-only fix.
Documentation/board checks and [self-review](SELF_REVIEW.md) pass with unchanged
requirements and plan binding. Native evidence supplied by the coordinator closes
AC5; no human criterion or attended server is pending.

## Ownership and remaining release qualification

All implementer commands are finished; source worktree is clean after canonical
commit. PC/WSL and publication ownership remain with root. The frozen publisher,
tag candidate, MSI/runtime/source/SPDX assets and approved credentials were not
modified during this closeout. Root owns publication and downloaded-product trust
verification; this card does not claim those actions have completed.

The coordinator retains raw31vulnerability findings without new approval claims.
Mechanical audit and actual native signature evidence do not close independent
security/legal, clean-account/native runtime/vault/crash or broader M8/M9 gaps.
The supplied security report explicitly retains those pending qualifications.
Original4c failure, old unsigned DLL and all meaningful negatives stay preserved.

## Recheck before landing on main (2026-10-22)

The Done notes came from `codex/v021-windows-gate-fixes` (5108d4e05 and
be8bcd104), cherry-picked with `-x` onto main 4e318b625. Before landing them,
a Claude rfc-implementer checked again:

- Tag v0.2.2 resolves to 23447f3a9, which main contains. Main has not
  changed this card, its checkpoints or its plan since 23447.
- GitHub's asset digests for the v0.2.2 prerelease match the MSI
  (`b3e2a542…`), the SPDX (`088c1778…`) and the corresponding-source zip
  (`504b3855…`). The published `.msi.release.json` and `.msi.provenance.json`
  bind 23447, the application `af01d262…` and the signed xdiff `f5a21055…`.
- The four receipt inputs under the v0.2.2 publisher's
  `.mask/sessions/release-v0.2.2/native-final` still hash exactly as
  [artifact-acceptance-r4.json](artifact-acceptance-r4.json) records. R4
  still reads exit 0, source 23447, `RELEASE_PACKAGE_OK` and
  `RELEASE_SECURITY_MECHANICS_OK`, with licence approval pending and trust
  not assessed.
- On main, `private_library_producers` and `PRIVATE_LIBRARIES` still bind the
  declared-library producers, `inspect(..., private_producers=...)` still does
  the exact DLL check, and `Hardening.cmake` still emits shared-library maps.
  On macOS, `package_runtime_tests.py` (29, 1 skip),
  `native_release_tests.py` (30, 1 skip) and `verify_release_security_tests.py`
  (18) pass.

Nothing native was rerun, so R1 and R4 remain the Windows evidence. This
recheck approves nothing about release trust, security, legal terms,
vulnerabilities or the M8/M9 native, vault and crash gaps. BPR-0962 remains
blocked; see its card.
