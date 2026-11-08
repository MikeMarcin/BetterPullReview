# Review-tour authoring and GitHub import

RFC-BPR-0039's manual-authoring milestone adds a purpose-specific core aggregate
in `review_tour.hpp`. A draft belongs to one application profile, accepted host
fingerprint, account, repository and pull request. Its local ID and revision
are distinct from GitHub identities. Problem and Solution are independent raw
Markdown fields. Vector position defines stop order; titles never identify stops.

Each destination is one inclusive interval in an exact comparison,
side, commit and repository path, with a blob identity when known. Both endpoints
must have the same comparison and side. Semantic `source_line_ref` endpoints stay
zero-based; the author sees one-based inclusive lines. Original source remains separate from a
current mapping. Representing historical or unchanged source grants no native
comment-write authority. Unknown source permits local saving but prevents manual
completion. Observing a changed head marks stops for attention without rewriting
the authored head or original source.

Commands bind owner, draft and expected revision. Reorder retains IDs; duplicate
and split allocate from a nondecreasing high-water mark. Split operates at a UTF-8
byte boundary and preserves related sources in both results. Merge concatenates
bodies and unions related source destinations. Restore supports local structural
undo while retaining the high-water mark, so an undone creation cannot recycle an
ID. Refused commands return the original draft intact. These operations emit no
publication, viewed-file, resolution, approval or correctness-receipt requests.

The core admits up to 256 stops, 64 KiB UTF-8 bytes per text field, 32 related
sources per stop, and a 4 MiB conservative encoded-size budget for authored prose,
retained baselines, identities, paths and metadata. A destination path is at most
4,096 bytes. Bounds refuse the whole command; they never truncate prose. Incomplete
local drafts, including an empty brief or skeleton stop, are valid saved state.

Persistence, workbench composition, public navigation and remote publication are
separate acceptance surfaces. The core tests alone do not establish any of them.

The admitted profile-private `state/v1/review-tours.sqlite3` keeps database schema 1 and writes version 7
payload records under application ID `0x42505254`. The exact owner and local draft ID
form the primary key. Payloads are bounded binary records with an integrity
digest; unknown schemas or corrupt payloads refuse rather than pretending that
the draft is absent. At most 128 drafts are retained with no automatic eviction.
The existing private-root admission, hardened connection, WAL and full synchronous
commit policy apply. Saves compare the expected revision inside one transaction.

The application host coalesces edits while a save runs off the decision lane.
A later acknowledgement never replaces newer editor text. Orderly shutdown
drains the last accepted edit. A failed save remains visibly unsaved and requires
an explicit retry; a conflicting save preserves both copies and never overwrites
the other writer automatically. Local structural undo keeps at most 16 snapshots
within 8 MiB per history direction; native text editors own text undo. Editing text
clears structural history so a later structural undo cannot erase a newer edit.
Qt routes standard Undo/Redo in the stop editor and outline to that history while
available, then to native text editing. Each structural history entry also
retains the current stable stop; a selection revision distinguishes a fresh
Undo/Redo selection intent from an ordinary save acknowledgement. Right-click
retains an existing selected group or selects the clicked stop. Ordered stable
ID sets extend the existing core move, duplicate, merge and remove commands;
validation refuses an invalid set atomically. Merge requires adjacent stops.
An internal drag from the dotted handle emits one revision-bound semantic move;
only the accepted projection reorders rows. Tab and Shift+Tab explicitly traverse
visible enabled controls, including assistance buttons, in all three prose fields.

Authored text and bounded raw remote baselines are explicit private retention
classes, outside the metadata ledger. Baselines retain raw body, source span,
remote identity/version and local generation; they confer no current remote
truth or mutation authority. Profile purge includes the exact tour store and
admitted sidecars. Local loss can lose unsent work; it never deletes shared
GitHub comments. Cold import uses the published objects described below; remote
update recovery belongs to the publication and maintenance milestones.

Prepare review tour is available from Overview and the Pull Request menu
(`Alt+T`). It opens the Tour page alongside the current Comments and Notes tray.
Problem and Solution use independent native plain-text documents; coaching never
becomes source. Their safe Markdown preview states its HTML/disclosure limits.
Section changes and preview perform no remote operation or agent launch.

The composition binds the store to the admitted application data root and exact
accepted owner. Identity invalidation clears the displayed copy. Pending or failed
private copies remain associated with that owner and can be resumed by reopening
the pull request. At most eight such suspended copies are retained per window;
further new tours are refused with a recovery explanation rather than evicting
unsaved prose. Clean saved copies are reconstructed from the private store.
Merely opening a PR does not consume a durable tour slot: an untouched empty
seed is observed and discarded without a save.

