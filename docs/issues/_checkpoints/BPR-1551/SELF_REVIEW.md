# BPR-1551 source review

Sole implementer review under existingr3 mandate; not independent trust approval.
Only fixed declared private libraries gain producer lookup authority. CMake emits
package identity and actual target path; native Windows input validation refuses
foreign build paths, missing/extra identities and aliases. Auditing still requires
exact bytes, DLL type, full map identity/timestamp/base/entry/section/address and
complete public/static names. All names still reach forbidden-seam checks; unknown
DLLs retain existing upstream refusal. No binary payload exemption is added.

MSVC maps extend only to already hardened SHARED_LIBRARY targets; Release EXE
PDB/OPT settings and all existing hardening remain unchanged. App/runtime source,
Git xdiff pin, dynamic linkage and LGPL corresponding source are untouched.
Signing reuses the existing driver/publisher/certificate/timestamp checks for the
same declared source-built library; staged signed bytes and complete offline
receipts remain mandatory. The firstPartyImages wire field is an inventory name,
not a change of third-party copyright/licensing status.

Portable current fixtures: PE/security11pass, package-layout7pass, native-release
29pass/1native-Windows skip, runtime22pass/1native-Linux skip. Added tests show
staged DLL substitution stops before installer signing and missing DLL receipt
fails after all ordinary hashes/provenance are regenerated. Exact declaration,
foreign build path, missing/wrong full map, altered bytes/nonDLL, hidden seam and
identical unknown DLL are rejected. Native actual producer/full-map and Windows
module regression acceptance remain pending. Root will alone rebuild/sign into a
new output; actual extracted-package audit/signature coverage is required before
card finish. Old signed candidate and published Mac0.2.1 stay immutable.
