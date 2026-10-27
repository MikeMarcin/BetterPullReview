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
- Ask about a conversation ([[BPR-1773]]): **Ask agent** on a review
  thread (its header and its menu) or on a saved private note opens the
  Agent tab with the conversation as a chip in the question field. The chip
  stays for follow-ups until its close button removes it, the prompts are
  about the conversation while it is there, and each turn's asked line names
  the conversation above the question ("About src/cache.cpp line 42, 2
  comments", then the question). Nothing is sent until the reviewer asks.
- Draft comment on any answer ([[BPR-1773]], owner look 2026-10-27): every
  answer's footer has Copy, **Save as note** and **Draft comment**, in one
  quiet style. Draft comment writes the answer, or the part selected, as a
  private draft: the reply of the review thread the question was about, a
  comment on the lines selected when it was asked, or otherwise a new pull
  request comment on Overview. It then reads **Go to draft**, as Save as note
  then reads Go to note. Nothing is published until the reviewer publishes.

No opening, navigation, refresh, comment arrival, or revision update automatically invokes an agent.

## Visible session contract

The AI drawer always shows provider, Git host, workspace, selected epoch pair, exact head OID, supplemental context chips, and stale/current state.

Expanding from selected lines to an entire file, PR, or repository requires an explicit context action. Private notes and unpublished drafts are opt-in per session.

## Comment workshop

Operations include clarify, shorten, make actionable, phrase as a question, make more direct, and check whether the premise is supported. Since BPR-0533 those six are a typed set in the tree, offered as controls on the proposed-revision surface: pressing one starts one agent turn through the single `workshop_comment` entry point, and the proposal names the operation it answered. The controls close with a tooltip while the turn runs. Since BPR-0538 they reopen when it ends, including a turn that returns no revision; that outcome explains why no revision appeared. Results stay in the local editor until the user publishes through normal GitHub review controls.

Since BPR-1091 a workshop press also sends the profile's Review feedback guidance (Agents & context › AI assistance › Review feedback), but only while Settings › Comments › Comment workshop › Feedback guidance (the stored label helper; a Comment labels row until [[BPR-1597]]) is on. The shipped default is sent under its version name; a customized hook is sent as written. With the helper off the request is exactly what it was before. Under the before and after, one line says what the proposed revision changes besides wording, read the way Publish reads it, in marks: the label as its mark (a change shows the mark before an arrow and the mark after, the empty Plain mark where there is none, and an unreadable header as written under a wavy line), whether it asks a question (a speech bubble with a `?`), and the suggested change (its ring). What stays the same is faint and wordless; what changes carries one word: added, removed or changed. Each part's tooltip says it in a sentence. A plain comment reworded says "Wording only". Accepting is still one local edit; publishing, the review decision and resolving stay separate actions.

Since BPR-1245 a press runs as an RFC-BPR-0023 private run. It first saves the draft's current text in Private history as a reviewer revision that cannot change, on a thread about the draft's line, and History selects it. It then asks the agent with that revision as the only selected source; the draft itself is not sent. Each press adds one such revision. If the revision cannot be saved, nothing is asked and the controls reopen with the reason.

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

## Implementation truth

Everything above is the intended experience, written before any of it existed.
This section says what RFC-BPR-0012 actually built, so a reader can tell a
specification apart from a surface they can put their hands on.
`docs/architecture/ai-agent-boundary.md` carries the same facts as a boundary
contract, with the check for each; this section is the reviewer-facing half.

### What shipped

Three surfaces exist. `qt::side_chat_panel` is the drawer contents — provider,
Git host, workspace, epoch pair, exact head OID, session state, scope, context
chips, private-context opt-in, and continuations, each a separate field rather
than a sentence (BPR-0502, `qt_side_chat_panel`). `qt::finding_queue_view` is
the finding queue, showing severity, confidence, category, claim, evidence,
consequence, and anchor state as seven separate fields and offering exactly the
six actions listed above (BPR-0503, `qt_finding_queue_view`). The comment
workshop is not a fourth surface: it lives inside the `qt::review_draft_editor`
the reviewer already types into (BPR-0503, `qt_review_draft_editor`). All three,
plus the finding host, the workshop host, and the MCP helper supervisor, are
constructed in the shipped application and driven by real gestures (BPR-0504,
`application_agent_finding_composition_contract`, `composed_application_hosts`).

Every action is still an intent. A gesture on any of these surfaces emits one
typed value and waits; nothing on screen moves until an accepted projection
comes back. That is why a refused conversion is *shown* as a refusal rather
than looking like nothing happened.

### What the anchor states are called

Three, in the core's own spelling, and the queue shows them by name:

- `anchored` — the frozen snapshot contains exactly what the finding named.
- `unanchored` — it does not, and no substitute was looked for.
- `stale` — the finding was analyzed against a different revision entirely.

The queue prints the state and its reason together, as `state (reason)`: a
finding that landed reads `anchored (none)`, and one that did not reads
`unanchored (unknown_path)`, `unanchored (span_past_end)`, or
`unanchored (malformed)`. An unanchored finding is still listed with everything
else it says, because a reviewer who cannot see what the agent got wrong cannot
correct it (BPR-0497, `core_agent_finding`; BPR-0503, `qt_finding_queue_view`).

### What the two continuations are

`start_new_session` and `continue_against_old_revision`. They are a fixed
two-element array on the core's `agent_continuation_offer`, not a list that
could grow, and the target they continue against is the session's own binding —
there is no field on that type the head now under review could occupy. After
head movement the panel says stale in the core's own words, offers exactly those
two, starts neither, and disables Send until the reviewer answers (BPR-0499,
`core_agent_session`; BPR-0502, `qt_side_chat_panel`).

Continuing against the old revision runs the turn on that revision's files
and binding, and since BPR-0532 its findings can anchor: the comparison the
old revision was reviewed under is not kept once the head moves, so the
application recomputes it — one file, bounded in time — and the turn is
frozen against, and its findings validated against, that revision's own
identities. BetterPullReview never places old findings against the new
comparison. When the recompute is refused or exceeds its bound the turn
runs with nothing frozen and its findings arrive unanchored, which is the
behaviour the owner accepted (BPR-0521) and which remains the documented
fallback; a fresh session at the new head anchors as usual.

### What surface the workshop actually uses

Plain text, on two `QPlainTextEdit` surfaces shown at once: the reviewer's
original and the agent's proposed revision, each read from its own source.
This workshop preserves the original plain-text contract even though other
read-only surfaces now render Markdown under RFC-BPR-0036. The original choice
is recorded in
`docs/verification/RFC-BPR-0012.md` under **Surface decision (BPR-0503)** — the
comparison is between the reviewer's bytes and the agent's bytes, and a renderer
would show neither, least of all a proposal differing only by a backtick or a
leading `#`. Accepting emits exactly one draft `edit` carrying the proposed text
byte for byte; rejecting emits nothing and leaves the original byte-identical,
tab and trailing space included (BPR-0503, `qt_review_draft_editor`).

Compose gives the offered revision a fixed **Comment workshop** header with
the existing Accept revision and Reject revision actions ([[BPR-0966]]).
The workbench's one Dif widget moves there at 64 logical
pixels while that exact draft's revision is offered, and remains after the
local acceptance. The accepted proposal plays `comment_ready`; the matching
explicit acceptance plays `acknowledge`. Neither publishes a comment. Rejection,
draft replacement or a conflict removes the illustration. The header stays
visible while the before/after scrolls, and Escape returns from its controls
as it does from the editor. Leaving and returning shows
neutral rest without replaying a cue; hidden and reduced-motion preferences
retain the same draft controls. This slot is Compose's prime placement under
the owner amendment; [[BPR-1209]] separately owns Dif's default corner elsewhere.

### What is specified here but not yet built

Delivered since this list was first written, and kept here so the list is
read against the tree rather than against its history: the side-chat panel
and the finding queue sit in the conversations drawer ([[BPR-0513]]); a
send reaches one agent turn through the composed side-chat session host
([[BPR-0514]]); and the typed message rides the agent's input frame
([[BPR-0517]]).


- **The six comment-workshop operations above are controls that start turns
  (BPR-0533).** They are one typed set, `core::workshop_operation`, carried
  through the one shipped path: a press starts one agent turn under the
  single `core::agent_entry_point` `workshop_comment`, and
  `application::agent_comment_workshop_host` turns that turn's converted
  outcome into one before/after proposal that names the operation asked for.
  A turn that ends without a revision reopens the six and shows its reason
  beside the proposal area ([[BPR-0538]]).
- **The MCP helper's frozen snapshot carries alignment facts, not file bytes.**
  Its `binding` and `alignment` records are written from the same values the
  finding validator uses, and `files/` is created empty, so `list_snapshot_files`
  answers with an empty list rather than a wrong one (BPR-0504).
