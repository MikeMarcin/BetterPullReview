---
id: "RFC-BPR-0043"
title: "Scoped Review Preparation AI Assistance"
status: "Proposed"
area: "BPR"
milestone: "Review tours"
depends_on: ["RFC-BPR-0011", "RFC-BPR-0012", "RFC-BPR-0018", "RFC-BPR-0042"]
pack_version: "review-tour-v1"
metadata: {"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# RFC-BPR-0043: Scoped Review Preparation AI Assistance

## Decision and precedence

AI is optional, explicit and narrowly scoped. Provide magic-button assistance inside the ordinary Problem, Solution and stop-comment editors. Apply a successful rewrite **in place as one ordinary undoable edit**. No separate suggestion pane, Use suggestion, Apply, Accept replacement, or Dismiss workflow is required. This is the user's final chosen interaction and supersedes earlier discussion proposing side-by-side replacement acceptance.

This RFC narrowly extends the existing explicit AI boundary: clicking the magic button authorizes a replacement of one captured local field/span. It never authorizes publication, other fields, code changes, navigation, review disposition or autonomous follow-on calls. Existing non-tour comment-workshop behavior remains unchanged unless separately requested. Reuse its agent/session and draft infrastructure without importing its separate Accept-revision UI into these tour editors.

The complete manual path, GitHub round trip and post-publication maintenance are delivered first. No agent is launched by opening the wizard, editing text, selecting a stop, loading GitHub content, changing settings, or publishing manually. Unavailable assistance leaves editors usable.

## Four independently customizable hooks

Expose Settings → AI assistance → Review preparation with four built-in instruction hooks: Problem statement, Solution summary, Tour outline, and Tour-stop comment. Each has editable instructions, Restore default, an effective-source/version display, and an explicitly invoked nonpublishing test action. Use existing profile-scoped settings and AI profile selection; do not hardcode model names or effort settings in this pack. A hook selects writing behavior, not credentials or mutation authority.

The pack includes exact initial defaults in [skill-defaults](assets/bpr-review-tour-v1/skill-defaults/README.md). Ship text resources under the existing packaging/resource approach and allow user-owned overrides. No new generic plugin platform, shell-script execution engine, MASK role hierarchy or automatic repository-supplied prompt loading is needed. Project/team overrides may be added only with existing explicit trusted-config selection; untrusted PR/repository content cannot override the hook's application contract.

## Invocation scope

| State at the click | Operation and visible label |
|---|---|
| One nonempty editor selection | Rewrite only that range; **Enhance selection**. |
| No selection, field contains non-whitespace text | Rewrite the complete field; **Enhance problem statement**, **Enhance solution**, or **Enhance comment**. |
| No selection, field is empty/whitespace-only | Recover/draft that field from approved PR context; **Draft from PR context**. |

Selection takes precedence even if the selected characters are whitespace. The caret alone is not a selection. Initial delivery supports one contiguous text selection, including multiline selection. Multiple disjoint carets/rectangular virtual-space selections must be refused clearly or require selection of one real contiguous span; do not silently rewrite their bounding rectangle or several spans. This does not change BPR's separate multi-caret editing plans.

Capture selection before the toolbar takes focus. The selection defines the exact write boundary, not the maximum read context. The model may receive surrounding text, the other brief field, selected/related code and approved PR explanation as context. It returns only a replacement for the captured text, through a bounded structured result. The application assembles unchanged prefix + replacement + unchanged suffix. Neither the prompt nor a model-returned offset controls that boundary.

Do not silently expand to a paragraph/sentence/Markdown block. Partial phrase selection returns a fitting phrase. Markdown syntax outside the selected span remains byte-for-byte unchanged. A model must not close/reopen formatting outside its allowed scope or change adjacent punctuation. An equal replacement is a no-op and creates no undo entry.

## Problem and solution writing rules

Nonempty input is an editing task: preserve author meaning, qualifications and supported specificity. Prefer concise concrete language. Add headings/lists/tables only when they actually help; one sentence can remain one sentence. Do not turn “reduces” into “eliminates,” infer production severity, assert test success from test existence, or invent rejected alternatives.

For the user's input “Crash when reading data that's been concurrently deleted.”, an acceptable improvement is “Reading data can crash when that data is deleted concurrently.” It must not diagnose a use-after-free or claim full thread safety merely from that sentence.

For an empty field, recover existing statements from the raw PR description and relevant author tour/review comments first. Preserve their source identities. Where needed, use the actual diff and pinned source to propose a cautious explanation. Retrieved text may itself be incorrect or generic; it is source material, not unquestioned truth. Private invocation notes distinguish Recovered, Inferred and Still unclear. They are not automatically appended to the public text. With insufficient evidence, leave the editor unchanged and show one useful focused uncertainty rather than fabricate intent.

The solution hook receives the current problem for coherence but cannot alter it. Describe the chosen mechanism and limits; avoid a file inventory. Likewise, the problem operation does not rewrite the solution. Repeated invocation uses the author's current edited text, not an earlier saved or generated version.

Jira retrieval is explicitly deferred. Preserve linked Jira URLs and allow authors to paste relevant context now. Do not query Jira, GitHub issues, arbitrary linked sites, or company-wide search automatically. A later extension must be explicitly scoped to a selected linked ticket with its own credential/trust and disclosure policy; it is not a dependency or open gate for this pack.

## Exact undo/redo and asynchronous completion

At click time capture request ID, editor/field ID, draft/stop ID, exact raw-body generation and digest, raw UTF-8 replacement interval, Qt selection coordinates/direction, profile/account/PR identity, source comparison, hook/profile version, and focus/caret revision. Application-owned request IDs are never accepted from model output as authority.

Retain original text while generation runs. Show a small busy/cancel state, not streamed partial replacements. A complete, valid response becomes one distinct local edit transaction; adjacent typing must not coalesce with it. Undo restores exact original bytes and selection; redo restores the exact received replacement without another AI call. Keep replacement text selected when the original selection/focus is still current, enabling another immediate enhancement. If the author moved the caret elsewhere without editing, replace only the captured range and do not steal focus or selection.

Any text edit in that field, including IME commit, undo/redo, another completed rewrite, remote refresh acceptance, or a same-draft external-window update, invalidates the pending request. Compare a monotonically increasing generation, not only text equality: editing and undoing back to identical bytes does not resurrect an old response. A later invocation supersedes the earlier one. PR/profile/account changes, closing/suspending the editor, retargeting the stop or changing relevant source context cancel it. An inactive/replaced editor never receives text secretly on return.

Refusal, cancellation, protocol failure, timeout, invalid UTF-8, forbidden scope, over-budget output or stale completion leaves text and undo history unchanged. State “Rewrite cancelled because the text changed,” where applicable. No automatic merge or retry on the author's behalf is needed. Undo after publication still changes only the local draft; it cannot retract a GitHub comment.

Qt's text cursor edit-block mechanism is a possible implementation primitive, not the whole correctness argument. Use the current editor's undo architecture or a narrow wrapper so selection restoration and canonical bytes are covered. Never implement this by `setPlainText` on every projection, which can reset edit history. Preserve a tested UTF-16-to-UTF-8 boundary map, newline/CRLF conventions, surrogate pairs, combining text, tabs, and paragraph separators. A Qt selected-text representation is not assumed to be the canonical source string. [QT-CURSOR] in [EXTERNAL-AUTHORITY](assets/bpr-review-tour-v1/references/EXTERNAL-AUTHORITY.md).

## Optional outline, not full-tour generation

**Suggest tour outline** returns only proposed titles, source destinations, short private reasons and optional author questions. It returns no publishable comment bodies, full description replacements, Ready flags, publication directives, or auto-follow-up actions. Unexpected comment-body fields in the outline result are rejected, not silently turned into drafts. Suggested titles and destinations remain editable and require actual source validation; an inferred destination that cannot resolve remains an unresolved suggestion.

The author may insert suggested skeleton entries as a single undoable local outline edit, then freely remove, merge, split or reorder them. Every generated skeleton stop begins Suggested/unvisited. A visit means the actual stop editor and its relevant code or explicit source-unavailable state were shown—not just a selected row in a hidden/background view. A source-unavailable skeleton cannot be passed as source-validated Ready.

Generating a comment requires another explicit magic-button invocation on **that currently visited stop**. It writes directly into that stop editor with ordinary undo. There is no Generate all comments, Complete tour, auto-advance generation, bulk Ready, or one-click AI-authored tour publication path.

The current generated text/destination must receive an author completion decision while visible, e.g. **Ready and next**. This is preparation acknowledgement, not an AI Apply/Dismiss UI. Visiting an empty placeholder before an asynchronous result arrives does not acknowledge the eventual text. Regeneration/retargeting invalidates readiness; deterministic ordinal/footer updates do not. No minimum dwell time, forced keystrokes or artificial edits are required. The mechanism creates a deliberate opportunity for scrutiny; it does not claim to prove comprehension.

## Host and protocol boundary

Reuse the existing explicit agent-launch/session/context-supplement path. The provider/profile determines local/cloud inference according to current BPR settings. Publish nothing from the agent; do not give it GitHub tokens, mutation ports, arbitrary URL fetch authority or executable repository hooks. All PR comments, source text, tickets pasted by the author and returned model text are untrusted data. Instructions in them do not override application scope or publication policy.

Use bounded request context with explicit included/omitted source references and current revisions. Do not send the entire source dump or entire private repository by default. Raw text/prose, prompt content, code and credentials are excluded from standard diagnostic logs. Retain bounded provenance identifiers for the applied local edit under current privacy policy, not fabricated model/version claims. Cold GitHub import cannot recover provenance that was never published.

The host validates a tagged response shape and emits a typed local-editor replacement or outline proposal. The core validates captured identity/generation/scope; Qt performs the one editor transaction and the host persists its accepted local delta. A stale or duplicate completion cannot insert twice. Settings overrides can alter writing instructions but not widen scope, accept malformed output, acknowledge a stop, publish, or execute code.

## Acceptance criteria

- **AC43-01:** No action outside the explicit assistance controls launches AI; all manual features continue without an AI provider.
- **AC43-02:** Four separately configurable hooks are versioned, previewable/testable explicitly, resettable and correctly scoped to app profiles.
- **AC43-03:** Selection/whole-field/empty-field precedence is exact, including whitespace selection and toolbar focus preservation.
- **AC43-04:** Only the captured raw-text interval changes; surrounding Markdown, Unicode and newline bytes are preserved.
- **AC43-05:** One successful invocation is one distinct undoable edit; undo restores original text/selection, redo replays bytes without inference, and equal/failed results create no entry.
- **AC43-06:** Editing, undo-back-to-identical-text, supersession, IME, PR/account/window changes and stale responses cannot overwrite newer work or target a different field.
- **AC43-07:** Empty fields recover existing PR explanations before inference; unsupported claims are not invented, and no Jira/issue lookup occurs.
- **AC43-08:** Outline output has no bodies; each generated stop requires a visible visit and current-content completion, and per-stop generation cannot run in bulk.
- **AC43-09:** Skill overrides and prompt-injected source/model output cannot widen writes, invoke tools, auto-publish, acknowledge stops or leak credentials.
- **AC43-10:** End-to-end native editor tests exercise the real undo path, profile settings and agent delivery seam; canned response tests are not represented as quality evidence for an untested model.

## Deferred work

Jira integration, general issue-tracker search, simultaneous multi-selection AI rewriting, full-tour generation, autonomous preparation agents, team policy scores, company-wide analytics, and changes to ordinary non-tour comment-workshop acceptance are outside this pack. A text-only settings hook is not an orchestration topology. The handoff uses the installed implementation workflow without hardcoded model routes.
