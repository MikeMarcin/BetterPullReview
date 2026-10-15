---
schema: "implement-rfc.card.v1"
id: "BPR-1527"
title: "Repair final native release composition contracts"
status: "In Progress"
rfcs: ["RFC-BPR-0023", "RFC-BPR-0052"]
depends_on: []
priority: "P1"
metadata: {"source":"Final 0.2.1 macOS normal gate at d52f5bb1221e26dc00dbc9269c3ad0408b64548a","assigned_implementer":"/root/bpr1525_implementer","mandate":"authorized-release-continuation-final-gates-r3","milestone_plan":{"id":"bpr-1527-final-native-release-contracts","path":"docs/rfcs/plans/bpr-1527-final-native-release-contracts/milestone-plan.json"}}
---

# BPR-1527: Repair final native release composition contracts

The final macOS release gate exposed five failures on clean integrated source
`d52f5bb1221e26dc00dbc9269c3ad0408b64548a`: agent invocation composition,
UIforETW selected diff, threaded reply, label authoring and reply draft.
Preserve their actual output and distinguish inherited defects from regressions.
BPR-1525 and BPR-1526 remain Done with their original acceptance intact.

## Acceptance

- AC1: Retain each of the five exact-source failures and demonstrate its cause, including baseline provenance and useful counterexamples; identify any remaining uncertainty honestly.
- AC2: Threaded Reply, label authoring and reply-draft public contracts pass with exact thread, anchor, draft, publication, refresh and restart assertions intact; navigation, stale scope and unrelated drafts cannot acquire a delayed reply target.
- AC3: The agent invocation seed and reopen composition pass through the accepted immutable-source public path, preserving deliberate mutable-input refusal, unchecked-source confidentiality, exact invocation/cleanup/quiescence and zero GitHub mutation assertions.
- AC4: The UIforETW public capture passes an exact complete toolbar-order oracle including the shipped Accept file action, preserving existing appearance, geometry, navigation, pixel and height assertions; an omitted or misplaced required control fails.
- AC5: Pass focused native Mac verification of all five courses and affected portable/Windows/Ubuntu paths when their slots are released, meaningful negative controls, format 22, appropriate strict tidy, architecture and documentation checks; self-review and commit the narrow repair, and hand back exact source/command ownership with exact-source release gates and packages required for the selected release set: macOS-only 0.2.1 now, Windows 0.2.2 follow-up. The affected portable/Windows/Ubuntu engineering acceptance remains required to finish BPR-1527; it no longer blocks the separately accepted Mac slice from 0.2.1 integration.
- AC6: Diagnose the final d52 Windows `qt_diff_workbench_scale_200` timeout120.03s, demonstrate a bounded source-backed whole-workbench improvement and pass the normal complete course within its unchanged120s deadline under prescribed resource exclusivity, preserving all input/assertions and checking affected scales; retain the prior119.08s focused result and final-gate context honestly.
- AC7: Diagnose and repair `application_review_progress_push_contract`, all three `application_review_tour_composition` scales, `application_active_changed_scaling` and `application_review_tour_cold_import_public_path`, retaining exact durable receipt identities, tour receipt isolation, automatic-Seen policy, bounded changed-set work and meaningful counterexamples without weaker thresholds or semantic assertions.
- AC8: All three `application_source_find` scales pass their existing supported900-pixel-window, every-drawer-open floating-widget fit and query-width assertions, with exact geometry/focus/interaction and native appearance retained; counterexamples detect the demonstrated layout cause.

- AC9: Repair the enumerated final Windows title/browser/window local-cycle republication contract failures, preserving exact same-PR local-cycle identity and browser-action refusal, new-PR replacement, Home rules and all platform scale/width assertions; pass meaningful focused counterexamples and native courses once the PC slot is released.

- AC10: Diagnose and repair the three explicitly enumerated final Windows review-submission composition courses, retaining exact current-head line and inline-draft admission, pending-selection and summary stale-view refusal, publication counts, recovery and authority assertions; pass unchanged native courses at all three scales after PC slot release. Mac d52 passes do not establish a Windows fix.

- AC11: Diagnose and repair the final Windows application_review_navigation_entry bound-message lifetime failure, preserving exact Review entry, file/pair/semantic position, all command routes and the existing approximately three-second explanation lifetime with unchanged 2600ms persistence and 3600ms upper checks; pass the normal native course for0.2.2 after PC release.

## Explicit scope amendment

The coordinator explicitly added the sixth criterion after the final Windows gate timed out at
test359 (346/705). The Qt courses were serial; the earlier architecture591.63s
and age180.26s courses had finished, with no competing root/WSL workload.
The 125% course was still running when the amendment arrived: do not preempt it.
All original five criteria and milestone exits remain. This is a new acceptance
unit under the existing r3 mandate, not a reopened BPR-1525 or relaxed120s gate.

