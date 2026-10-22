# RFC-BPR-0090 checkpoint: M1 to M4 and Carry built; the closeout waits on BPR-1733's live half

Assignment: record the owner's answers to RFC-DRAFT-0025 (session
`e5387830d18edd8d541b96e47ecbd9cb`) and the chat clarification of Q1, revise
the design so only the selected lines are editable, adopt the RFC, split it
into cards, and implement M1 to M4, from the RFC implementation coordinator.
Branch `worktree-agent-ab2406ef642d68509`, rebased onto main
`0986da0ff`. The card IDs are the coordinator's allocation in `0986da0ff`
(BPR-1726 to BPR-1733). The one new provisional ID is `BPR-1734`, for
the coordinator to allocate.

`frontend-design:frontend-design` was loaded before the design frames v2,
the pane, card and Finish your review presentation, and the read-only
refinement, which all keep the product's own palette.

## Result

| Item | Status | Where |
|---|---|---|
| Owner answers Q1 to Q5 and the Q1 clarification | recorded | [[BPR-1723]], RFC "Owner decisions" |
| RFC adoption | RFC-BPR-0090, In Progress, indexed | `f0e03c9be` |
| M1 core | done | BPR-1726 |
| M2 Qt pane | done | BPR-1727 |
| M3 drawer, hold and Convert, lifecycle | done apart from Carry | BPR-1728 |
| M4 live course, CRLF measurement and the owner's look | done: AC1 `dbc6c0f71`, AC2 `ddc878a47` | BPR-1729 |
| The read-only code set back (owner's drawer-link note) | done, `d2db849ab` | BPR-1729, RFC "The editable range" |
| One-pane stray highlight (owner's one-pane note) | done, `ad9dbb5f1`; introduced by `d7d95ed0d` (BPR-1533) | BPR-1734 |
| Carry and Copy change at a newer head | built and passing on fixtures, `fc30dfbdb`; AC1's BPR-Test half open (the live harness writes no draft) | BPR-1733 |
| Closeout | reconciled: AC01 to AC10 pass, AC08 on fixtures; not closed while BPR-1733 is open | BPR-1730 |
| Follow-ups | stacked two panes (Q4 note), a durable file-comment subject | BPR-1731, BPR-1732 |

## Sessions and resources

- Look `7dba4a1f1dbf08a0ad93da313b35fbcd`: closed, host stopped.
- Wizard `882aa75a-b47d-49a0-ab01-340f0ca80646`
  (`/Users/jaeger/.mask/wizard/rfc-0090-commit-suggestions-v001`): complete,
  host stopped through its script.
- BPR-Test #123: closed by the course's `cleanup`, both branches deleted.
- Frozen root `/Users/jaeger/bpr-rfc0090-in-pane-v001.t2govxlb`: removed.

## Next action

The coordinator decides BPR-1733 AC1's BPR-Test half: an owner wizard on a
frozen build (write a suggestion on a BPR-Test pull request, the agent pushes
a commit, the owner carries it), or reading the card's criterion as the
RFC's AC08, which names no live run. Then BPR-1733 closes, BPR-1730 records
it and sets RFC-BPR-0090 to Done.

Outside this RFC: `application_start_review_contract` fails on main since
`7a00adb52` (its probe expects "Review was not published.", the workflow
says "Not published: GitHub refused this review.").
