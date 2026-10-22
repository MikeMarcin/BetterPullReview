# RFC-BPR-0090 checkpoint: adopted, M1 to M3 done, M4 parked on the owner

Assignment: record the owner's answers to RFC-DRAFT-0025 (session
`e5387830d18edd8d541b96e47ecbd9cb`) and the chat clarification of Q1, revise
the design so only the selected lines are editable, adopt the RFC, split it
into cards, and implement M1 to M4, from the RFC implementation coordinator,
over main `07a9594c3`. Branch `worktree-agent-ab2406ef642d68509`, rebased
onto main `7075954df`. New issue IDs are provisional (`BPR-1726` to
`AH8`); the coordinator allocates them on main. No card's metadata carries a
`provisional_id` key.

`frontend-design:frontend-design` was loaded before the design frames v2 and
the pane, card and Finish your review presentation, which keep the product's
own palette: the write-blue for the range and the reading amber for a hold.

## Result

| Item | Status | Where |
|---|---|---|
| Owner answers Q1 to Q5 and the Q1 clarification | recorded; session handed back, released and its host stopped | [[BPR-1723]], RFC "Owner decisions" |
| RFC adoption | RFC-BPR-0090 r2, In Progress, indexed; draft kept as provenance | `f0e03c9be` |
| M1 core | done | BPR-1726, `860b7726f` |
| M2 Qt pane | done | BPR-1727, `e249fc5f6` |
| M3 drawer, hold and Convert, lifecycle | done apart from Carry | BPR-1728, `a7b8b0f6d` |
| Carry and Copy change at a newer head | Backlog: needs the host's old-to-new head correspondence | BPR-1733 |
| M4 live course | five planned suggestions accepted and read back identical on BPR-Test #123 | BPR-1729 |
| M4 CRLF commit measurement | parked: GitHub's Commit suggestions is the owner's (wizard) | BPR-1729 |
| M4 look (RFC AC10) | parked: six questions on a frozen build | BPR-1729 |
| Follow-ups | stacked two panes (Q4 note), a durable file-comment subject | BPR-1731, AH7 |

## Open sessions and resources

- Look: attended-review `7dba4a1f1dbf08a0ad93da313b35fbcd`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0090-look-v001`,
  bound to this worktree as its checkout; ready at event cursor 18. Its host
  was started for the owner; the URL is in the state's `serve.log`.
- Wizard: `882aa75a-b47d-49a0-ab01-340f0ca80646`, state
  `/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001`; URL in its
  `serve.log`.
- Frozen build: `/Users/jaeger/bpr-rfc0090-in-pane-v001.t2govxlb`, from
  `b13164f02` (this branch's `a7b8b0f6d`), executable SHA-256
  `34a0eb21f6ed5c8962405acca55c83becd4075c6054a6c85cb73e6f5860c51c0`.
- BPR-Test #123 and its branches `rfc-0090-base-20261021-182601` and
  `rfc-0090-head-20261021-182601` stay until the measurement; the course
  script's `cleanup` closes and deletes them. The script and its state are
  `/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001/course/live_course.py`
  and `live-state.json` beside it (`measure`, `cleanup`; `publish` needs a
  build of `test_pull_request_suggestion`).

## Next action

After the owner: read the look with `status --after 18`; once the wizard's
first step is acknowledged, run the course's `measure` and record it with
`record --step measure`; record AH4's AC1 and AC2; `cleanup` #123; remove the
frozen root with `scripts/freeze_attended_build.py remove`; then BPR-1733,
then the closeout BPR-1730.
