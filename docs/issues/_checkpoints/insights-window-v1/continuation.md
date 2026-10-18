# Insights window continuation

Mandate `insights-window-v1-r1`; assignment `window-and-storage`, BPR-1606 and
BPR-1607. Fresh post-attended engineering owner `/root/insights_first_acceptance`.
The first milestone's criteria are **accepted**. Local finalization/integration
is being completed below. No later milestone or RFC closeout is accepted.
Strategy remains coordinator-implementer, recipe revision 2.

## Candidate and authority

- RFC_BASE_SHA: `0378ab977a7ffc74e0ac5ac2f70ee37a9cf00a66`.
- Import/allocation: `6a7073240` (RFC-BPR-0086, BPR-1606–1615).
- Owned branch: `codex/insights-window-v1`, source/tracker root
  `/Users/jaeger/.codex/worktrees/3996/BetterPullReview`.
- Corrected shell source: `ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce`.
  Exact supplied HTML: 900 × 542 content, centered compact tabs/icon actions,
  fixed header, compact cards and thin Storage rows. Later data is empty states.
- Integration candidate: `4a89314cc5d4191365196e600c2f59e6546da200`, merging
  completed BPR-1616 main `f067cdb8c` without conflicts. Reviewed shell source
  and storage behavior are unchanged. Main was clean and its writer finished.
- Authoritative BPR-1616 allocation `8317ad1dc` had already been adopted as
  `7dc22dad9`; the merge preserves its receipt/counter and Done card. No new
  IDs allocated. The documentation gate caught its obsolete adopted Ready copy
  beside main's Done card; only the stale Ready copy was removed. BPR-1602's
  terminal store classification remains intact.
- BPR-1606 accepted through board finish `e88e5edcf`; BPR-1607 promoted
  `c90437d93`, claimed `db67c88c7`. Final finish/integration is recorded below.
- [Execution selection](execution-selection.json), [live ID map](id-map.json),
  [changed implementation paths](changed-paths.txt) and the
  [verification ledger](../../../verification/RFC-BPR-0086.md) preserve scope.

## Exact owner acceptance and review state

Session `e51e8b870c12f3d41b3f243b2891758a`, candidate
`BPR-1606-insights-shell-v002`, revision `ae0a40211-v002`, fingerprint
`649ab6f1674c77749d04c5695c2a1b2f3ce864869977ba766afd4619cf561d69`.
Owner event 8 is **Accept**, empty note, decision
`b1070c71-f804-48f1-8210-80d373c6fd8a`; event 9 is handback. The coordinator
verified and durably consumed cursor 9 before this engineering round. This
satisfies the shell judgment only; engineering evidence supplies the other exits.
After recording acceptance, scoped `resume-development` released the accepted
candidate's protections at event 10. **Consumed cursor 10**. Its verdict remains
intact. The exact state's `stop` command stopped review host PID 48248, formerly
foreground tool session 99620/port 65063. No host or waiter remains.

- State: `/Users/jaeger/.mask/attended-review-sessions/bpr-insights-window-v002-20261018`.
- Registry: `/Users/jaeger/.mask/attended-review-resources`.
- Script: `/Users/jaeger/.codex/worktrees/3996/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py`.
- Frozen root retained: `/Users/jaeger/bpr-insights-window-v003.an3a0rb6`.
  Source/build are inside that root; freeze and launcher `--check` passed.
- Executable SHA-256: `0ca475fa463a6dcb9d933f0c98e13784fd3ff7147fb1716b24212e5e86c982da`.
- Launch: `/opt/homebrew/opt/python@3.14/bin/python3.14 /Users/jaeger/bpr-insights-window-v003.an3a0rb6/launch.py`.
- No pending owner decision for this milestone. No session is newly prepared.

## Preserved rejection and correction history

Session `a25204cd201da1ca5dbff4439c52bda4`, events 6–7 rejected
`BPR-1606-insights-shell-v001`, revision `5a02177b3-v001`, fingerprint
`2d266983a09915e9ca589be39aadf67a9230894e0c1023b7418f46473e071bf4`:
“that looks almost nothing like the mockups /Users/jaeger/Downloads/bpr_insights_window_fixed_size_mockup.html”.
Its protection was released at event 8 before correction, consumed cursor 8.
Old session: `/Users/jaeger/.mask/attended-review-sessions/bpr-insights-window-v001-20261018`.
Rejected frozen root retained: `/Users/jaeger/bpr-insights-window-v002.osewa1la`.
Its server was stopped through its exact state, with no pattern kill.
Initial implementation `b7b651c0a8158209691d398e4d5449f61e2a9a83`; lifetime fix
`5a02177b30250494811033052b673c20a9b5a7b0`; corrected shell `ae0a402118`.
Prior guards/checkpoints: `886ac861a2`, `a41a45d1d`, `8ed4608cb`, `1bcbe9a90`.
The latter retains the full previous handoff. No rejection was overwritten.

## Verification and remaining scope

The exact HTML was rendered through disposable headless Chrome with remote
resources blocked. The owner's desktop/browser profile were never driven/read.
Prior UX correction loaded `frontend-design` from
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
All six Qt pages passed inspected Light/Dark renders and public courses at
1×, 1.25× and 2×. Frozen course passed through its registered network-denying
wrapper from writable evidence; CTest itself could not create a temporary log
in the deliberately read-only frozen tree. This limitation remains recorded.

Round 3 focused build of `test_insights_public_path` and
`app_better_pull_review` passes. Integration at `4a89314cc` passes 10/10 serial
CTest rows in 24.59 seconds: architecture, core reset, Insights host, SQLite
maintenance/reset, SQLite/Git Insights accounting, composition contract, profile
recovery and the Insights public path. No broad rebuild/matrix was run.
Unchanged passing LLVM/format 22, production syntax, manifest, architecture
negative controls, scaling and 67 documentation-regression tests are reused.
Documentation gate is rerun after final bookkeeping. No provider, credential,
mutation, deletion or new source behavior changed in this round.

The first milestone meets every bound obligation and all three exits.
Windows/Linux native UI/new-course containment, later charts/statistics, full
export privacy and the macOS idle hour remain pending/unmeasured. RFC status
stays In Progress. Later acceptance cannot be inferred from this first milestone.

Runtime round 3 files: the current session's `evidence/acceptance-r3/`, including
initial binding, exact card acceptance, integration build/tests, final binding,
observation and logical check. The BPR-1607 execution-status paragraph alone
was updated; its complete scope/acceptance bullets are unchanged. The final
binding is renewed for that bookkeeping-only source-text change.

## Ownership, integration and continuation

All prior writers/builds/freeze/checks finished. Round 3 owns only this milestone
and local integration; no delegation, live GitHub call, push, stash, owner desktop
access, automation or recurring waiter. Accepted candidate protection is released;
its retained frozen source stays read-only. Only exact task-owned commands were
managed. The original implementation build tree is retained for the next owner.

After BPR-1607 finish and the documentation gate, recheck main's clean state and
revision, fast-forward locally, then bind/check the accepted milestone against
the exact final revision. Stop at this boundary. Next plan-eligible milestone is
`github-budget` (BPR-1608 and BPR-1609); the coordinator starts its fresh owner.
The other milestones retain their authored dependencies.
