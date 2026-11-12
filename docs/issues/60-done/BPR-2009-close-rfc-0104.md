---
schema: "implement-rfc.card.v1"
id: "BPR-2009"
title: "Verify and close RFC-BPR-0104"
status: "Done"
rfcs: ["RFC-BPR-0104"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0021-close-agent-scratch.md","draft_title":"Close the agent scratch folder RFC"}
depends_on: ["BPR-2007", "BPR-2008"]
closeout_for: ["RFC-BPR-0104"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"882ebed54e593f0595b4afac458d5cc76ac50c1f","evidence":{"kind":"observation","detail":"Live agent-scratch course, both agents (ledger)"}},{"criterion":"AC02","outcome":"pass","revision":"882ebed54e593f0595b4afac458d5cc76ac50c1f","evidence":{"kind":"observation","detail":"Live: workspace writes refused, git status clean"}},{"criterion":"AC03","outcome":"pass","revision":"7a5bb7de1cf6f7289c0142a8fc02a94990cecb9b","evidence":{"kind":"observation","detail":"As amended: unit and host rows (ledger)"}},{"criterion":"AC04","outcome":"pass","revision":"882ebed54e593f0595b4afac458d5cc76ac50c1f","evidence":{"kind":"observation","detail":"Unit, host and storage rows; composed question has no course"}},{"criterion":"AC05","outcome":"pass","revision":"882ebed54e593f0595b4afac458d5cc76ac50c1f","evidence":{"kind":"observation","detail":"qt_agent_footer, core_agent_grant; owner accepted 246ef52c"}},{"criterion":"AC06","outcome":"pass","revision":"7a5bb7de1cf6f7289c0142a8fc02a94990cecb9b","evidence":{"kind":"observation","detail":"application_agent_launch argv tests; checkout guard by amendment"}},{"criterion":"AC07","outcome":"pass","revision":"7a5bb7de1cf6f7289c0142a8fc02a94990cecb9b","evidence":{"kind":"observation","detail":"Fourth independent re-review passed; items fixed in 7a5bb7de1"}},{"criterion":"AC08","outcome":"pass","revision":"882ebed54e593f0595b4afac458d5cc76ac50c1f","evidence":{"kind":"observation","detail":"core_agent_grant, sqlite_agent_setup_storage"}},{"criterion":"AC09","outcome":"pass","revision":"882ebed54e593f0595b4afac458d5cc76ac50c1f","evidence":{"kind":"observation","detail":"sqlite_agent_setup_storage and live next turn; New session shares the stored choice"}}]
---

# BPR-2009: Verify and close RFC-BPR-0104

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0104](../../rfcs/RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md)
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
| **AC01:** With the scratch folder on, an installed Claude and an installed Codex each write a script into the folder, run it against the workspace and report its output in one turn; with it off, the launch carries no such allowance. | Live agent-scratch course (Claude Code Haiku, Codex): each wrote `count.py`, ran it and reported its output; off: no allowance (`application_agent_launch`, `application_agent_invocation_host`) | Pass | `882ebed54` |
| **AC02:** With the scratch folder on, an attempt by the agent to create or change a file in the workspace or elsewhere outside the folder is not allowed by anything BPR passed, and `git status` in the workspace is clean after the turn. | Live: each agent's workspace write refused; `git status` clean in every made workspace | Pass | `882ebed54` |
| **AC03:** The folder is outside the repository and the workspace, is removed with its session, and refuses a write past 256 MiB with a reason the agent receives. Amended 2026-11-11: past its set limit (256 MiB to begin with), a turn starting gets no write allowance and is told why, and a turn reaching it is stopped with the reason shown; an unused folder is removed after the kept days, and Settings clears folders. | `application_agent_scratch_folder`, `application_agent_invocation_host`, `application_agent_launch`: the amended criterion (set limit, stop during a turn, removal by age, Clear) | Pass as amended; removal by pull request state is outside it ([[BPR-2101]]) | `531a039cd`, `7a5bb7de1` |
| **AC04:** Build and run tests is unavailable, with its reason, for a checkout the reviewer chose. In a disposable checkout it asks once per pull request, and declining leaves the launch without the build allowance. | `core_agent_grant`, `application_agent_invocation_host`, `qt_agent_setup_view`, `sqlite_agent_setup_storage` | Pass at the unit, host and storage layers; the question's composed path has no course | `882ebed54` |
| **AC05:** The footer picker's grant item (amended 2026-11-11, [[BPR-2051]]; its two toggles, amended the same day after owner look `e97c6a3d`) matches the launch for each of the three states, and no text calls the grant a sandbox for a runtime that does not enforce one. | `qt_agent_footer`, `core_agent_grant`; owner accepted the picker toggles (`246ef52c`) | Pass | `882ebed54`, `531a039cd` |
| **AC06:** Tour runs and one-shot hooks launch exactly as before. | `application_agent_launch`: read-only and tool-free runs refuse a grant; tour and hook argv tests pass | Pass; a launch in a made checkout gains only the checkout guard, by the RFC's third-round amendment | `7a5bb7de1` |
| **AC07:** An independent critical review has read the launch allowances for both agents and recorded what each runtime enforces and what it only permits. | Independent reviews: pass-with-findings twice, FAIL (F1, F2, fixed in `059e191fb`), then the fourth re-review passed with no blocking finding; its items fixed in `7a5bb7de1` | Pass | `7a5bb7de1` |
| **AC08:** A newly added agent and an agent carried over from 0.4.2 both launch with the scratch allowance; after the agent's starting state is set to Off, a pull request the reviewer has not changed launches without it. | `core_agent_grant`, `sqlite_agent_setup_storage` | Pass | `882ebed54` |
| **AC09:** Turning the scratch folder off from the footer picker's grant item in one pull request holds for that pull request and agent through New session and a restart, takes effect at the next turn, leaves another pull request on, and leaves the agent's starting state unchanged. | `sqlite_agent_setup_storage`; live footer course: next turn without allowance | Pass; New session reads the same stored choice and has no separate course | `882ebed54` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0104.md). AC04's question and AC09's New session have no composed course; both are the same stored choice the unit, host and storage tests read. Removal of scratch folders by pull request state is [[BPR-2101]], outside AC03 as amended. The release exposure the third round found in shipped builds is [[BPR-2119]] (main), outside this RFC's criteria. Documentation gate run before this commit. RFC-BPR-0104 is Auto-Verified.
