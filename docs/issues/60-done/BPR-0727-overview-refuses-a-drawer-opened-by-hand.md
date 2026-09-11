---
schema: "implement-rfc.card.v1"
id: "BPR-0727"
title: "Overview refuses a drawer opened by hand"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"c4817ab5-22dc-4ec9-9278-9c28c08ae457"}
acceptance: [{"criterion":"AC1 core_workbench_interaction pins that a drawer gesture in Overview is refused with a named reason and accepted in the other three modalities","outcome":"pass","revision":"working tree over a067a29f","evidence":{"kind":"command","detail":"test_overview_refuses_a_drawer_opened_by_hand: toggle and exact expand refused with overview_keeps_drawers_collapsed and state unchanged, toggles still offered, Read/Review/Discuss open and close by hand; ctest core_workbench_interaction passes"}},{"criterion":"AC2 qt_workbench_window_scale_100 pins that in Overview the strip toggles and shortcuts leave every drawer collapsed and the refusal is visible, and Read/Review/Discuss still open theirs","outcome":"pass","revision":"working tree over a067a29f","evidence":{"kind":"command","detail":"test_overview_refuses_a_hand_opened_drawer_and_says_so: modality intent precedes the presets, the toggle leaves as a gesture and stays unchecked, the published refusal reaches the status bar and last_refusal(), Read sends its modality then opens conversations; qt_workbench_window_scale_100/125/200, qt_composed_github_scale_100, application_workbench_host and application_composition_contract pass; architecture boundary tests OK; tidy and clang-format clean on touched files"}},{"criterion":"AC3 the owner accepts it on the signed bundle in an attended session","outcome":"pass","revision":"2cdc0e57","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0727-overview-drawers, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint cd9e85e3a5f668f64b26b56e4bc7cb7bad3d62926fdf7cec2f71d14661c10bba, decision e10736e3-b90e-4621-8bc6-e3065e9e883b, actor human, 2026-09-11T23:46:29Z: accept. Overview refuses a hand-opened drawer with the status bar saying why (visible in the owner's BPR-0646 Overview screenshot: 'Open the files drawer and close the comments drawer to read the pull request overview.'); the other modalities open theirs; and the d11 judgement shown for the verdict, that Overview is chosen and never inferred from the files-only drawer shape at restart, was accepted with it, without a note."}}]
---

# Overview refuses a drawer opened by hand

The owner chose, on the options candidate `BPR-0674-overview-drawer-policy`
in the v006 attended session (decision `c4817ab5`, 18:33:56Z): "Overview
keeps every drawer collapsed; a hand-opened drawer is refused". This is the
"or maybe always" of their 2026-09-07 note that
[BPR-0674](BPR-0674-overview-is-starved-by-the-drawers.md)
left to them.

## Scope

- While Overview is the active modality, the three drawers stay
  collapsed: the strip toggles, Alt+Shift+1/2/3 and, with drawer controls
  shown, the rails do not open a drawer, and say so (a disabled toggle is
  not allowed on the strip by RFC-BPR-0028, so the control stays enabled
  and the refusal is explained where the reviewer looks, or the gesture
  switches the modality). Choosing another modality restores that
  modality's preset as today.
- The core decides: the refusal is the workbench interaction's, keyed on
  the modality the window has chosen, not a Qt-side swallow; the window
  still presents what the core publishes.
- RFC-BPR-0028 AC-03 ("never change on their own unless a configured
  transition fires") and the Modalities paragraph are amended to name the
  Overview rule.
- Owned paths: `src/core/src/workbench_interaction.cpp` and its header,
  `src/adapters/qt/src/workbench_window.cpp`, their tests, RFC-BPR-0028.

## Acceptance

- `core_workbench_interaction` pins that a drawer gesture in Overview is
  refused with a named reason and accepted in the other three modalities.
- `qt_workbench_window_scale_100` pins that in Overview the strip toggles
  and shortcuts leave every drawer collapsed and the refusal is visible,
  and that Read, Review and Discuss still open theirs.
- The owner accepts it on the signed bundle in an attended session.

## Verification record, 2026-09-11 (d11)

The core decides. `workbench_interaction_state` gains `Modality`
(`core::workbench_modality`, default review) and the variant gains
`set_workbench_modality_intent`; choosing Overview collapses the three
drawers in the same decision, and while Overview is retained `apply_drawer`
refuses any request for `expanded` (toggle or exact set) with the new
`workbench_reason_code::overview_keeps_drawers_collapsed`, changing nothing;
collapsing stays accepted, and the drawer commands stay enabled (the strip
may not carry a disabled toggle; the header comment on
`project_workbench_commands` is amended). Read, Review and Discuss retain
the modality and leave the drawers to their presets, which the window still
sends as exact set-drawer intents after the modality intent.

The host publishes the refusal. `workbench_projection.Refusal`
(`src/application/include/bpr/application/workbench_runtime_ports.hpp`) is an
event carried by the publication that answers a refused gesture and never
republished; `apply_now` in `workbench_application_host.cpp` sets it from the
transition's reason. The existing host pin "a rejected gesture publishes
nothing" became "publishes only its refusal". No `src/app` file changed;
`dispatch_workbench_intent` forwards the variant as it is.

Qt presents. `choose_modality` dispatches the modality intent before the
presets; `publish` shows `drawer_reason_text(Refusal)` in the status bar and
exposes `last_refusal()`; in Overview `update_drawer_action` describes the
enabled, unchecked toggle with the rule. Alt+Shift+1/2 and the rails reach
the same toggles. `infer_modality_from_drawers` no longer reads the
files-only shape as Overview: Overview is chosen, never inferred, because
its preset (all collapsed, BPR-0674) is indistinguishable from a Read with
the conversations closed by hand and inferring it would make the core
refuse a toggle the strip never promised to refuse; the restart pin in the
window tests states that.

Pinned by `test_overview_refuses_a_drawer_opened_by_hand`
(`tests/core/workbench_interaction_tests.cpp`) and
`test_overview_refuses_a_hand_opened_drawer_and_says_so`
(`tests/adapters_qt/workbench_window_tests.cpp`). RFC-BPR-0028 AC-03 and the
Modalities paragraph name the Overview rule. Landed with [[BPR-0728]] as one
drawer milestone; the finish commit for this card carries the shared
`workbench_window.cpp` and window-test edits of both.

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0727-overview-drawers` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: Overview refuses a hand-opened drawer and says why; the second criterion is the d11 judgement shown for the owner, that Overview is chosen and never inferred from the files-only shape at restart.

## Attended verdict, 2026-09-11 (v007): accepted

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; the session was handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0727-overview-drawers`, decision `e10736e3`, 23:46:29Z: **accept**.
Accepted without a note, including the judgement made while landing the card: the window no longer infers Overview from the files-only drawer shape at restart; Overview is only ever chosen. Done.
