---
schema: "implement-rfc.card.v1"
id: "BPR-1245"
title: "Fifteen agent composition courses predate RFC-BPR-0023's right-side drafts and private runs"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-0587","BPR-1211","BPR-1139","BPR-1238"]}
acceptance: [{"criterion":"The fifteen rows pass on macOS","outcome":"pending","revision":"b2fd518bd","evidence":{"kind":"command","detail":"14 of 15 pass on macOS arm64 dev (all chat, invocation composition and workspace diagnostic, and eight finding rows). application_agent_finding_workshop_press_contract stays red: the owner accepted BPR-1245-workshop-v001 at hand-back 869 (session cbd48b91000b00f1e72e1ad62c7c384c, decision 65482b51-2b17-4824-8cad-1d1ca17efcf4): a workshop press first records the draft as an immutable reviewer revision in Private history and asks with it as the only selected source. Not built yet; engineering pending."}},{"criterion":"The card names each course's cause","outcome":"pass","revision":"b2fd518bd","evidence":{"kind":"observation","detail":"Resolution table names f55e39e72 (right-side drafts, mutable context refusal), 8790cdc7b (28-pixel Dif), 9b71342a0/1d84e59c2/f55e39e72 (launch-path count) and the private-review promotion defect fixed on BPR-1246; the workshop press cause is named and parked."}}]
---

# BPR-1245: Fifteen agent composition courses predate RFC-BPR-0023's right-side drafts and private runs

## Observation

At `main` 83edd361b on macOS arm64 (dev preset), these rows fail; the
BPR-1211 implementer recorded the same set at its baseline d25f17b69, and the
Windows station records two of them on [[BPR-1139]]:

| Rows | Failure |
|---|---|
| `application_agent_chat_composition_contract` | `agent-chat-composition-failure:chat-reply-promotion-target` |
| `application_agent_chat_stale_contract` | `panel-status:Not started: side chat refused: session_stale (head_moved)` |
| `application_agent_chat_{continued,anchored_continuation}_contract` | exit 1, no failure marker |
| `application_agent_finding_{composition,full_agent,finding_host_construction_cut,workshop_press,workshop_construction_cut,mcp_construction_cut,chat_panel_construction_cut,queue_construction_cut,dif_context}_contract` | `local-draft failed=draft-shown draft-shown=false` |
| `application_agent_invocation_{composition,workspace_diagnostic}_contract` | `Choose immutable revisions or findings in Private history, then Ask agent using selected sources.` |

## Diagnosis so far

Two product rules from f55e39e72 ("Finish BPR-0587", RFC-BPR-0023, closed and
owner-accepted) are enough to explain most rows, but each course still needs
its own reading:

- A new draft is offered only on a right-side line. The finding course reaches
  its line with `navigate_change(next)`, and the reversed or first change can
  be a left-side line, so no draft appears. [[BPR-1238]] met the same rule in
  the defect probe.
- `launch_agent_invocation` refuses a turn whose context includes notes or a
  draft; private runs now choose immutable revisions in Private history. The
  invocation courses include them.

The chat courses' promotion target and stale-session refusals were not
diagnosed. Bisecting as BPR-1238 did (a worktree build of the app between
beddf1f55, where the composed rows passed, and d25f17b69) will tell whether
they share the cause.

## Outcome

Each course exercises the accepted RFC-BPR-0023 flow: a right-side line for a
new draft, and a private run from selected immutable sources where it asks an
agent. Any product defect found on the way is fixed where it lives, not
relaxed in the course.

## Direct acceptance

- The fifteen rows pass on macOS.
- The card names each course's cause.

## Resolution (red-test backlog milestone, macOS arm64, 2026-09-30)

Fourteen rows pass. The fifteenth, `application_agent_finding_workshop_press_contract`,
needs an owner decision and is parked as an attended item (below).

The causes were diagnosed by instrumenting each course's failing step rather
than by a worktree bisect: each one reduced to a named commit, and three were
the rules already recorded on the card.

