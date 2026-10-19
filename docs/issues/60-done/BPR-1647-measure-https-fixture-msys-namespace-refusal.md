---
schema: "implement-rfc.card.v1"
id: "BPR-1647"
title: "Measure HTTPS fixture MSYS namespace refusal"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/github_contract/CMakeLists.txt","scripts/verify_test_network_isolation.py","tests/scripts/verify_test_network_isolation_tests.py"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"Only release_update_native_https uses the measured msys-runtime-object finite pin, with matching WINDOWS_UNCONTAINED registration and verifier/unit cause set. Loopback port count1, all26 scenarios,90s timeout, credentials/privacy and product policy remain unchanged. No shared ACL change.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-focused-recovered-r1/result.json"}},{"criterion":"AC2","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"Original full26 direct course passes; exact contained MSYS OpenSSL NtCreateDirectoryObject BaseNamedObjects runtime namespace refusal0xC0000022/exit3221225794 retained. Actual registered full HTTPS row PASS8.44005s, mandatory native Windows manifest1.41546s,72 verifier unit checks PASS. Mac/Ubuntu generated manifests also pass; other platform containment is unchanged. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-focused-recovered-r1/junit.xml"}}]
---

# BPR-1647: Measure HTTPS fixture MSYS namespace refusal

The full26-scenario native HTTPS course passes directly. Its contained OpenSSL child exits3221225794 at NtCreateDirectoryObject(\BaseNamedObjects\msys-2.0S5-1888ae32e00d56aa), status0xC0000022. This is a measured external MSYS kernel-object boundary.

## Acceptance

- AC1: Register only release_update_native_https in the existing finite Windows exception mechanism with msys-runtime-object, matching source registration and verifier/unit cause inventory. Retain loopback port count1, disposable CA, all26 scenarios, privacy and90s timeout; no shared ACL or product policy change.
- AC2: Preserve exact direct/contained logs and tool hashes, run the actual complete native HTTPS row plus mandatory generated test_network_isolation_manifest and verifier unit/registration checks.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.

## Source checkpoint

The bounded repair is implemented without changing product behavior, original
assertions or deadlines. Source self-review and diff checks pass; touched C++
uses clang-format22. Actual focused native verification remains pending.

## Observed verification and closeout

Original full26 direct course passes; exact contained MSYS OpenSSL NtCreateDirectoryObject BaseNamedObjects runtime namespace refusal0xC0000022/exit3221225794 retained. Actual registered full HTTPS row PASS8.44005s, mandatory native Windows manifest1.41546s,72 verifier unit checks PASS. Mac/Ubuntu generated manifests also pass; other platform containment is unchanged. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.

The changed C++ set passes clang-format22; the four shared fixture translation
units pass focused clang-tidy22. The Windows-only app observation compiles in
actual Debug and Release builds. The network verifier72 unit checks,
documentation67 tests and documentation/diff gates pass. Offscreen evidence
qualifies no native desktop or system appearance. Final producer source freeze
follows all card/ledger upkeep; remaining timing/native/vault/security and
source-access obligations stay with the coordinator.
