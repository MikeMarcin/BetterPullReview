---
schema: "implement-rfc.card.v1"
id: "BPR-2006"
title: "Verify and close RFC-BPR-0103"
status: "Done"
rfcs: ["RFC-BPR-0103"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0018-close-agent-work-visible.md","draft_title":"Close the agent work visibility RFC"}
depends_on: ["BPR-2004", "BPR-2005"]
closeout_for: ["RFC-BPR-0103"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_agent_helper_turn"}},{"criterion":"AC02","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'application_agent_helper_turn|application_agent_chat_helpers_public_path'"}},{"criterion":"AC03","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_agent_chat_helpers_unfinished_public_path"}},{"criterion":"AC04","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_agent_helper_turn"}},{"criterion":"AC05","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_agent_activity_view|core_agent_activity'"}},{"criterion":"AC06","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_agent_activity_view"}},{"criterion":"AC07","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"observation","detail":"live_second_account_course --course agent-helpers, Claude Code 2.1.319 and Codex 0.167.0, 2026-11-10"}},{"criterion":"AC08","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_tour_side_chat"}},{"criterion":"AC09","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"observation","detail":"Owner accepted BPR-2005-running, -quiet and -ended in attended session d2cc3389"}}]
---

# BPR-2006: Verify and close RFC-BPR-0103

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0103](../../rfcs/RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md)
against actual evidence from the cards this one depends on, including its
live, installed-agent and owner-attended criteria, the reconciliation of the
live RFCs it amends, and the documentation gate. Record failures and unreached
requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** For each installed agent and transport BPR supports, a recorded course states how background work is reported and whether the agent is run with helpers visible or foreground only. | Installed Claude Code 2.1.319 and Codex 0.167.0 on their real transports; `application_agent_helper_turn` replays three recorded traces | Pass | `12e9c6fb4` |
| **AC02:** With a scripted runtime that starts three helpers, writes a message, and reports `result` before they finish, the turn stays running, shows three helper rows with their states, accepts the later frames, and ends once after the closing message with one duration and one cost. | `application_agent_helper_turn`; `application_agent_chat_helpers_public_path` | Pass | `12e9c6fb4` |
| **AC03:** With the same script and one helper that never finishes before the connection closes, the turn ends as an error that names the unfinished helper and keeps the text that arrived. | `application_agent_helper_turn` (cut trace); `application_agent_chat_helpers_unfinished_public_path` | Pass | `12e9c6fb4` |
| **AC04:** A frame that arrives when no turn is open is refused as before. | `application_agent_helper_turn`: late answer and helper start refused after the turn | Pass | `12e9c6fb4` |
| **AC05:** After 120 seconds without a frame the notice appears with both choices; a new frame clears it; Keep waiting leaves the run untouched. | `qt_agent_activity_view` on a held clock (119 s, 120 s, Keep waiting, Stop); `core_agent_activity` | Pass | `12e9c6fb4` |
| **AC06:** During a turn the block names the file being read or the command being run as the runtime reports them, and "last activity" resets on every frame. | `qt_agent_activity_view`: reading, running, last activity; composed course | Pass | `12e9c6fb4` |
| **AC07:** With an installed agent on MikeMarcin/BPR-Test, a request that makes the agent start helpers either shows them and completes in one turn, or runs foreground only and completes in one turn. | Live `agent-helpers` course on BPR-Test #40 (read only): Claude Code shows three helpers in one turn; Codex runs them in the foreground in one turn | Pass for both installed agents | `12e9c6fb4` |
| **AC08:** The tour side chat shows the same block and the same endings. | `qt_tour_side_chat`: the same block, quiet notice and folded line | Pass | `12e9c6fb4` |
| **AC09:** The owner accepts the running, quiet and ended states on a frozen build. | Owner look `d2cc3389…`: `eead98a1`, `25cac43a`, `d19d4d2d` | Pass: all three accepted | `12e9c6fb4` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0103.md). BPR-2089 (the Agent tab reports idle before its first question, Backlog) is a later observation outside these criteria. Documentation gate run before this commit. RFC-BPR-0103 is Auto-Verified.
