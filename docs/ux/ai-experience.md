# Explicit AI Experience

## Architectural interaction contract

The functional core decides whether an explicit AI action is legal, what local context scope is selected, and whether a result is current or stale. The host launches and communicates with the separately authenticated agent. The Qt adapter presents context, approvals, chat, findings, and promotion controls. Agent output re-enters as typed events and cannot mutate GitHub or the UI directly.

## Entry points

- **Ask** (RFC-BPR-0128): one Ask control at three anchors, the review bar,
  the open file's tab and the open thread's foot beside Reply, and one Ask
  item in a thread's and a file tab's right-click menus. Each menu lists the
  quick asks of its scope (a suggested prompt with Quick ask ticked and a
  Thread, File or Pull request scope), decided when it opens: a quick ask
  whose subject is absent is not listed, and one that cannot run yet is
  greyed with its reason. Custom question… opens the Agent tab on the
  subject and sends nothing; Edit Quick Asks… opens Settings at the prompts.
  The agent controls these replace (Ask agent and Was this addressed? in a
  thread's header, Explain this file in the file's header) are gone.
- Reports (RFC-BPR-0128): an answer that is a report (What changed since my
  review, Explain this file and Was this addressed? about several threads
  always; any other by its shape) is a card where it was asked, its summary
  and an attachment row, and opens as a tab in the files' tab strip. Where
  the report labels findings, their counts stand between the two as a donut
  with a legend, and a slice or a count opens the report at its first
  finding (owner, 2026-11-25: "I'm thinking that I want to move the report
  findings donut & counts above the attachment"); Tab goes through the
  counts to the attachment. The
  tab draws a report that keeps to its template (docs/design/report-templates-v1)
  as a page: verdict rules, a tally, places that open the code, the Agent
  tab's code boxes and Suggested change with Add as draft comment, in the
  theme's report colours (Settings › Appearance › Reports). A floating menu
  lists its sections; View source and Copy source keep the Markdown.
- A review as a report ([[BPR-2219]], docs/design/review-report-v1): Review
  this pull request and Review {file} answer as a report that opens with a
  verdict (Needs changes, Ready with comments, Ready to merge) over its
  summary, then its findings under Must fix, Worth a look and Good, each at
  its place in the code. What the agent said while it worked (the words
  between its tool calls) is not part of the report: the report starts at
  its title. Every finding has one keep action at its foot, the core's
  choice (`core::report_findings`, by `core::decide_keep`'s rule): **Draft
  comment** where it names lines, a private draft there that starts from
  the finding's label, its reasons and its suggested change (or from the
  comment the agent suggested for that place); **Keep local** where it
  names only a file or nothing. Once made it reads Drafted, or Kept, with
  **Go to draft** or **Go to comment**, and a report opened again still
  says so. **Keep summary**, under the summary, keeps the verdict, the
  summary and a line a finding as a Local comment on the pull request.
  Nothing is published until the reviewer publishes.
- The report's links answer the pointer as buttons do (owner, 2026-11-25:
  "I cant actually click go to draft properly until I deselect it and then
  click it again"): a press on a link starts no selection, and one click
  follows it whatever the page has selected. The link the keyboard is on
  wears the window's focus ring, round the whole button of a keep action,
  and is never a text selection. Tab and Shift+Tab move it, Return follows
  it, and a keep action made with Return hands it to the **Go to draft** or
  **Go to comment** left in its place. One made with the pointer hands it
  to nothing, and a press of the pointer on the page ends it.
- The report is drawn in the app's own hand (look review-report-v001's
  hand-back; docs/design/review-report-v2, Cards): the application's font
  and the pane's own ground and ink, the opening on a bordered band with the
  verdict as a filled mark, each finding a bordered box as a conversation
  card is. The theme's report roles colour only verdicts.
