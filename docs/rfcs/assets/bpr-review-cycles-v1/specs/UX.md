# Native Review Cycles workbench

## Entry and distinction

Add **Open Project…** and a **Review Cycles** recent-items section beside existing PR
entry points. Opening a folder/module/file starts or resumes a local cycle with no
required remote/account/agent setup. Capture scope is visible, with simple defaults
and optional exclusions. The first editable source note should not require a cycle
template, title essay, goal wizard, label or commit. Use an intelligible local default
title and permit later rename. A file open outside a repository is legitimate.

A cycle's header uses its title and a **Local cycle** badge, not a fake PR number or
GitHub account. An optional linked PR is explicit navigation, not a replacement of the
local identity. A cycle is distinct from a Review Tour (an author's conceptual guide
published on a PR) and from private notes on an existing PR. Keep the brand BetterPullReview;
no new app or mascot is requested. Do not rename established PR commands globally.

## Integrate the selected scaffolding

Use the RFC-BPR-0052 title row, review bar, folding Updates line, per-document tabs and
right tray. Do not introduce another permanent toolbar/row or undo the owner-selected
chrome. Fit cycle-local names and actions into the same semantic slots: comparison/
revision selector; previous/next file/change where meaningful; file progress;
**Compose feedback** and **Comment/Reply**. The established compose keyboard gesture
may route by active subject; it must never publish a PR because a cycle is selected.

Full source is the initial mode and uses one source pane. The comparison selector
can switch to This round, Since my last review, All cycle changes or exact revisions.
No initial all-added diff, duplicated identical panes or placeholder empty baseline.
Change navigation is disabled/absent when no comparison exists, while file navigation
and comments remain available. Keep full source reachable throughout all iterations.

The Updates line shows captured local revisions and observed quality/history gaps;
it does not use capture order as proof of Git upload order. Per-tab exact revision
bindings and reading positions survive restart. Later captures show newer material is
available, not move a selection or editor under the user's cursor. Overview/Discuss
use the window-level selection, as the existing scaffolding specifies.

## Feedback and suggestions

Use Comments for local conversations and an appropriate shared Findings view for
claimed/verified items; do not require AI to instantiate either. A selected range
supports Comment and Suggest change; file/module/cycle-level feedback is accessible
without a bogus line anchor. Related-source references keep one conceptual discussion
connected across files. The inline editor starts with exact selected code and supports
before/after preview, ordinary undo and native keyboard/clipboard behavior.

The suggestion editor is deliberately limited to code proposals, not an IDE workspace.
“Open in editor” remains the normal way to implement larger changes. Editing a
suggestion does not edit disk. Already-applied local edits are labelled as such after
capture. A question or illustrative example must not display an automatic Apply badge.
Ambiguous/stale/deleted anchors retain a readable original source view and do not jump
silently to a plausible current line.

Do not invent new label art. Reuse the all-twelve-label treatment and Plain from
0047–0051 when available. Compact native controls must expose accessible names,
keyboard focus and source text. Agent provenance is optional; human and unknown
authors have normal readable presentation, not errors. Imported claimed identity must
not look verified. Preserve the user's multi-caret/virtual-space editing requirements.

## Compose feedback and share

The composition page contains an optional overall goal, requested action, constraints
and the ordered item list. Most users add notes while browsing and only inspect this
page before delivery. Selection/order/context toggles compose the same payload; there
is no second combined-prompt editor or mandatory AI-generated summary. A deterministic
**Preview feedback** shows the readable outgoing content, not a model paraphrase.

Use **Share feedback…** with Copy to Clipboard, Save to File…, Use in external agent
and Send to Agent… according to policy/capabilities. Names are proposed interaction
labels, to refine through the existing attended design process without changing the
semantics. Copy and Save are first-class top-level choices, never hidden behind an
“AI tools” page. Send is not the default required completion action. With integrations
removed, the page remains complete and uncluttered; no disabled AI setup banners.

The preview shows the selected round, item/context counts, source/replacement content
and destination. Freeze and share can happen in one user gesture after preview.
Subsequent edits create an amendment/next draft without changing the already-shared
round. Re-copying or saving the same round reuses its identity. Status text distinguishes
Copied, Saved, Retrieved, Adapter accepted and Recipient acknowledged; none means
work completed or verified. Oversize output clearly explains the limit; no silent
truncation or forced per-comment handoff.

**Use in external agent** shows the exact cycle/round invocation and access status.
When the harness has the thin skill and local CLI reachability, the user can ask it
to read the round directly; no file export is required. The user still chooses their
own harness. Missing reachability or denied automation leaves Copy/Save available
only where policy allows, not as an evasion. No implicit persistent background bridge.

## Receive, compare and continue

The primary return action is **Capture revision**. It works regardless of delivery
receipt, reported responses, agent installation or BPR having been closed. Report
import/manual reply are adjacent optional conveniences. A recipient's response is
shown with the original item and relevant captured diff. `Addressed` invites review;
only explicit reviewer action marks the item verified. A missing report is not an
error and does not block the next cycle. “No source changes” is an ordinary outcome.

Compose the next round from selected existing items plus new observations. Whole-round
handoff is normal; the UI should not force an acknowledgment or confirmation per item.
A reviewer can send one item urgently by selecting it, without breaking the batch model.
Do not start a new agent, auto-capture on each save, auto-dispatch on every note, or
require waiting for a provider's callback before continuing.

## Publish when ready

**Publish as PR…** prepares the finished result. Distinguish the original inspection
baseline, selected local final revision and outgoing Git base in the preview. Show
preexisting work, chosen improvements and any candidate preparation changes. The
suggested default is one clean new commit, not a destructive local squash operation.
Choose destination/branch, attribution, title/description and optional draft-PR state.
Unselected local discussions stay private. Optional tour authoring is available through
the existing workflow after a PR exists; it is not a required stage of local review.

A blocked native operation explains the exact capability and offers allowed external
preparation/linking. No automatic reauthentication, push, force update, branch deletion
or publication happens because all feedback was verified. After publication retain
local history and show the linked PR with its actual shared state.

## Native qualification and visual authority

Build a concise design artifact/course for entry, full-source R0, an exact suggestion,
sharing without AI, R1 comparison/verification and publication preview. Adapt to the
existing selected 0052 design and live implementation, not an old screenshot. Record
owner decisions through the current attended-review workflow; do not invent approval
of pixel design in this pack. Code and automated semantic tests can proceed where
contracted behavior is clear; only genuinely undecided visual choices need that review.

Test the established native width floors, keyboard conflicts, focus restoration,
accessibility names, platform shortcut conventions, IME and source/Markdown copy.
Measure workbench height against the current 0052 baseline; no unapproved persistent
chrome tax. The example layouts are descriptions, not accepted art or evidence that a
widget exists. Native Windows/macOS/Linux observations must identify the producing
revision and actual environment; cross compilation is not native acceptance.
