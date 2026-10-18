# Insights window continuation

- Mandate: `insights-window-v1-r1`, implement the named pack; original pack immutable.
- Assignment: window-and-storage, BPR-1606 and BPR-1607, engineering owner `/root/insights_window_storage`. Stop at this milestone boundary.
- Baseline: `0378ab977a7ffc74e0ac5ac2f70ee37a9cf00a66`. Selection: [execution-selection.json](execution-selection.json).
- Import on main authorized by coordinator; source branch will be `codex/insights-window-v1`. No push authorization.
- Pending: implementation, verification, frozen shell and short attended review; all later milestones unstarted.
- Next: adopt plan, claim BPR-1606, implement public window and local-only snapshot.
- Commands: no task background processes. Other repository worktrees are unrelated and preserved.
