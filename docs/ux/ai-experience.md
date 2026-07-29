# Explicit AI Experience

## Architectural interaction contract

The functional core decides whether an explicit AI action is legal, what local context scope is selected, and whether a result is current or stale. The host launches and communicates with the separately authenticated agent. The Qt adapter presents context, approvals, chat, findings, and promotion controls. Agent output re-enters as typed events and cannot mutate GitHub or the UI directly.

## Entry points

- Ask about selection
- Analyze current file
- Analyze latest upload
- Evaluate proposed fix
- Workshop comment
- Run AI review
- Open full agent session

No opening, navigation, refresh, comment arrival, or revision update automatically invokes an agent.

## Visible session contract

The AI drawer always shows provider, Git host, workspace, selected epoch pair, exact head OID, supplemental context chips, and stale/current state.

Expanding from selected lines to an entire file, PR, or repository requires an explicit context action. Private notes and unpublished drafts are opt-in per session.

## Comment workshop

Operations include clarify, shorten, make actionable, phrase as a question, make more direct, and check whether the premise is supported. Results stay in the local editor until the user publishes through normal GitHub review controls.

## AI findings

Structured findings include severity, confidence, category, path, line range, claim, evidence, consequence, and suggested comment. BetterPullReview validates the anchor against the analyzed snapshot. Invalid anchors remain unanchored rather than being guessed.

Finding actions:

- Open evidence
- Ask follow-up
- Convert to local draft comment
- Convert to local draft defect
- Add to private notes
- Dismiss

## Full agent mode

Full agent mode is equivalent to launching the configured CLI/harness with PR and local context. The agent may use its own GitHub tools and normal approvals. Any resulting comments or review mutations return through ordinary GitHub synchronization.

## MCP helper

A first-party read-only MCP helper may expose the frozen BetterPullReview review projection to approved agents. It has no GitHub credential, no mutation tools, no arbitrary filesystem access, and no network listener; stdio is preferred.
