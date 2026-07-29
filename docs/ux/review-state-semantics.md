# Review State Semantics

## Architectural ownership

Receipt, acceptance, actionability, and defect-transition semantics are deterministic functional-core policy. The host owns SQLite/GitHub observations and persistence. The Qt adapter presents labels, filters, badges, explanations, and actions; changing a badge in a Qt model does not itself change semantic state.

## File states

| State | Meaning |
|---|---|
| Unseen | Current compared material has not been opened |
| Seen | Opened but not explicitly completed |
| Read | Explicitly inspected current material |
| Accepted | Explicitly inspected with no remaining personal action |
| Changed since read | Material changed after read receipt |
| Changed since accepted | Material changed after acceptance receipt |
| Needs verification | Material related to an open item changed after inspection |
| Excluded | Intentionally excluded for this material identity |

Opening a file does not automatically mark it read.

## Conversation states

Read and accepted receipts apply through a specific latest comment identity and edit version. New replies or edits reopen unread state. GitHub resolution remains a separate shared fact.

## Defects

A defect is a local classification of an ordinary GitHub review thread. Severity and category are local unless the user expresses them in ordinary comment prose.

`Code changed near anchor` is evidence for `Needs verification`, not proof of a fix. Only the reviewer can mark verified.

## Actionability order

Default next-actionable priority:

1. Newly requested initial review
2. Unread author reply or new comment
3. Fix awaiting verification
4. File changed since acceptance or last review
5. Unread current file material
6. Open required-change defect
7. Remaining ordinary unread file

The ordering is configurable but deterministic and inspectable.

## Private versus shared actions

The UI labels every action as `Local` or `GitHub` in tooltips and confirmation surfaces where ambiguity is possible. `Accept file` is local. `Resolve thread` is shared. `Approve pull request` is shared.