The complete final Mac result was719pass/14fail/1skip, exit8. On releasing the
Mac slot, the coordinator explicitly added nine courses: review-progress push,
three tour composition scales, active-changed scaling, tour cold import and
three source-find scales. The six prior criteria and exits remain unchanged.
All fourteen exact diagnostics were read before further edits. The failed
producer remains untouched; only the owned-worktree Mac build slot is released.
Windows/WSL remain coordinator-owned while their final suite runs. The skipped
Debug cache-bytes course still requires the later optimized Release supplement.

## Ownership and limits

The coordinator allocated BPR-1527 in `26fa0b98fde716b7aefffb92da0bae7676ab3c8e`
and assigned the existing sole implementer under mandate
`authorized-release-continuation-final-gates-r3`. The assigned worktree is
`codex/v021-windows-gate-fixes`. Root main and both active final producer
suites stay untouched. No native build, test or performance process begins
until the coordinator explicitly releases its platform slot. Later failures
need an enumerated scope amendment. No assertion or threshold relaxation,
unsupported-platform waiver, signing, authentication or publication is authorized.

The [checkpoint](../_checkpoints/BPR-1527/CURRENT.md) records the diagnosis,
runtime binding and native continuation frontier.

The separately tracked local-cycle/browser unit currently enumerates: `qt_workbench_title_row_scale_100`, `qt_workbench_title_row_scale_125`, `qt_workbench_title_row_scale_200`, `qt_workbench_open_on_github_scale_100`, `qt_workbench_open_on_github_scale_125`, `qt_workbench_open_on_github_scale_200`, `qt_workbench_window_scale_100`, `qt_workbench_window_scale_125`, `qt_workbench_window_scale_200`. All report the same local-cycle republication assertion. No unobserved selector is inferred.

The scoped [source self-review](../_checkpoints/BPR-1527/SELF_REVIEW.md) records the pinned range, exact origin/lifecycle guards and source-backed oracle reconciliation. Focused evidence is recorded in the checkpoint; all pending native/static/release obligations remain explicit.

Coordinator read-only review found and reproduced a retroactive receipt-origin defect in intermediate0f6d. The [review disposition](../_checkpoints/BPR-1527/review-origin-correction.json) records the26.06s reverse-control failure and corrected6747ca029 source with both held orderings and20normal public courses passing; fresh strict checks of all eight selected TUs, format 22 and architecture pass; native acceptance remains pending.

The coordinator explicitly added a separate submission contract unit after final
Windows tests #621/#622 failed in 42.47s/42.83s and then confirmed #623 with the
same current-head pending-draft selection assertion. Exact selectors are
`application_review_submission_composition_public_path`,
`application_review_submission_composition_scale_125` and
`application_review_submission_composition_scale_200`. Prior nine criteria and
eight exits remain. Mac d52 passed these courses; no Windows fix is inferred from
the reply repair, because this failure occurs before Comment or draft creation.

## Human release boundary amendment, 2026-10-15

The human directed: "If windows is far away then just release Mac only and
continue Windows fixes for 0.2.2". The coordinator explicitly selects macOS-only
0.2.1; Windows and Ubuntu 0.2.1 production is canceled/deferred, and Windows
repairs target 0.2.2. This supersedes the prior common-source, three-platform
0.2.1 coupling. The accepted focused Mac source may integrate for its full Mac
gate while this card remains In Progress for Windows native repair and affected
portability verification. All ten criteria and nine exits retain their behavioral
and timing requirements; no failed course becomes accepted.

[The release-scope receipt](../_checkpoints/BPR-1527/release-scope-amendment.json)
preserves authority, exact candidate, pending native/security/legal obligations
and the producer boundary. This docs-only slice does not change version 0.2.1.
After handback, root owns Mac CPU and its frozen producer. Later Windows repairs
stay on the owned branch after explicit PC slot release; they never modify or
rebind the Mac 0.2.1 producer. Ubuntu release timing is not newly selected.

## Windows 0.2.2 navigation unit

After the macOS-only notes handback at `c233c7683`, root explicitly adds the
completed final Windows #691 `application_review_navigation_entry` failure
(67.40s): "bound explanation remains for almost three seconds". The new criterion and tenth exit
preserve all previous ten criteria/nine exits, source identity and timing bounds.
This follow-up is excluded from the frozen Mac 0.2.1 producer. The complete d52
Windows result is 705 accounted: 679 pass, 25 fail, 1 configured Debug cache-bytes
skip. Root released the quiescent PC to this implementer for serial Windows/WSL
repair and acceptance; there is no signing, auth, security or reboot authority.