| Rows | Cause | Change |
|---|---|---|
| Finding courses (all nine: `local-draft`) and the chat composition course (`chat-reply-promotion-target`) | f55e39e72 (BPR-0587): a new draft only on a right-side line. `navigate_change(next)` lands on the first change's first left line, left:45, a deletion | New `tests/end_to_end/right_side_change.hpp`: from the selected change on, select the first change's right side, as a reviewer does before writing a comment |
| Chat composition, after its promotion | The product defect fixed on [[BPR-1246]]: promotion refused while the private review accepted the draft later | The course waits for the promoted draft and the Comments tab |
| All four chat rows (`oneLaunchPath`) | The course counts `dispatch_agent_invocation(` in `github_application.cpp` as its one-launch-path tripwire: 1 declaration and 3 calls. Three gestures joined since: tour writing hooks and their saved-hook test (9b71342a0, 1d84e59c2, writing-only) and RFC-BPR-0023's private run from selected sources (f55e39e72) | Reviewed: all three reach `launch_agent_invocation` and its single `AgentHost->dispatch(`, which stays pinned at one. The count is now 7, with the gestures named |
| `application_agent_finding_dif_context_contract`, `application_agent_invocation_workspace_diagnostic_contract` | 8790cdc7b reduced the corner Dif to one status row, 28 pixels, and rejected 40; both courses still expected 40×40 | Both expect 28×28 |
| `application_agent_finding_dif_context_contract` (acknowledge) | Dif acknowledges when the accepted revision's draft command is accepted, which with private review is on the private review's answer | The course waits for the acknowledge clip |
| `application_agent_invocation_composition_contract` | f55e39e72: the launch panel's mutable note and draft fields are refused with directions to Private history, and private-run admission accepts only immutable sources (`invalid_provenance`). The course selected note #246 and the draft | The course keeps its canaries and first shows the refusal as a negative control (nothing starts). It then writes one reviewer revision in Private history, selects it as the only source, and asks the external-terminal agent with **Ask agent using only selected sources**. The delivered context must equal the expected fields exactly (164 fields): `private.count` 1 with that revision's identity, digest and body, the question as `chat_message`, `note.count` and `draft.count` 0, and none of the note or draft canaries. A mutated expected body fails the row |

### Parked: the comment workshop

`application_agent_finding_workshop_press_contract` presses the draft editor's
Clarify control (RFC-BPR-0012's comment workshop, BPR-0533). Since f55e39e72
every press is refused with "Choose immutable revisions or findings in Private
history, then Ask agent using selected sources." The press sends the draft on
screen as its context, and RFC-BPR-0023 refuses that twice by design:
`launch_agent_invocation` refuses mutable note or draft context, and the agent
host requires every non-writing run to carry a private-run admission, which
refuses a draft as `invalid_provenance`.

RFC-BPR-0023 says it does not replace RFC-BPR-0012, which owns the workshop,
so the six controls are offered but can never answer. A candidate that
exempted the press was tried and withdrawn: it would need the host's
admission invariant relaxed, which is RFC-BPR-0023's provenance rule, not a
course detail. The owner question is attended item `BPR-1245-workshop-v001`
in session `cbd48b91000b00f1e72e1ad62c7c384c` (no server started).

Published: `git pull --rebase` renamed the milestone commits. `b2fd518bd` above is `db162ddbd` on main and `1dbf9d5ca` is `68599e10c`; the attended item now names `db162ddbd`.

## Linux: bpr-main-linux, 2026-10-01

At `db021262a`, `application_agent_finding_workshop_press_contract` failed in
bpr-main-win's BPR-0905 Linux full suite and again run serially in 46 s:
`agent-finding-composition-failure:agent-run failed=run-quiescent process=0
panel=Not started: Choose immutable revisions or findings in Private history,
then Ask agent using sel…`. That is the parked refusal, so Linux waits on the
same owner decision (`BPR-1245-workshop-v001`). The other fourteen rows did
not fail in that run.

## Owner verdict at hand-back 869 — 2026-10-03

The owner accepted `BPR-1245-workshop-v001` in session
`cbd48b91000b00f1e72e1ad62c7c384c`, handed back at event 869:

| Candidate | Fingerprint | Decision | Verdict |
|---|---|---|---|
| `BPR-1245-workshop-v001` | `b794d310…73a0` | `65482b51-2b17-4824-8cad-1d1ca17efcf4`, saved 2026-10-03T05:23:04Z | Accept, no note |

The accepted proposal, as the item asked it at `db162ddbd`: "pressing a
comment-workshop control (Clarify, Shorten and the rest) on a draft first
saves the draft's current text in Private history as an immutable reviewer
revision, then asks the agent with that revision as the only selected source.
The answer stays a proposal on the draft until you accept it, as before."
The six controls, the rule of one turn per press and the proposal/accept step
stay. Each press adds one reviewer revision to Private history, holding the
draft's text at that moment and shown in History like any other.

**This is an owner decision to build, not an acceptance of built behaviour.**
The item said "Nothing is built for this yet". No commit since `db162ddbd`
names the workshop, and `src/app/src/github_application.cpp` still carries the
refusal at `21f6e6461`. The row was not rerun for this record: the press is
refused, and `application_agent_finding_workshop_press_contract` stays red. Implementing the decision is engineering in `src/`: the press
path through the Compose page and the application host, the private-history
revision write, and the private-run admission with that revision as the only
source. The course must then be brought to the accepted flow. RFC-BPR-0012's
workshop text may need a note recording that the workshop runs as a private run
from a recorded revision under RFC-BPR-0023.

Unmet: the first criterion, "The fifteen rows pass on macOS" (14 of 15), and
Linux shows the same failing row. The second criterion, naming each course's
cause, passes. The card stays Parked until the build is dispatched. No human
question remains open on it. `BPR-1245-workshop-v001` is released by candidate
name. Its Accept stays recorded.
