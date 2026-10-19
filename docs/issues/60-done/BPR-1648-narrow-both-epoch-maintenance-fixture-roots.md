---
schema: "implement-rfc.card.v1"
id: "BPR-1648"
title: "Narrow both epoch maintenance fixture roots"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/git_contract/epoch_maintenance_integration_tests.cpp"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"Existing make_storage_root_owner_private now narrows both Git and catalog fixture roots; production permission checks, pruning, deletion, lifetime, byte totals and recovery predicates remain.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-focused-recovered-r1/result.json"}},{"criterion":"AC2","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"Complete native epoch maintenance PASS8.86113s and two Windows recovery consumers PASS23.6378/36.5195s. Corresponding Mac and Ubuntu three consumers pass. Original false Windows Git-root privacy predicate and direct storage refusal remain retained. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-epoch-consumers-recovered-r1/junit.xml"}}]
---

# BPR-1648: Narrow both epoch maintenance fixture roots

The native epoch maintenance fixture narrows catalog with make_storage_root_owner_private but uses POSIX permissions only on git. Actual Windows is_owner_private(git,true) is false before Insights storage construction.

## Acceptance

- AC1: Apply the existing owner-private helper to the fixture git root as well as catalog, preserving production storage permission checks, lifetime, pruning, deletion and byte totals.
- AC2: Rebuild and run the complete native epoch_maintenance_integration course and retain the original direct Insights review store unavailable failure and measured root predicate.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.

## Source checkpoint

The bounded repair is implemented without changing product behavior, original
assertions or deadlines. Source self-review and diff checks pass; touched C++
uses clang-format22. Actual focused native verification remains pending.

## Observed verification and closeout

Complete native epoch maintenance PASS8.86113s and two Windows recovery consumers PASS23.6378/36.5195s. Corresponding Mac and Ubuntu three consumers pass. Original false Windows Git-root privacy predicate and direct storage refusal remain retained. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.

The changed C++ set passes clang-format22; the four shared fixture translation
units pass focused clang-tidy22. The Windows-only app observation compiles in
actual Debug and Release builds. The network verifier72 unit checks,
documentation67 tests and documentation/diff gates pass. Offscreen evidence
qualifies no native desktop or system appearance. Final producer source freeze
follows all card/ledger upkeep; remaining timing/native/vault/security and
source-access obligations stay with the coordinator.
