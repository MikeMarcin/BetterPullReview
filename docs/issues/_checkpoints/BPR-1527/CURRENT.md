# BPR-1527 current frontier

Assignment: `bpr-1527-final-native-release-contracts` /
`final-composition-contracts`, mandate
`authorized-release-continuation-final-gates-r3`.
Sole engineering owner: `/root/bpr1525_implementer`, native
rfc-implementer / gpt-6.1-sol / xhigh. No differing effective route was exposed.
No subagents. The selected coordinator-implementer recipe and shared engineering
contract apply. BPR-1525 and BPR-1526 remain Done and unchanged.

## Source and command ownership

The worktree fast-forwarded cleanly from `e84bd7317a07d6534479460dbc34b24b76ba4634`
to allocation `26fa0b98fde716b7aefffb92da0bae7676ab3c8e`. The only intervening
tree changes were the coordinator's BPR-1527 reservation and issue counter.
The final-gate source is `d52f5bb1221e26dc00dbc9269c3ad0408b64548a`;
its production and test tree is identical to the adopted predecessor.
The assigned branch remains `codex/v021-windows-gate-fixes`.

Both final native release suites are coordinator-owned and still active.
No build, test, performance process or producer mutation is authorized until
the coordinator explicitly releases the relevant slot. This implementer has
started none. Read-only source/log investigation and local plan/card upkeep
are authorized. No review server or attended candidate is open.

## Original failures and read-only diagnosis

Mac log: `/tmp/bpr-release-021-macos-final.log`; producer:
`/Users/jaeger/Documents/Codex/releases/v0.2.1-macos`.

- Agent invocation composition, test566, 47.04s: positive-seed reaches typed-success
  with core/workspace/process/cleanup all zero. The agent panel still shows the
  deliberately tested mutable-input refusal. The later Private history callback
  passes empty note selection and no draft, so this old status alone does not
  establish a second mutable dispatch. Trace the private panel's status, workflow
  completion and launch admission before selecting a repair.
- UIforETW selected diff, test583, 2.40s: light capture reports
  `review_bar_exact_order=false`. Product BPR-1507 adds the Accept file action
  at index10 (`c9b8386ba0`); the probe still requires15 actions and private
  progress at10. Preserve the shipped control and strengthen the exact oracle
  to all16 actions, including its exact default action. Native confirmation and
  missing/misplaced-control counterexamples remain pending.
- Threaded reply, test592, 40.57s: phase5 has a draft at the exact right-side
  surface line41, but `ReplyToThread` is absent and evidence00000.
- Label authoring, test593, 40.55s: reaches phase19 with reply absent and inline
  reply editor present after completing the root-comment publication courses.
- Reply draft, test597, 40.57s: refresh course reaches phase2 with reply absent
  and inline editor present, evidence0000000.

The three reply failures share the public card action and `begin_review_reply`.
The existing-draft path retargets an edit even when its identity is an ephemeral
placeholder. Dispatch routes such an edit into `materialize_draft`, which
explicitly requires `!ChangeReplyTarget`. The no-draft path instead selects an
actual reply through the existing typed `select_reply` operation. Proposed
repair: use that operation for an exact current placeholder too, retaining all
thread/anchor/navigation/scope guards. Runtime branch proof and negative controls
remain pending. Blame places lazy materialization in BPR-1520 `7bf1baeeb5`
and the retarget path in `f55e39e726`; both precede release8f0d and BPR-1525/26.
This is provenance evidence, not an executed historical reproduction.

## Remaining work

Bind and claim the new card before product edits. Complete agent admission
diagnosis, implement only demonstrated repairs, then use the first explicitly
released native slot for the shortest affected public compositions and useful
negative controls. Focused acceptance does not replace root's final full native
gates, packages, corresponding-source/SPDX/notices or release approval.