- A report has one link colour: the page's own blue, which
  `core::derive_report_shades` derives from the pane's ground and holds to
  4.5:1 on it (in the built-in themes `#1B5D8C` in Light and `#9CC3DF` in
  Dark). The question under the title, its speech bubble, a place, a link in
  the text, the menu's bar for the current section and the links of a report
  drawn as Markdown all wear it. The platform's link role, which no theme
  owns and which is a brighter blue, is not used on the page (owner,
  2026-11-24: "Can we have the softer blue for the question link too on
  reports"). A theme change redraws the page shown.
  `qt_agent_report_view` covers it in Light and Dark.
- A turn sent from a prompt shows the prompt's name as the user's turn, with
  **Show prompt** to see what was sent (look review-report-v001: "All of
  this extra context from a prompt we select should be hidden").
- A review prompt says what it asks ([[BPR-2219]], owner 2026-11-16): the
  shipped Review this pull request and Review {file} carry their real
  instructions as their body in Settings › Prompts, where they are edited
  like any prompt: what to review, the focus, the sections, the severity
  scheme, the verdict, the tone. Restore built-ins… restores them. Under
  the editor of any prompt that asks for findings, **Result: Findings
  report** shows, read-only, what is always added: the report format
  BetterPullReview reads (the title, the verdict line, a finding's place,
  the suggestion block, the findings block). It is the application's, so an
  edited prompt cannot break Draft comment; a report without an optional
  part (no verdict, other section names, another severity scheme) still
  shows and still drafts.
- **Writing style** ([[BPR-2264]], owner 2026-11-18: "a feature that is
  built-in but editable"). One Feature, `/bpr:writing-style`, sets how the
  agent words everything a person reads: Agent tab answers, reports and
  suggested comments, the comment workshop, the returning questions,
  Explain this file, Suggest the change, every tour run and tour chat,
  Enhance, and a local draft's description, discussion, commit message and
  check. Every such request carries it after its own instructions and
  format, which win where they disagree; in a JSON answer it governs only
  the text inside string values. It is edited in Settings › Prompts, under
  Features, like a built-in prompt (Changed from built-in, Reset to
  built-in, the version it was written against), and travels in the settings exchange as
  `feature.writing-style`. Its text is sent as written, so it can simply
  point at a skill installed for the agent. A prompt or skill that names
  `/bpr:writing-style` still sends it once.
- **Comment labels** ([[BPR-2270]], owner 2026-11-18: "Maybe the comment tag
  is another editable feature defaulted to our conventional comments"). The
  second editable Feature, `/bpr:comment-labels`, says how the agent labels
  each comment it writes for the reviewer to post: suggested comments in
  reviews and reports, replies, and the comment workshop's drafts. Built in,
  it is the Conventional Comments convention, with a weight decoration only
  where it differs from the label's. It is edited under Features beside the
  Writing style and travels as `feature.comment-labels`. Cleared, it is kept
  blank and sends nothing, so comments stay plain (owner: "could be a user
  just blanks that out because they want all plain comments"); Reset to
  built-in brings the convention back. The app never adds a label of its
  own: Draft comment, Draft all, Keep local and Draft comment on an answer
  keep the agent's text, with a catalog label read into the label control
  and any other label left as written. The app recognizes Conventional
  Comments labels for its label controls and blocker count.
- A drafted suggestion carries a `(blocking)` or `(non-blocking)` only where
  it adds to the label ([[BPR-2264]], owner 2026-11-18: "They should only
  appear when they add value over the tag"). An `issue` blocks and a
  `suggestion` or `nitpick` does not, so `issue (blocking)` drafts as
  `issue:` and `suggestion (non-blocking)` as `suggestion:`; `suggestion
  (blocking)` and `issue (non-blocking)` stay. A `todo`, `typo` or `chore`
  does not block either ([[BPR-2276]], owner 2026-11-18: "nothing is a
  blocker unless we don't want the PR to merge without it fixed"), so
  `todo (blocking)` stays and `todo (non-blocking)` drafts as `todo:`. The review prompt and the
  comment workshop's guidance ask for the same, and the reviewer can still
  add either in the label toggles.
- Ask about selection
- Analyze current file: **Explain this file** (RFC-BPR-0117, [[BPR-2076]]).
  Its subject is the file open in Review, never a selection: the control at
  the right of the comparison's header and Explain This File in a tab's
  menu. The question carries the file's change in the pull request and its
  text at the head; the rest of the change is context. The answer shows in a
  card under the file's header, its references links, with Keep local (a
  Local comment on the file whose first line names the agent), Continue in
  Agent tab and Dismiss, and is a turn in the Agent tab's history either way.
  RFC-BPR-0128 amends both: Explain this file is a quick ask in the file's
  Ask and the file tab's menu, and its answer is a report card and report.
- Analyze latest upload: **What changed since my review**
  (RFC-BPR-0116, [[BPR-2073]]). Offered when the pull request has commits
  newer than the reviewer's last submitted review or Seen, in the Pull
  Request menu beside Seen and among the Agent tab's suggestions. Its
  question names both revisions and carries the change between them; when
  the earlier revision cannot be read the control is unavailable and says
  so, and nothing else is sent in its place.
- Evaluate proposed fix: **Was this addressed?** (RFC-BPR-0116,
  [[BPR-2074]]). Its subject is a review thread the reviewer commented in,
  never a selection; it is offered once the pull request has commits newer
  than the reviewer's last comment there, on the thread and among its
  conversation's prompts. The answer leads with Addressed, Partly, Not
  addressed or Cannot tell and is shown as the agent's claim: it resolves,
  verifies and posts nothing. RFC-BPR-0128 amends it: it is offered on any
  review thread once the head is past the commit the thread started on, and
  on a file and the pull request as one ask carrying every thread, answered
  as a report with a claim a thread; each claim is a chip in its thread's
  header. A merge of the base branch in What changed is one line with its
  commit count.
- Workshop comment
- Run AI review
- Open full agent session
- Ask about a conversation ([[BPR-1773]]): **Ask agent** on a review
  thread (its header and its menu) or on my own unsent comment, Local or
  Draft (RFC-BPR-0127, recorded in Private history by one path), opens the
  Agent tab with the conversation as a chip in the question field. The chip
  stays for follow-ups until its close button removes it, the prompts are
  about the conversation while it is there, and each turn's asked line names
  the conversation above the question ("About src/cache.cpp line 42, 2
  comments", then the question). Nothing is sent until the reviewer asks.
- One keep action on any answer ([[BPR-1773]], owner look 2026-10-27;
  RFC-BPR-0127): every answer's footer has Copy and one keep action, in one
  quiet style, chosen by `core::decide_keep`. **Draft comment** writes the
  answer, or the part selected, as a private draft: the reply of the review
  thread the question was about, or a comment on the lines selected when it
  was asked. Where neither was, **Keep local** keeps it as a Local comment on
  the pull request. They then read **Go to draft** and **Go to comment**.
  Finding cards, the finding queue and Explain this file follow the same
  rule. Nothing is published until the reviewer publishes.

No opening, navigation, refresh, comment arrival, or revision update automatically invokes an agent.

## Visible session contract

The AI drawer always shows provider, Git host, workspace, selected epoch pair, exact head OID, supplemental context chips, and stale/current state.

Expanding from selected lines to an entire file, PR, or repository requires an explicit context action. Private notes and unpublished drafts are opt-in per session.

## Follow-ups while an agent works (RFC-BPR-0126)

The Agent tab, every tour part's chat (a stop, Problem, Solution), the
whole-tour conversation and a local draft's discussion take follow-ups
while their agent works. Each is its own queue, owned by its session, not
by a window: every window showing the session shows and edits the same
queue, and closing a window neither pauses nor hands it over.

- **Queue or Steer.** Settings › Agents & context › Follow-up behaviour
  chooses which one Send does while the turn runs or messages wait (default
  Queue); Cmd+Return (Ctrl+Return off macOS) does the other for one message.
  Stop stays beside Send. Queue keeps the message in BPR and sends it as its
  own turn once the current run has really ended. Steer writes it into the
  running turn on the kept agent connection (Claude after its current tool
  call, never "now"; Codex App Server `turn/steer`); where the run cannot
  take it the composer says why and the message queues.
- **The pending list.** One line a message above the composer, its subject
  and attachments as captured: Edit in place (held, never sent or skipped,
  while the editor is open), Remove, drag or Option-Up and Option-Down to
  reorder, Steer, Pause queue and Resume queue. States are words:
  Queued, Sending, **Sent to agent**, Paused, **Delivery unknown** (with
  Discard; it is never resent by itself). A released message leaves the
  list once its turn begins: the conversation shows it as the running turn,
  and it still holds the session until that run ends. A steered message
  shows in the conversation at once, as the author's, after the step it
  followed, and stays on that turn.
- **Pausing.** Stop, a failure, a usage limit, a restart and unknown
  delivery pause the queue; a restored queue opens paused. A changed target,
  configuration or authority pauses rather than retargeting.
- **Stop all.** The status bar's agent footer, beside the agent picker, has
  one Stop all while any turn runs or anything is queued: it stops every
  turn in every session and pauses every queue.
- **Cost.** Queueing and editing make no request. A steered run is one turn
  for its cost, however many native parts it took.

Known limits: a steered message keeps Sent to agent in the pending list
until its turn ends; whole-tour feedback during the first draft and a local
draft's first description draft are fresh runs, so they queue and cannot be
steered.

## Comment workshop

Operations include clarify, shorten, make actionable, phrase as a question, make more direct, and check whether the premise is supported. Since BPR-0533 those six are a typed set in the tree, offered as controls on the proposed-revision surface: pressing one starts one agent turn through the single `workshop_comment` entry point, and the proposal names the operation it answered. The controls close with a tooltip while the turn runs. Since BPR-0538 they reopen when it ends, including a turn that returns no revision; that outcome explains why no revision appeared. Results stay in the local editor until the user publishes through normal GitHub review controls.

Since RFC-BPR-0118 a seventh operation, `suggest_change`, is not a prose rewrite and is not one of the six controls: **Suggest the change**, in the Agent accent in a draft comment's tool row, asks for replacement text for exactly the draft's head lines. Its first answer is one fresh, tool-free call briefed by `review.suggest-change`, an ordinary Agent tab turn with Ask more. The core accepts the reply only when its range is the draft's, its stated current text equals the head's lines byte for byte, and the replacement is a different, bounded change GitHub can take as a suggestion; an accepted reply opens as RFC-BPR-0090's in-pane edit, unsaved, until the reviewer keeps it. A refused reply is never shown as a suggestion: the field says which check failed and the reply can be read as text.

Since BPR-1091 a workshop press also sends the profile's Review feedback guidance (Agents › AI assistance › Review feedback), but only while Settings › Comments › Comment workshop › Feedback guidance (the stored label helper; a Comment labels row until [[BPR-1597]]) is on. The shipped default is sent under its version name; a customized hook is sent as written. With the helper off the request is exactly what it was before. Under the before and after, one line says what the proposed revision changes besides wording, read the way Publish reads it, in marks: the label as its mark (a change shows the mark before an arrow and the mark after, the empty Plain mark where there is none, and an unreadable header as written under a wavy line), whether it asks a question (a speech bubble with a `?`), and the suggested change (its ring). What stays the same is faint and wordless; what changes carries one word: added, removed or changed. Each part's tooltip says it in a sentence. A plain comment reworded says "Wording only". Accepting is still one local edit; publishing, the review decision and resolving stay separate actions.

Since BPR-1245 a press runs as an RFC-BPR-0023 private run. It first saves the draft's current text in Private history as a reviewer revision that cannot change, on a thread about the draft's line, and History selects it. It then asks the agent with that revision as the only selected source; the draft itself is not sent. Each press adds one such revision. If the revision cannot be saved, nothing is asked and the controls reopen with the reason.

## AI findings

Structured findings include severity, confidence, category, path, line range, claim, evidence, consequence, and suggested comment. BetterPullReview validates the anchor against the analyzed snapshot. Invalid anchors remain unanchored rather than being guessed.

Finding actions:

- Open evidence
- Ask follow-up: a side chat beside the report ([[BPR-2322]]) whose rounds read
  the pull request's workspace with read-only tools, as a tour part's chat
  does ([[BPR-2370]])
- Draft comment, or Keep local where no line takes a draft (RFC-BPR-0127:
  one of the two, `core::decide_keep`'s choice)
- Convert to local draft defect
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

The saved native conversation an Agent tab turn resumes is pinned the same
way. When the head has moved, the tab offers the choice before a question is
sent, also for a conversation with no runtime session to restore (a
transcript replay, or one whose turn ended before the agent named its
session). The core's `offer_pinned_conversation` decides: a conversation of
this pull request that was never used moves to the head under review without
asking (a new conversation takes its place); any other pinned conversation
offers Start a new session and Continue at its own head ("Continue at
1a2b3c4"). Start a new session there is History's New session, under core's
one rule for it (`decide_new_session`, which never calls a session pinned to
another revision the new one): at once, and with nothing sent, the tab shows
an empty unnamed session at the head under review, ticked in History, and the
conversation left behind stays in History with its name and turns. The
card's choices wrap onto further rows in a narrow drawer, and a choice whose
words are a sentence takes a second line; none is cut. A turn that still
reaches a pinned conversation is refused before anything runs, and the
stopped card shows those buttons ("Continue at 1a2b3c4", "Start a new
session") in place of Resend; the refusal's words name exactly them, and
each sends the kept message. Elsewhere the stopped card's Resend sends the
kept message again (owner, 2026-11-24).

A conversation the agent can resume, whose complete workspace is kept,
follows the head instead, like a `git pull` in another terminal (RFC-BPR-0096,
owner continue-at-latest amendment, 2026-11-24). Sending a question moves its
workspace to the head under review, the agent is told what moved, and the
turn's reply card says "Updated to 1a2b3c4 — 3 commits". Uncommitted edits or
commits of the conversation's own stop the move: the stopped card then offers
Rebase onto the new head, Keep my changes on a branch and move, and Ask the
agent to reconcile (which sends a built-in prompt after the move), before
Continue at the old head and Start a new session. A rebase that conflicts puts
the workspace back as it was and names the files.

The pull request's sessions share one checkout, and it moves for all of them
(owner, 2026-11-24: "a session in the same PR? yes single checkout"). The
other sessions are not woken: nothing is sent to them ("The other sessions, we
dont wake them up and send them messages"). Opened later, such a session shows
a quiet line under its continuation mode, "Updated to 1a2b3c4 — this pull
request's checkout moved from 9f8e7d6 while another session asked", and its
next question tells the agent, once.

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
