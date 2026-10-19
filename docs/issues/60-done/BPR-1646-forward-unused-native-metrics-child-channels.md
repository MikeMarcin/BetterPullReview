---
schema: "implement-rfc.card.v1"
id: "BPR-1646"
title: "Forward unused native metrics child channels"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/application/insights_native_metrics_tests.cpp"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"Only the fixture forwards unused QProcess standard channels; containment, canonical self child, owned scratch,80ms CPU work, retained exact Windows process handle, reap and lifetime assertions remain. Two same static Qt queries are qualified for tidy22; no exception or product launch policy change.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-focused-recovered-r1/result.json"}},{"criterion":"AC2","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"Actual contained Windows final metrics PASS0.240959s, original direct-positive/contained private-pipe refusal retained. Mac final rebuilt metrics/tidy22 and actual Ubuntu metrics pass. Generated Windows manifest still selects AppContainer launcher. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-cache-bytes-release-recovered-r1/metrics-junit.xml"}}]
---

# BPR-1646: Forward unused native metrics child channels

application_insights_native_metrics passes directly but its contained canonical self child cannot create QProcess private standard-stream pipes. Neither parent nor synthetic energy child consumes those streams.

## Acceptance

- AC1: Forward only the unused standard input/output/error channels in the native metrics fixture, keeping containment, canonical self child, isolated scratch,80ms CPU work, exact process handle, reap and lifetime assertions.
- AC2: Rebuild and run the actual contained native metrics row. Preserve the original direct pass and contained CreateNamedPipe access denial; do not register an uncontained exception.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.

## Source checkpoint

The bounded repair is implemented without changing product behavior, original
assertions or deadlines. Source self-review and diff checks pass; touched C++
uses clang-format22. Actual focused native verification remains pending.

## Focused static-analysis follow-up

The actual Mac shared-fixture13-row course passes at7d4cd9f. Focused
clang-tidy22 exposes two pre-existing static-member-through-instance uses in
this touched fixture. Qualifying arguments/applicationFilePath with
QCoreApplication preserves the same static calls and the QCoreApplication
owner lifetime. Only this test translation unit changes; its exact native
rebuild/course/tidy follow-up remains pending. The original tidy failure
and passing other three translation units are retained.

## Observed verification and closeout

Actual contained Windows final metrics PASS0.240959s, original direct-positive/contained private-pipe refusal retained. Mac final rebuilt metrics/tidy22 and actual Ubuntu metrics pass. Generated Windows manifest still selects AppContainer launcher. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.

The changed C++ set passes clang-format22; the four shared fixture translation
units pass focused clang-tidy22. The Windows-only app observation compiles in
actual Debug and Release builds. The network verifier72 unit checks,
documentation67 tests and documentation/diff gates pass. Offscreen evidence
qualifies no native desktop or system appearance. Final producer source freeze
follows all card/ledger upkeep; remaining timing/native/vault/security and
source-access obligations stay with the coordinator.