`Add tour stop` is in the source context menu and Pull Request menu
(`Alt+Shift+T`). The Qt surface supplies real source-line endpoints. Composition
checks the exact accepted endpoint provenance, owner fingerprint, commit, blob
and repository path before inserting a stop. Unified selections spanning both
sides are refused rather than converted from painted rows into a false range.
Related source capture uses the same admission and stores no credential-bearing
fetch destination.

Since [[BPR-1593]] (RFC-BPR-0085 D4) the Tour tab is the route: the brief as
one card whose Problem and Solution rows open in place, then the stops as
numbered rings on an amber line, and the current stop's title, source,
explanation and actions written in place on the route. Since [[BPR-1936]] no
file chip divides the route (each stop's own chip names its file), and the
open stop's card collapses and opens again by its number, its header's empty
space, Space on the route or Return on the number; Problem, Solution and Stops
collapse by their headers the same way, and the tour does not scroll. A stop is
edited as the overview's description is: the pencil hides, the title is a
wrapping editor that stays whole, nothing else sits on the title row but the
trash and the chat toggle, and **Cancel** (Esc, the text as the editor opened
on it) and **Save** (Command/Control+Return, the default) sit at the card's
foot. **Use selected lines** is a quiet link offered only while the stop is
written. The card's trash and every stop's menu remove that stop; Undo brings
it back. Readiness reads from the ring: a green tick for Ready, an amber
number for Editing, a warning on the ring for Needs attention, a dashed ring
for Suggested. **Expand** opens the tour page in the main pane, with no
drawers: the brief and outline at the left, the stop with its code excerpt
under the amber bracket in the middle, and what GitHub will show with the
publication controls at the right; **Back to the code** (Esc) returns. Both
places are the same controls, moved, so the model, publication and recovery
below are unchanged. The section stack keeps its four pages (Problem,
Solution, stops, preview) with no visible tabs.

Since [[BPR-1878]] (owner looks v001 to v003) the tab ends with the Agent
tab's bottom bar: the same rounded field, inset as the Agent tab's question
field, holding the Agent tab's own agent picker on the right. The draft's
state and Preview and publish sit just above it; nothing is below the
picker. It is a second `agent_run_settings_picker` over the
Agent tab's `agent_run_settings_source`, so its pill ("agent · model ·
effort"), its menu and its choice are the Agent tab's, and choosing in
either shows in both. Its Model and Effort menus are the Agent tab's too,
and a tour drafting request runs with the chosen agent's model and effort
(`--model`, `--effort` for Claude Code), as a question in the Agent tab
does. As in the Agent tab it is hidden while no agent is chosen; a helper clicked then says why in the tab's notice (no agent set
up, none chosen, not this window's draft, or the agent busy). Each field's
helper uses RFC-BPR-0043's labels: **Draft from PR context** on an empty
Problem or Solution (the description's own section when it has one,
otherwise the agent's draft), **Enhance problem statement**, **Enhance
solution** or **Enhance comment** on written text, **Enhance selection** on
a selection, and **Generate comment** on an empty stop. **Copy selected
description text** copies the selection in the description's Source view
into the field. Moving on is **Next: Solution** or **Next: Stops**, on its
own row after the helpers. No notice names a control the tab lacks.

Since [[BPR-1881]] (owner, 2026-11-06) the tab follows the Agent tab's own
setup verdict, the core's `decide_agent_suggestions` that also holds the
suggested prompts back (`agent_tab_view::setup_readiness`). While the setup
is unfinished the tab's top says **Drafting help needs the agent.** with
**Set up the agent**, which shows the Agent tab at its setup's first
unfinished step, as its **Finish setup** does. Nothing opens the setup by
itself. The agent features (the three drafting helpers, **Suggest an
outline** and its insert, and the bottom bar) wear one agent accent,
`agent_accent` in `bpr/qt/agent_feature.hpp`: a deep indigo in a light
window and a soft one in a dark window (owner look v001: not red, which
reads as an error; clear of the link blue and the fixed violet), each at least 4.5:1 against the
window and base colours, with the agent's spark beside each button's words
so colour is not the only sign. They keep it after setup. Until the setup
has finished the helpers and the outline's buttons are disabled, and their
tooltips say the agent is not set up and where to set it up; the picker in
the bar stays usable, since it runs nothing. Showing the Tour tab lets the
setup look for agents, as showing the Agent tab does (RFC-BPR-0092 Q2), so
the tab can tell which state it is in.

Since [[BPR-1892]] (owner, 2026-11-06: "make the tour error a) stand out
more and b) send you there") a drafting request whose agent run fails says
so on the Tour tab, right under the control that made it (the Problem or
Solution helper, the stop's helper, or **Suggest an outline**). The notice
is a red card with a red edge and a warning mark, the Agent tab's stopped
card's red: one line, the control's own words and "failed.", then the
Agent tab's words for that outcome (`agent_tab_view::run_failure_words`,
the one copy). **Show on the Agent tab** opens the Agent tab at the run's
stopped card, which keeps the whole story, its log and **Resend**; nothing
switches tabs by itself. The notice clears when its request is made again
or succeeds, and its close button dismisses it. A stop the person asked
for says nothing there.

Since [[BPR-1900]] (owner, 2026-11-06: "the tour itself is much more of a
second agent session, separate and mostly background from my main agent chat
session", and "then it can fill its tour stops and problem and solution from
all that context") tour authoring is its own agent session, separate from the
Agent tab's chat: tour runs are not chat turns and chat turns are not tour
runs. The top of the tab has **Draft Tour**, in the agent accent. It asks
the agent, in the background, to read the pull request in the review's
workspace with read-only tools and propose the Problem, the Solution and the
stops (files, lines, order and explanations). While it runs the button says
**Stop drafting** and stops it; the author keeps writing, and leaving the
window does not stop it. **Draft from PR context** and **Suggest an outline**
read the repository the same way and say **Stop drafting** on their own
control while they run; Enhance and Generate comment stay tool-free. Without
a clone the tab says drafting reads only the pull request's files. The answer
is a proposal under the button: each part accepted (with the author's edit in
place), or discarded, on its own; a Problem or Solution the author already
wrote says that accepting replaces it; a stop whose lines are not lines of the
pull request's files is left out before the proposal is shown, with one quiet
line saying how many and each one kept in the transcript. Stops on the change
come first; one just off it says "Outside the change"; one off it on several
lines says it won't appear when published until it's on changed lines and
offers **Move to nearest change**. An accepted stop is
added at the end of the route at its head lines, to be confirmed in the code
before it is Ready. Since [[BPR-1925]] v003 (owner: "this whole section probably
just goes away and we move the accept into the stops directly") the proposal is
not a list under the button. Each pending part waits in the tour on what it
changes. A proposed stop sits on the route, after the tour's own, under a grey
ring and rail that turn amber once it is accepted. A stop's new explanation
sits on its card, where **Ready, next** is. A proposed Problem or Solution sits
under the part's own text, with a "Proposed" pill on its step. Each has
**Accept** at the right, with **Edit text** and **Discard** before it. One line
under the button counts them, with **Accept all**. **Ready, next** shows only
when it can act: a read stop whose title, explanation and lines are confirmed
and which is not yet Ready. Until the lines are confirmed (Show in code, then
Use selected lines while editing) the location line says "Lines not
confirmed" and Show in code leads; a Ready stop needs no button. Pressed, it
goes on to the next stop when the stop shows as Ready ([[BPR-1936]]). Since
[[BPR-1930]] the tour's own chat proposes the tour's shape too: stops to
remove, merge, split or reorder, and a focus. The core checks each part against
the tour (a merge keeps every line of its stops), and the stops a part changes
are marked grey on the route with a pill ("Remove", "Merge with 3", "Moves to
1"). Its box waits on the card of the stop it leads, with **Remove**, **Merge**
or **Split** at the right; a new order waits above the route with
**Reorder**. A focus from the chat applies at once, with Undo on its answer; it shows as a chip under the header and rides
every later draft and chat turn. Feedback given while Draft Tour runs is
sent once it proposes, and revises the stops still waiting. **Reset tour**
([[BPR-1931]]) asks first, naming what is lost, clears the draft, its
proposals, chats and focus, and offers **Undo reset** until the tour changes;
a published tour on GitHub is not touched. When Claude ran a command outside the read-only list, one
quiet line under the proposal says how many, with a link to them in the
transcript (BPR-1912). **Transcript** opens the session's runs, newest first,
in a window of its own beside the main window (BPR-1913): what was asked,
what it searched, read and ran, and what it answered or how it ended, with a
failed run's reason and log; it never opens by itself. A selection spans a
run's entries, and **Copy run** and **Copy all runs** put plain text on the
clipboard. BPR-1892's notice keeps its words and its button is now **Show the
run**, which opens that window at the run. Each
request is a fresh run given the tour so far, never `--resume`. See the
[AI agent boundary](ai-agent-boundary.md#the-tour-sessions-read-only-drafting-bpr-1900).

Since [[BPR-1914]] v003 (owner, 2026-11-06: "something more like a side chat")
each part of the tour (the tour as a whole, the Problem, the Solution, each
stop) has its own side chat. Since [[BPR-1925]] (owner, 2026-11-07: "it's kind
of weird that the sidechat is so decoupled from the tour when it's really
about the tour") the chat is the Tour tab's second column, beside the tour,
not a pane at the window's edge. While it is open the drawer widens to hold
both (740 pixels at first, then the width the reviewer drags it to), and the
part under discussion carries the agent's rule: a faint wash of the accent
and its line down the left of the Problem's or the Solution's panel, or of
the stop on the route. Each part's chat button sits at the right of its
header: the Problem's and the Solution's step rows, the open stop's title row,
each stop's row on the route, and Draft Tour's row for the tour as a
whole. It shows how many turns its chat has, and it toggles: a click opens
that part's chat, a click on the open part's closes the column, and a click
on another part's switches (v002, owner: "I also want the chat button to
toggle it open and closed"). Pressed, it is filled with the agent's accent.
There are no tabs, and going to another stop on the route brings that stop's
chat while a stop's chat is open. The chat goes with the Tour tab:
on Comments, Notes or Agent it is gone and the drawer is its narrow width
again, and the Tour tab counts the parts that have a chat in the agent's
accent; back on Tour it is as it was. Close gives the narrow drawer back and
keeps every chat. On the tour's own page (Expand) the chat sits beside the
tour there. The button,
or Open side chat in its menu, opens it, quoting the selected text; a stop's
chat shows the stop's lines in the diff, and Ask about its code quotes them.
The chat is plain: the author's bubbles, the agent's rendered replies with
highlighted code ([[BPR-1919]]), a folded "Worked for Ns", Copy per reply and
for the chat. **Update the Solution from this chat** (or the Problem, this
stop, the tour) asks for the part rewritten from the chat, and the proposal is
accepted, edited or discarded in the chat. Each chat is its own thread,
replayed alone with the part's current text. The conversation window described
next is the copyable record of every chat.

Since [[BPR-1934]] (owner, 2026-11-07: "Tour side chat should get suggestions
like the agent tab has") an empty chat offers suggested prompts under its
invitation, in the Agent tab's own component (`agent_suggestion_list`,
[[BPR-1877]]) and the agent's accent. A stop's chat offers "What does this
code do?", "Why does this change matter?", "Rewrite this explanation more
simply" and "Is this the right place for this stop?". The Problem's and the
Solution's offer "Make this clearer", "What's missing?" and "Make it
shorter". The whole tour's offers "Too many stops?", "Suggest a better
order", "Focus on …" and "What's not covered?". A click sends the prompt as
if typed, with any quoted passage. Shift-click puts it in the composer to
edit, and "Focus on …" always goes there to be finished. The core's
`decide_agent_suggestion_press` decides the press, as it does for the Agent
tab. The prompts show only once the agent is set up ([[BPR-1881]]), and they
go with the chat's first turn. The lists live in one place in the application
layer (`application::tour_chat_prompts`); the window hands them to the chat,
so Agent settings can later make them editable.

BPR-1934 v002 (owner on v001: "I want a suggest prompt for explain selection
like agent has for code, but mine for the selected block on the tour") adds
**Explain the selected block**, as the Agent tab offers Explain the selected
block for code. While text is selected in the Problem, the Solution or a
stop's explanation (its source or its rendered text), the chat offers it
first, even when the chat already has turns. In another part's chat it names
the part ("Explain the selected block in Stop 1"). Pressing it opens that
part's chat if needed and asks about exactly the selected text, quoted as the
turn's passage. With nothing selected it is not offered. The tour tells the
chat as the selection changes (`tour_side_chat::offer_selection`). The prompt
is in the same list, marked `Selection`.

Since [[BPR-1914]] (owner, 2026-11-06: "I want to ask questions, get back and
forth and have it update it") that window is the **Tour conversation**, opened
by **Conversation (n)**, which is always offered. Selecting text in the
Problem, the Solution or a stop's explanation offers **Ask about this** beside
the field's Enhance control; it opens the conversation with the passage quoted
above the ask box. A question with no passage asks about the tour or the
change. Each turn runs read-only in the workspace like a draft, and is given
the tour as it is now and the earlier turns, so a follow-up keeps its context
whatever the agent. The conversation reads as one selectable document, oldest
first, with each reply rendered as Markdown; **Copy turn** and **Copy
conversation** copy it as text. Asked to change the tour, the agent proposes:
a changed Problem or Solution, new stops, or a new explanation for a stop
already in the tour, which join the proposal under Draft Tour and change
nothing until accepted. Since [[BPR-1915]] every built-in tour prompt asks for
GitHub-flavoured Markdown that scans (short paragraphs, lists, inline code, no
headings), and a proposed text shows rendered as the preview and GitHub show
it, with **Edit text** for its source. See the
[RFC-BPR-0092 amendment](../rfcs/RFC-BPR-0092-agent-first-run-setup.md#amendment-bpr-1914-bpr-1915-2026-11-06-the-tour-conversation-and-formatted-drafts).

The stop editor retains native explanation documents by stop ID, with a
wrapping outline and separate title, explanation and related-source controls.
The context menu offers insert after, duplicate, merge selected adjacent stops,
source replacement and remove. Add also inserts after the current stop and
selects the new one. Command/Control+D and Delete act only with the outline
focused, preserving native text editing in the fields. `Alt+Shift+Up/Down`
reorders while keeping the selected group; the window's `Alt+Up/Down` change
gestures ([[BPR-1328]]) remain free. The window's First and Last change hold
`Alt+Shift+Up/Down` too ([[BPR-1532]]) and are unbound while the outline has
focus, so the reorder runs there and the change gestures everywhere else. The owner removed the separate Arrange and split controls
at attended hand-back 70; the underlying split algebra remains tested.
`Ready and next` (`Alt+Return`) marks only a complete, resolved stop Ready.
An oversized edit remains in its native document for shortening, copying or Undo.
Other fields and tour mutations wait until that edit fits, preventing a later
projection from silently replacing refused text.

Previous, Next and Return to tour preserve the independent conceptual route.
Normal file exploration leaves the selected stop and editor intact. Navigation
reuses the workbench file/diff path, then compares immutable source material
before restoring the range under the current runtime comparison identity.
Unavailable or changed material remains explicit; it never selects a nearby
line as a substitute. The ordinary opt-in “mark read when fully seen” trigger is
suppressed while Tour is shown. Neither readiness nor visiting a stop creates a
review receipt, resolution, approval, remote mutation or agent invocation.

The preview is private and lists the brief, ordered explanations, exact source
links and compatibility requirements. Remote publication has its own later
explicit Preview/Publish and recovery boundary.

## Raw Markdown and cold import

The existing paged provider reads raw comment bodies separately from display
prose, with returned IDs, web addresses, actors, review membership, versions and
available source coordinates. Numeric identities preserve the full uint64 range.
Pagination must finish before an accepted snapshot is complete. Missing raw
observations in an older recording leave a candidate read-only.

The core enumerates ordinary ordered lists in review summaries and PR comments.
An explicit description link prioritizes a candidate; multiple roots/lists require
selection. Index order, local stop IDs and displayed comment numbering remain
separate. Missing, repeated, malformed, foreign, self and mixed-author entries
remain visible; none grants mutation authority. Links are compared to exact
returned same-PR destinations and are never recursively fetched. Import refuses
to overwrite an existing private draft.

The bounded Markdown scanner excludes fences, quotes, nested lists and HTML
regions from top-level mapping. It preserves ambiguous sections/lists for manual
selection. Every editable section retains the complete raw baseline and a byte
span. Native selection translates UTF-16 positions and CRLF normalization back
to exact UTF-8 boundaries; a changed display or split surrogate is refused.
Disjoint edits validate expected span bytes and preserve all untouched material.
A custom stop body is retained whole unless its generated heading and final
navigation paragraph are confidently recognized.

Payload v2 added remote attribution, index baselines and read-only state. Payload
v3 adds a comparison-independent source destination. Versions 1 and 2 remain
readable without rewriting their content; the next save uses the current payload version. The database
schema and private-root contract are unchanged.

## Exact source and compatibility

An imported destination retains commit, optional blob, path, side and inclusive
zero-based range. It has no invented comparison digest. Native right-side
comments prefer the original commit/range. Missing original left-side source
facts remain unavailable. File/PR comment fallbacks use the provider's canonical
commit-pinned first-line permalink. The explicit tour codec extends the existing
single-line codec with ranges and historical commits, sharing canonical path
escaping and Markdown `?plain=1` handling. Parsing establishes an address;
workbench navigation checks the actual admitted commit, blob, path and side before
constructing current comparison line references. Unavailable source retains its
explanation and visible source link.

| Intended subject | Required exact operation | Publication boundary |
|---|---|---|
| Current diff line/range | Pending line batch | One pending review only when qualified |
| Current off-hunk range | Pending off-hunk batch | Separately qualified from standalone line creation |
| Changed-file permalink fallback | Pending file-thread operation | Joins the pending review only with exact support |
| Standalone file fallback | Standalone file-comment operation | Explicit compatibility plan |
| Historical/base/unchanged source | Tour PR-comment operation | Explicit compatibility plan |

Unknown, denied, throttled and indeterminate outcomes do not authorize another
representation. Exact unsupported evidence and exact fallback support are
separate facts. Suggestion bodies cannot silently become file/PR permalink prose.
The capability key separates operations and exact source digests in addition to
host, actor, repository, API/classifier revision and freshness. Existing
standalone evidence never qualifies pending file staging. The preparation host
has no live-qualified pending-operation observation and displays that limit;
loading source or importing an own comment does not create write authority.

## Staged publication wire contract

BPR-0999 extends the existing review-submission/mutation family with finite
pending-review operations. A batch sends the exact head and line/range comments
without an event. The recorded review ID receives COMMENT only through its
separate submission operation. Body updates use explicit PATCH/PUT; owned
pending deletion uses DELETE. Native HTTPS witnesses each mutation before
creating its Qt request. Ordinary summary and source-line publication retain
their contracts.

Raw responses retain complete numerical IDs, returned same-PR web URLs, actors,
review membership and inclusive source coordinates. A page continuation must
name the exact collection and next bounded page; incomplete membership cannot
establish ownership. The wire ceiling remains 64 KiB including JSON overhead,
100 comments per batch, 256 KiB per response and ten pages per collection.
These are fixture-tested contracts. Pending comment editability and website
visibility are not yet qualified on a live host, and pending file-thread staging
remains unavailable. No compiler, parser or read observation grants write
permission; the staged coordinator owns the journal and author command boundary.

Staged publication extends the existing review-submission family. A frozen plan
contains ordered sources/prose, exact live binding, owner/generation and any
separate description edit. The host consumes one bounded parent grant; each
transmission consumes an exact child journal revision. SQLite recovery v5 (v6 since [[BPR-1540]]) stores
one finite pending step and confirmed returned identities in the existing
private recovery database. Unknown writes are reconciled by complete reads and
are never blindly repeated. Stop/restart retains receipts and requires explicit
Resume for further writes. Live pending mutation endpoints are on by the
[owner amendment of 2026-10-09](../issues/40-parked/BPR-1222-qualify-live-github-review-tours.md#owner-amendment-2026-10-09-publication-on-and-files-changed-links).

## Published maintenance

An explicit update preview freezes the observed owned root, original stop bodies
and desired route in the existing staged-publication journal. New destinations
use a later pending batch; confirmed IDs survive Stop and restart. Body-only
edits preserve original threads and replies. Derived navigation changes only
where its bytes change, and the original root index is updated last. Every stop
retains an overview link through a partial update. Retirement removes membership,
not the shared discussion. Explicit source replacement creates a linked new
comment and preserves the old source binding. Published split/merge operations
retain the original discussions and prepare local replacements.

The Tour payload reads versions 1–7 and writes version 7, retaining bounded retired
stop baselines, exact baseline/local/remote conflict versions, the observed head,
per-stop reaffirmation and locally observed website changes. The existing recovery database retains its schema; staged payload
version 3 additionally stores the frozen maintenance draft and confirmed edit
prefix, while still reading versions 1–2. Existing profile purge and private-store
retention cover these bounded prose records; no shared object is deleted by purge.

Before each write the session reads exact actor/head, current owned bodies and
relevant review memberships. Website replies are preserved. A changed baseline
stops the plan and retains its desired text. These endpoints provide no evidenced
conditional body write: preflight and reconciliation cannot eliminate a concurrent
website edit between them. No operation is qualified merely by these fixture
results. Production Tour writes are on by the owner's 2026-10-09 amendment and
are qualified by the pre-release itself; the operations the BPR-Test course did
not try stay listed as untried on
[BPR-1222](../issues/40-parked/BPR-1222-qualify-live-github-review-tours.md).

Refreshing a bound tour compares its previous raw bodies, current draft and
complete website observation. Equal versions adopt website truth without writes;
two disjoint contiguous byte edits can merge. Overlap retains all three versions
in the profile store and exposes explicit local/website choices. Source spans are
mapped again from the fresh raw body. Unavailable or ambiguous observations keep
local work visibly stale, and unresolved conflicts withhold publication. Store
revision checks continue to serialize local windows independently of website
conflicts. Description changes patch the mapped brief sections and preserve
unrelated template bytes. For a first publication with neither section present,
the explicit preview inserts the brief at the beginning while retaining every
original byte; partial/duplicate sections require mapping. imported custom stop layouts retain a whole-comment
editor. Reimport from a clean profile retains only shared identities and source
addresses, without private preparation provenance or inferred viewing history.


## Revision attention and observed changes

An admitted selected epoch comparison supplies the existing deterministic line and
move correspondence. Proposed later ranges retain their exact comparison, commit,
blob and renamed path separately from the original public source. They never
create replacement subjects without the author's explicit retarget. Missing or
ambiguous correspondence stays explicit. Observing another head clears previous
reaffirmations for every stop, because identical selected bytes cannot prove an
explanation about a changed caller or contract. The author can deliberately
reaffirm an explanation while retaining its original source, including a bound
imported address whose old blob is unavailable locally.

The locally observed website-change list compares retained raw baseline versions
and ordered remote URLs. It distinguishes added, removed, reordered, changed-text
and changed-source stops without counting unpublished local edits as shared
history. A repeated equal observation retains the last differing observation;
successful publication clears that difference list. A clean profile states
“No prior local tour version.” Changed-stop navigation uses the existing outline
and does not create viewed-file receipts, approval, acceptance or resolution.

The complete fixture manual lifecycle and the separately pending operator course
are recorded in the [tour verification ledger](../verification/RFC-BPR-0039-0043.md)
and [website/native course](../verification/RFC-BPR-0039-0043.manual-course.md).


## Optional outline and author completion

The route and the tour page offer **Suggest an outline** through the same selected
profile, versioned outline hook and restricted writing process as per-field
assistance. It sends at most eight bounded sources, including the current exact
selected source when available. Returned references resolve only against those
supplied identities. A reference to prose has no invented code destination; its
skeleton remains unresolved until the author selects a real source. Unknown or
duplicate references, extra bodies, Ready flags and tool directives are refused.

The author chooses retained suggestions, then **Insert selected skeleton stops**
adds them as one local structural undo transaction. Every inserted explanation is
empty and Suggested/unvisited. Private reasons and author questions are shown
beside its editor and retained in payload version 7; they never enter published
Markdown. Older payloads remain readable without inventing provenance.

A visible visit shows the selected editor with its exact code or an explicit
source-unavailable explanation. It never acknowledges later generated prose.
**Generate comment** invokes the existing in-place editor only for that visible
stop; native Undo/Redo remains unchanged. **Ready and next** supplies a fresh
observation of that editor and the exact displayed source to the core. Hidden
or background editors, other source ranges, and unavailable source cannot complete
a generated stop. The next stop is merely selected; it triggers no inference.

Initial publication and published updates refuse any retained generated stop
without current author completion. Text, title, mapping or website narrative
changes clear readiness; derived numbering and navigation do not. Duplicating,
splitting or merging a generated stop preserves its completion requirement.
Removing a stop removes that outstanding obligation. Cold import cannot reconstruct
private generated provenance or author visits that were never published.


## Shared local history and resumption

The CLI and desktop use database schema 3 and the same atomic tour-state
record. Its version-two envelope adds the originating presentation to each
reversible transaction and an optional section/stable-stop resume hint. Legacy
draft payloads and version-one journal envelopes remain readable; opening them
does not invent prior history or provenance. The resume hint is private workflow
position, never a mutation target or proof of an author visit.

Each direction retains at most 16 transactions and 8 MiB. Eviction affects only
old undo/redo entries. Current text, unresolved conflict candidates, source and
readiness bindings remain in the current draft; remote publication records stay
in their separately admitted journal. Undo restores exact stored content with a
new revision, and redo needs no AI or network. No-op edits add no transaction.
Both history and current content commit under the existing exact revision CAS,
so a failed or killed commit cannot retain only one half of an edit.


## Owner forwarding

While the desktop owns a profile, `bpr tour` forwards each local draft command to
it instead of opening the store. The CLI acquires the profile lease first; only
when that lease is busy does it try the owner's tour endpoint, which is a
separate rendezvous beside the activation endpoint. Activation frames keep their
fixed meaning and an owner without a tour endpoint answers
`owner_protocol_unsupported`. The CLI never opens a second writer, steals a lease
or starts the desktop.

The endpoint is a per-user local socket bound only while the owner holds the
exact-root lease. Each connection is authenticated as the same OS user and
begins with an owner-advertised 64-byte binding: protocol version, profile, root
digest and a per-owner-instance nonce. The caller sends nothing until the binding
matches what it resolved for that profile and root, and the owner accepts a frame
only if it echoes that binding. Frames declare a length of at most 5 MiB and are
decoded by a closed codec: `load`, `mutate` and `request_status` for one opaque
draft selector, with local editing commands only. Observation, restore, import,
publication and provider operations cannot be encoded. A path, a PID or a JSON
actor string is never authority.

The owner serves one request at a time on its effect lane from its own store, as
one bounded transaction. Every mutation carries a request ID and a payload digest
the receiver computes itself. The outcome is committed in the same compare-and-set
save as the edit, so the same ID with the same digest returns the stored outcome
and the same ID with another payload is refused. A caller that lost an answer
learns of it as `outcome_unknown` and is told to inspect the exact ID
(`status --request-id`) before any retry; nothing is replayed on its behalf.
The journal keeps at most 4096 outcomes per draft and refuses further requests
rather than forget one, and it records only the local outcome, never that GitHub
accepted anything.

On Windows the same framing runs over an owner-private named pipe beside the
activation pipe, in byte mode, rejecting remote clients; the caller checks the
owner's user and the pipe's descriptor before sending ([[BPR-1168]]). Protocol 3
added the `cycle` operation: one finite `bpr cycle` command and that command's
own options, run by the owner on its cycle store (see
[the cycle terminal](../development/cycle-cli.md)). An owner or caller of another
version is told so before a payload is sent.

### A draft the desktop has open

A draft that a desktop window holds, current or retired, is read and written
through that window's host, on the decision lane, so there is one committed
history and never a second copy read from the store beside it. The endpoint is
polled on the effect lane; each request is routed to the window session that holds
the draft's opaque selector. The desktop lists its own draft in the profile's
target catalog when it composes the pull request, so the CLI and the owner name
the same draft by the same selector.

Ordinary typing is committed to the host on every change, so there is no pending
buffer to flush. Uncommitted input-method text is the exception: while any field of
the shown draft holds preedit text the owner answers `editor_busy` and changes
nothing, and the CLI reports that nothing was changed. A forwarded mutation is
answered only after the host has saved it durably; if the save has not landed
within five seconds the caller is told `outcome_unknown` and inspects the request
ID, exactly as for a lost answer. A draft nobody has touched is never persisted
and counts as saved.

Another presentation's edit reaches an open editor as a projection. The author's
caret, selection and scroll carry across the change, focus is not taken or given
up, the selected stop stays selected unless the edit removed it, and no selection
change is announced. A stale command, whether from the CLI or an old AI reply,
is refused as a conflict; both texts remain. Structure follows stable stop IDs, and
undo from any presentation restores the same order and IDs through the one shared
history.

Pinned source is read where the pull request is open (BPR-1227). `add`,
`show --source` and `walk` send the owner a `source` request (owner protocol v6)
naming the draft and one pinned commit, path, side and line range; nothing else
rides along, and it carries no request ID because a read records nothing. The
window whose current tour is that draft reads the range through its own epoch host
and Git cache on its diff lane, with the same bounds as a standalone read: at most
200 lines and 256 KiB, exact UTF-8, no working-tree text. The CLI then builds the
stop exactly as it does standalone and forwards it as an ordinary mutation, so only
the stop is recorded. The CLI process reads no provider and opens no profile store.
A draft no window shows is refused as `interaction_required`; open the pull request
in the desktop and retry. A closing window stops its reads and waits for them.

`open`, refresh, publish and recover still answer `interaction_required` while a
desktop owns the profile: they observe the provider for a pull request the owner may
not have composed, or need publication authority at the invoking terminal. Assist
takes no source evidence from a running desktop either; ask without `--sources`.

## Assistance tickets

The active agent of RFC-BPR-0046 asks for writing help through two short-lived
requests rather than a nested model. An assist ticket is a bounded record in the
draft's shared journal, committed under the same compare-and-set as the draft. It
fixes the hook, the field, the exact byte interval, the draft revision, the field's
digest, the hook revision and a snapshot of the sources, and it expires after
thirty minutes. The draft holds one live ticket; a new one supersedes the old.
Answering names the ticket and supplies only a result. The result is decoded by the
parent RFC-BPR-0043 contract, the ticket is spent, and only a result whose revision,
field bytes and hook revision still match is spliced in through the shared text
transaction as one undoable edit. Outline answers become body-free proposals kept
in the same journal, and a chosen subset is inserted as Suggested stops through the
existing structural command. Read paths issue nothing, and nothing here reads a
provider or starts a process.
