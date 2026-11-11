---
schema: "implement-rfc.card.v1"
id: "BPR-2001"
title: "Verify and close RFC-BPR-0101"
status: "Done"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0013-close-agent-chat-conventions.md","draft_title":"Close the agent chat conventions RFC"}
depends_on: ["BPR-1998", "BPR-1999", "BPR-2000"]
closeout_for: ["RFC-BPR-0101"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_workbench_window_scale_100|qt_tour_side_chat'"}},{"criterion":"AC02","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'core_agent_reference|qt_workbench_window_scale_100'"}},{"criterion":"AC03","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_workbench_agent_send_refusal|qt_conversation_cards_view'"}},{"criterion":"AC04","outcome":"pass","revision":"b1d66829db229f0bb9474ab27a0e361306695cdf","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'core_agent_cost|qt_workbench_agent_send_refusal'"}},{"criterion":"AC05","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"observation","detail":"Opt-in live-claude course with installed Claude Code 2.1.319; ledger section Installed Claude course"}},{"criterion":"AC06","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_tour_side_chat"}},{"criterion":"AC07","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_workbench_window_scale_100"}},{"criterion":"AC08","outcome":"pass","revision":"b1d66829db229f0bb9474ab27a0e361306695cdf","evidence":{"kind":"observation","detail":"Owner accepted the accent in session fd2f7fdb and the one-line footers in session 9b8a51a3"}},{"criterion":"AC09","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_appearance_settings_panel|qt_colour_role_list|qt_appearance_role_display|core_appearance_contrast|application_appearance_older_theme_contract'"}}]
---

# BPR-2001: Verify and close RFC-BPR-0101

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md) against actual
evidence from the cards this one depends on, including its live,
installed-agent and owner-attended criteria, the reconciliation of the live
RFCs it amends, and the documentation gate. Record failures and unreached
requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Each listed form, in prose, in emphasis and as a whole code span, becomes a link in an Agent tab reply, a selected-lines side chat reply and a tour side chat reply, and opens the named line with its range revealed; the same text inside fenced code does not. | `qt_workbench_window_scale_100` (Agent tab and the side chat panel it presents), `qt_tour_side_chat`; fenced code left alone | Pass | `073cca272` |
| **AC02:** A reference to an unchanged file of the head opens that file at the line or is not drawn as a link; a name shared by two files offers both; a path that matches nothing is plain text. | `core_agent_reference`, `qt_workbench_window_scale_100`: shared name offers both, missing path plain, unchanged file not a link (no unchanged-file view) | Pass | `073cca272` |
| **AC03:** Return sends and Shift+Return adds a line in the Agent tab and the selected-lines side chat; Return during input-method composition does not send; a comment composer still requires Ctrl or Cmd with Return. | `qt_workbench_agent_send_refusal` (`test_the_agent_chat_sends_on_return_and_says_its_cost`), `qt_conversation_cards_view` | Pass | `073cca272` |
| **AC04:** Reported costs of 0.4123567, 0.004 and 12 are shown as `$0.41`, `< $0.01` (amended 2026-11-10 from `under $0.01`, [[BPR-2038]]) and `$12.00`; a run with no reported cost shows none. | `core_agent_cost`; `qt_workbench_agent_send_refusal` at the BPR-2038 candidate | Pass | `b1d66829d` |
| **AC05:** After three turns in one native session with an installed Claude, each turn's footer shows a cost, the session line shows their sum, and the record states how the runtime's figure was interpreted. | Installed Claude course: three native turns, footers and session sum; interpretation recorded (`ResumedNative`) | Pass | `073cca272` |
| **AC06:** The tour side chat shows the turn and session costs under the same rules. | `tour_thread_cost`; tour side chat ending line and session total | Pass (no installed tour run) | `073cca272` |
| **AC07:** An inventory of every control that dispatches an agent run and every control that wears the accent shows the two sets are equal, in the Agent tab, the side chats, the tour, the comment workshop and Review Cycles. | `test_the_agent_accent_is_on_exactly_the_run_controls` walks every button of the composed window | Pass | `073cca272` |
| **AC08:** The owner accepts the Agent tab's accent and footers on a frozen build. | Owner looks: accent (`fd2f7fdb…`, `e8d273c6`, frozen `c1d445720`); footers reworked by BPR-2038 and accepted (`9b8a51a3…`, `f55208c9`, frozen `b1d66829d`) | Pass | `b1d66829d` |
| **AC09:** Appearance › Colours lists Agent accent; with nothing customised every accented surface shows the shipped Light and Dark values; changing it for one theme changes every accented surface in that theme and no other theme; Reset restores the built-in value; and a theme document written before the role existed imports with the built-in value. | `qt_appearance_settings_panel`, `qt_colour_role_list`, `qt_appearance_role_display`, `core_appearance_contrast`, `application_appearance_older_theme_contract` | Pass | `073cca272` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0101.md). AC02 holds through its second branch (an unchanged file is not drawn as a link; there is no unchanged-file view). AC06 rests on unit and composed courses; no installed-agent tour run was made, and the criterion does not ask for one. The AC07 inventory test walks the current window and has passed in later rounds after BPR-1995 and BPR-2051 removed controls. Documentation gate run before this commit. RFC-BPR-0101 is Auto-Verified.
