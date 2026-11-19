# Synthetic organization companion

RFC-BPR-0112 AC07: a stand-in for a separately maintained companion skill
that adapts an organization's own create-PR tool to BetterPullReview's
workflow contract (`bpr.workflow-request` / `bpr.workflow-response`). It
needs no enterprise account, script or service.

- `synthetic_org_companion.py` is the companion. The author runs the command
  the draft's Workflow area names; BetterPullReview never starts it.
- `upstream/` plays the installed upstream tool: `RULES.json` is its rules
  (the companion reports their SHA-256 as the upstream fingerprint), and
  `helpers/validate` is a helper it can call on its own. A
  `SELF_REVIEWED` file is the marker only the upstream tool writes.
- `synthetic-org.bprsettings` is the shared configuration: one
  workflow and the agent its description action asks.

The companion owns the organization's rules; BetterPullReview holds none of
them. Its publish action records the plan it was handed in
`published-plans.jsonl` beside the rules and reports the pull request number
in `CREATED_PULL_REQUEST` (written by whoever plays GitHub), or loses its
answer once when `LOSE_ANSWER` exists; it creates nothing. `UPLOAD_OK` makes
the follow-up's visual upload succeed.
