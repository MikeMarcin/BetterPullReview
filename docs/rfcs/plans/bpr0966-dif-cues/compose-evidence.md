# BPR-0966 approved Compose workshop slot

Assignment: BPR-0966 only, coordinator continuation on 2026-09-26, mandate
`r4-with-owner-compose-20260925-r2`, segment `approved-slot-r2`.
Source/tracker: `/Users/jaeger/.codex/worktrees/bpr0966-dif-cues/BetterPullReview`,
branch `codex/bpr0966-compose-takeover`, entry `fdeea1ac`.
Claim: `aacead372a65fbd207203eae903f4e842d9586f3`.
Product/test candidate: `388f814c7be796b91c4dd6779422be3d4321e4f3`.

The owner selected a visible Compose workshop slot in decision `dba44cfd`
(session `06ad5734`, 2026-09-25). AC1 is settled without removing either cue
or rewriting RFC-BPR-0031 acceptance. The later corner direction explicitly
preserves Compose as a prime placement. BPR-1209 owns that default corner;
the second integration adapts the shared Dif course's initial consider placement.

## Implementation and direct path

The inline Compose page has a fixed Comment workshop header outside its
scroll area. It renders the existing Accept revision and Reject revision
actions as keyboard-addressable buttons. Previously those actions existed
without visible buttons on this page. Escape works from the header as from
the editor. A separate 64-pixel illustration slot takes the existing single
Dif widget while the editor offers its exact matching proposal, including
after local acceptance. The editor's existing Accept revision eligibility
also removes the illustration on draft mismatch or conflict. The proposal
header and its semantic controls survive hidden-art preferences.

Rationale: keeping the decision and illustration outside the scrolling
before/after leaves acknowledgment visible when the reviewer accepts after
reading the proposed text. No duplicate draft/session state was added.
The existing cue router still checks the complete accepted session, action
generation, draft/revision and exact proposed bytes. No router, host, core,
asset or publication behavior changed. Returning to Compose starts at rest;
there is no replay. The default drawer placement is retained pending BPR-1209.

The public course opens a fixture PR, obtains an actual canary finding,
converts it to a proposal, and clicks the visible Accept revision button
after scrolling the editor to its bottom. It observes actual Paint events
for consider, ready and acknowledge. Assertions bind the exact original
draft, accepted draft ID/body, one acceptance intent, one retained sprite,
fully exposed proposal/accepted workshop slots, neutral return, hidden/reduced
preferences, rejection without editing, and all three zero-publication counters.
The focused Qt placement selector also covers the existing Compose
navigation/keyboard courses, prior Home/drawer placement, Escape from the
new header and refusal to retain the illustration for a different draft.

## Verification at the candidate

macOS arm64, Debug, AppleClang 21, Qt 6.11.2; offscreen Qt, serial,
network-isolated fixtures. No native human, Windows/Linux, installed release,
live account, paid model or remote mutation evidence is claimed.

- Failing before: the rebuilt `aacead37` baseline (source equals `fdeea1ac`)
  fails only ready and acknowledge in 20.57 seconds. Diagnostic remains
  `1:1:1:0:0:1:0`; both editor states are visible, both Dif states hidden.
- Final focused behavior: 6/6 pass. `qt_dif_workshop_placement` 1.43 s,
  `qt_dif_runtime` 1.53 s, `qt_review_draft_editor` 0.50 s,
  `application_agent_finding_composition_contract` 16.23 s,
  `application_agent_finding_workshop_press_contract` 15.66 s,
  `application_agent_finding_dif_context_contract` 19.48 s.
- The same integration batch's additional `architecture_boundaries` fails
  on the unchanged `src/adapters/mcp/src/mcp_terminal_diagnostic.hpp`
  Windows containment-API ownership rule. An isolated archive of `aacead37`
  reproduces that exact failure at verifier lines 1656/1756. Neither file
  changed here. The 7-check aggregate is therefore 6 pass / 1 baseline fail,
  56.70 seconds, not an all-green architecture claim.
- Scoped `clang-format --dry-run --Werror` and `git diff --check` pass.
  Self-review covered widget reparenting, callbacks, exact action eligibility,
  focus/Escape, scroll exposure, preference independence and retained cue guards.
- Documentation gate passes with the installed implement-rfc scripts on
  `PYTHONPATH`; its regression suite passes 64/64 (2.704 s).

Commands from the source root (all builds use `--parallel 2`):

```sh
cmake --build --preset dev --parallel 2 --target test_better_pull_review_qt_workbench_window_tests test_better_pull_review_agent_finding_composition_public_path test_better_pull_review_agent_finding_canary test_better_pull_review_qt_dif_runtime_tests test_better_pull_review_qt_review_draft_editor_tests tool_better_pull_review_mcp_context_helper
BPR_DIF_WORKSHOP_RENDER_DIR=/Users/jaeger/.codex/worktrees/bpr0966-dif-cues/BetterPullReview/build/bpr0966-compose-captures ctest --test-dir /Users/jaeger/.codex/worktrees/bpr0966-dif-cues/BetterPullReview/build/dev --output-on-failure --no-tests=error -j 1 -R '^(application_agent_finding_(dif_context|composition|workshop_press)_contract|qt_dif_runtime|qt_review_draft_editor|qt_dif_workshop_placement|architecture_boundaries)$'
PYTHONPATH=/Users/jaeger/.mask/store/v1/6ad19f0fc539b9b393a02dbeff6bf0745f83ac499d7c953cba4d9236bf667832/payload/implement-rfc/scripts python3 scripts/verify_documentation.py --root .
PYTHONPATH=/Users/jaeger/.mask/store/v1/6ad19f0fc539b9b393a02dbeff6bf0745f83ac499d7c953cba4d9236bf667832/payload/implement-rfc/scripts python3 tests/scripts/verify_documentation_tests.py
```

Local ignored logs are `build/bpr0966-compose-{failing-before,candidate,
architecture-baseline,documentation,documentation-tests}.log`.
The temporary architecture archive was removed by its owning context.

## Candidate identities and remaining review

SHA-256:

- Finding course: `60ad221adab5b13736477731d3b99c0489743c424630a7b824fcc3a52ebd84e7`.
- Fixture canary: `109315a6ee7d4f519e3b49b33577d62b6794be4d7a796b59869b07c46df24ccc`.
- Proposal capture: `10ebc68a8f1a37b25c62b4a535090f2b5980d8c1a5f757d0be229f74a7b4a04b`.
- Accepted capture: `61cf2d0938db5f47f2283f4688428994cd8e9cec62f42c03fced818d0c424bf3`.

The captures are actual 1280 × 820 application renders, not mockups. The
proposal capture retains the preexisting stale conflict-status sentence;
the accepted draft, enabled action and passing identity assertions establish
the actual state. That unrelated wording was reported to the coordinator
and RFC-BPR-0052 follow-up owner, not silently altered here.

AC2's bounded implementation is verified. AC3's owner look at the new slot
remains pending as one Accept/Reject question. Frozen review copies, session
identity and the private loopback URL live under the ignored runtime paths
named in `handoff.json`; images and logs are not committed. Windows remains
explicitly deferred, including its earlier consider-cue observation. No
human or other-platform verdict is inferred from the fixture pass.
