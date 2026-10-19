---
schema: "implement-rfc.card.v1"
id: "BPR-1644"
title: "Diagnose Ubuntu Insights budget description"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Native f8b8 QLocaleC description has exactown412/other1240/remaining3348 withoutcomma; recordedoriginalpredicate fails. Later1700files literal similarlyfails, en_US control passes.","artifact":".mask/sessions/release-v0.3.0/native-linux/v030-linux-focused-r4-evidence.tar.gz"}},{"criterion":"AC2","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Only expectations format exact1240/3348/1700 through existingQLocale; ownpersistedwait412, exacttextanchors/allrecordconditions and everybudget/privacy/stall/storage/renderassertion retained. No producttext/calculation change.","artifact":".mask/sessions/release-v0.3.0/native-linux/v030-linux-focused-r4-evidence.tar.gz"}},{"criterion":"AC3","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Complete nativeLinuxc3bc course passes defaultC1.19s and en_US1.16s; Mac4e0a completecourse passes1.57s. Scopedtidy22Werror/format22/architecture/docs/diff pass; intentionalcommits retained.","artifact":".mask/sessions/release-v0.3.0/native-linux/v030-linux-focused-r4-evidence.tar.gz"}}]
---

# BPR-1644: Diagnose Ubuntu Insights budget description

The last actual Ubuntu normal row application_insights_public_path fails the
two-client budget assertion after the persisted ownRequests property reaches412.
The same run observes one real250ms main-thread stall; its later failure is the
accessible description's expected1,240 other-client and3,348 remaining strings.

## Acceptance

- AC1: Retain the original row and observe actual accessible text, locale and persisted own count; establish whether budget semantics or presentation expectation causes the failure without assuming a locale issue.
- AC2: Make only a source-backed bounded repair preserving exact persisted count, lowest remaining response, budget split and accessibility/localization behavior, plus all stall/privacy/storage/render assertions.
- AC3: Verify the complete native public course and meaningful controls, focused format/tidy/documentation, intentional commits and host quiescence; no blind assertion weakening or threshold change.

## Source diagnosis checkpoint

The production budget_bar formats all four numeric fields through QLocale.
The assertion spells comma-grouped literals, and does not currently report the
actual description or locale. A platform-locale mismatch is a hypothesis until
native failure context is observed. Failure-only diagnostics can expose the
synthetic description, ownRequests property and locale with the original
predicate unchanged.

Evidence: ignored ubuntu-gate-r5 raw/normalized logs, lines5404–5411.
The coordinator explicitly released the shared PC for focused repairs; the
Mac807 integration remains parent-owned and protected.


## Actual native observation and bounded repair

At sourcef8b8f3383 the original predicate fails under QLocale C with ownRequests412
and accessible text: BetterPullReview about412 requests; other clients about1240;
3348 left of5000. The exact seeded counts are correct; this locale does not use
comma group separators. Native receipt/log v030-linux-focused-r2 retains the
full description and original failure. No budget calculation defect is inferred.

The assertion now formats the same exact expected1240 and3348 using QLocale,
matching the established product localization contract. It also binds them to
their existing other-clients/left-of text, rather than accepting an arbitrary
substring occurrence. Persisted412 wait, all budget semantics, product text,
stall bounds and every later public-course assertion remain unchanged. Source
format22/diff checks pass; native complete-course and tidy verification remain
pending on this candidate.


The next native control at395ae2393 confirms the corrected budget passes in C
and exposes one further comma literal in the same course: the1700-files
lifetime record. The complete en_US course passes1.17 seconds with unchanged
values. Production records use the same QLocale formatter. That expectation now
formats exactly1700 files through QLocale while retaining all five record
conditions; the next native C course must still finish every assertion.

## Verified repair result

Native f8b8 QLocaleC description has exactown412/other1240/remaining3348 withoutcomma; recordedoriginalpredicate fails. Later1700files literal similarlyfails, en_US control passes.

Only expectations format exact1240/3348/1700 through existingQLocale; ownpersistedwait412, exacttextanchors/allrecordconditions and everybudget/privacy/stall/storage/renderassertion retained. No producttext/calculation change.

Complete nativeLinuxc3bc course passes defaultC1.19s and en_US1.16s; Mac4e0a completecourse passes1.57s. Scopedtidy22Werror/format22/architecture/docs/diff pass; intentionalcommits retained.

Exact receipts, raw failures, JUnit and source bindings remain under the ignored
release-v0.3.0 session; other release qualifications are separate obligations.
