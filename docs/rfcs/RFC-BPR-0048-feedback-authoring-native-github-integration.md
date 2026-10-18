---
id: "RFC-BPR-0048"
title: "Feedback Authoring and Native GitHub Review Integration"
status: "Proposed"
area: "BPR"
milestone: "Post-tour feedback conventions"
depends_on: ["RFC-BPR-0047", "RFC-BPR-0008", "RFC-BPR-0009", "RFC-BPR-0036"]
pack_version: "conventional-comments-v1"
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "proposed_id_requires_live_reconciliation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# RFC-BPR-0048: Feedback Authoring and Native GitHub Review Integration

## Outcome

Make convention-aware feedback easy to author while preserving ordinary GitHub
comments, native suggested changes and review-level Comment / Approve / Request
changes. Integrate with the existing comment workshop and private defect workflow.
Do not introduce a second review publisher, a new defect lifecycle, or mandatory
comment linting. Human-only operation is a complete, first-class workflow.

This proposal is additive to RFC-BPR-0008 and RFC-BPR-0009. Their settled historical
text/evidence is not rewritten. Update current architecture and user documentation
to distinguish explicit public convention text from private classification.

## Optional authoring

Start with conventional recognition available, convention-assisted composition
opt-in, and no required label. All twelve choices and Plain are reachable by
keyboard. Remember the user's chosen authoring preference through the existing
settings authority; do not make the most recently used `(blocking)` an implicit
default for the next comment. An existing manually typed prefix works whether
assisted composition is on or off.

The Markdown draft is authoritative. Controls edit that draft; they do not retain
a competing hidden public label. A user can write first and classify afterward,
or choose intent first. One explicit edit transaction changes the intended prefix,
updates the preview and derived state, and participates in the editor's normal
undo/redo and durable revision handling. Incomplete typing must not continually
rewrite text, move the caret, reset multiple selections, or produce intrusive
validation. Preserve the existing editor's virtual-space/multi-caret behavior
where available; this pack is not an editor replacement.

Selecting Plain removes only the recognized managed prefix with a preview of the
result, preserving the subject, discussion and any suggested change. Malformed or
unrecognized leading syntax is never silently deleted. If conversion needs a
larger change, propose an explicit draft edit rather than reconstructing the body
from parsed fields. A header edit based on a stale body revision is refused or
presented as a conflict candidate through the existing mechanism.

Lightweight wording assistance may prompt for a problem, consequence, evidence or
requested action, but does not impose fixed fields or a minimum essay. Questions
remain questions. Convention diagnostics advise; they do not prevent legitimate
plain prose or create an organization-policy enforcement system.

## Native suggested changes are independent of intent

A comment labelled `issue` may contain a native suggested change. A comment
labelled `suggestion` may contain no patch. Preserve both combinations and comments
with several distinct fences. Do not infer a label from a fenced code language.

The existing suggestion authoring/preview path must coexist with the prefix. A
label/decorations edit, formatting pass, private-note promotion or AI workshop
operation must not alter the suggestion fence delimiter, optional range syntax,
replacement bytes, tabs, line endings, empty/deletion content or surrounding
Markdown unless the user explicitly edits those bytes. JSON transport escaping
may differ, but decoded body and semantic anchor must remain identical.

Only advertise an applicable native change when the existing GitHub placement and
capability contract supports that exact location. A permalink fallback, top-level
comment, unavailable/outdated anchor or unsupported GHES operation is not an
applicable patch merely because it renders a `suggestion` fence. Preserve the
text and provide an honest native limitation / existing browser handoff; never
move the edit to a nearby line to make a button work.

Where the current BPR build has no native apply-suggestion action, a precise
GitHub handoff is sufficient for application of the change. Do not implement a
new commit API or claim successful application from local rendering. Existing
native suggestion functionality must not be removed or disabled by the integration.
[GH-COMMENTS], [GH-WRITE-COMMENTS] and [GH-APPLY] record the public contracts.

## Native review decisions and useful preflight

Keep Comment, Approve and Request changes as explicit review-level choices in the
existing submission surface. Submit through the current review submission host,
recovery store and provider port using the unchanged native disposition values.
Labelling, filtering, saving a draft or changing a private defect produces no
review event. Resolving the last feedback item does not approve the PR; adding an
issue does not request changes.

Make the relationship useful. When preparing a review, project the reviewer's
own unresolved explicitly blocking feedback and relevant selected pending draft
comments, preserving the distinction between already published and still private.
Link back to those exact items. Show unavailable/partial resolution or label
coverage as uncertainty, not "no blockers." Label guidance such as necessary
work or a chore may appear separately; it must not be misrepresented as an
explicit blocking decoration. Other reviewers' feedback may be inspected but
must not be attributed to the submitting reviewer or dictate their decision.

A reviewer can deliberately submit any GitHub-allowed disposition after reading
that context. Do not require a ceremonial explanation or automatic post. The
warning/summary design is decided in RFC-BPR-0050. Changing a label, summary,
selected comments, target head or account after preflight invalidates the relevant
prepared action under the existing request-identity rules. No new shadow batching
system is introduced; use the inherited draft/review submission capabilities.

Review dismissal, stale-review rules and repository merge eligibility remain
native GitHub facts. This feature neither predicts nor enforces a merge rule.
The native review choices and API event vocabulary are grounded in
[GH-REVIEWS] and [GH-WRITE-REVIEWS].

## Defects and private notes

Public intent and personal tracking are distinct. Creating a defect through an
explicit defect-authoring flow may seed an `issue` draft and the existing private
record as one named local operation; remote publication still requires its
separate established action. Reading `issue:` offers existing Track as defect
functionality but does not silently create a local obligation. Classifying an
already published plain comment locally does not rewrite that comment.

Severity, category and model provenance remain private unless the user
intentionally includes them in the public body. Do not derive a public
`(blocking)` from private severity. An explicit header decoration such as
`(security)` is public prose; a private category with the same spelling is a
separate datum. No private metadata is appended automatically on promotion.

Neither a website resolution, accepted suggestion, author reply, code change nor
label edit verifies a local defect. Preserve the existing explicit verification
transition and optional separate native resolve action. Removing the `issue`
label does not withdraw a personal defect; the reader may reassess it explicitly.

At a private-note or AI-finding promotion seam already present at intake, compose
into the same durable public draft and preview the exact output. Preserve source
lineage and provenance privately; squashing does not concatenate several leading
labels into a fake multi-label header. The user chooses the resulting comment's
intent. Do not implement the entire still-deferred RFC-BPR-0023 as a hidden
prerequisite. Record which live promotion paths are integrated and leave absent
future private-thread paths as explicit compatibility boundaries, not claimed
runtime behavior.

## Tour compatibility

Import follows the review-tour and `pr-tour` CLI-extension packs. Use their live
ID maps and current shared draft, publication and hook contracts. Guided tour
stops are ordinary comments; they need not acquire a feedback label. An author
may deliberately label a tour comment, but neither authoring defaults nor AI
hooks should turn every explanatory stop into an issue or note automatically.

Do not reorder tour stops, regenerate a tour, overwrite its problem/solution
editors, replace navigation links or reinterpret local tour metadata as public
feedback. A label edit within a supported user-content segment must retain the
surrounding tour structure; generated/managed boundaries remain owned by the tour
engine. Public meaning must still be understandable without BPR.

## Runtime, persistence and effects

Reuse the current application mutation/submission coordinator, exact
host/account/repository/PR/head authority, shared local-owner handoff and durable
drafts. Qt emits intents and renders candidates; it does not publish or persist.
CLI uses the same services, not a GUI subprocess. Save operations use optimistic
revision checks and existing recovery policies across windows and interfaces.

Cache invalidation, draft migration and parse revision changes are local operations
only. A publication request is bound to the exact body, anchor and review event.
An ambiguous transmitted request must be reconciled before any retry; changing a
label while that request is in flight cannot turn a retransmission into a new
undetected post. Follow existing transaction and idempotency mechanisms rather
than inventing a label-specific journal.

## Owner amendment 1 (2026-10-16): no typed label codes

The owner, in chat on 2026-10-16, verbatim: "I think we can get rid of our
custom shortcodes for the conventional comment tags... they don't add much
over just typing it out yourself and the dropdown picker is convenient
enough." [[BPR-1565]] carries it out.

- **Removed.** The comment editor's leading `:xx:` codes (`:qu:` for
  question and the eleven others, from BPR-1079 v2 and [[BPR-1083]]) and the
  completion list a colon at the start opened. This was the typed half of
  the convention-assisted composition above. A label is now typed out in
  full, or chosen with the label chip and its menu (⌘L opens it; ⌘0 in the
  open menu was Plain until owner amendment 2), in the composer and in a
  reply written in place.
- **Kept.** The chip, its menu, the decoration toggles, keyboard access to
  all twelve labels and Plain, recognition of any label typed in full, and
  the one-edit, one-undo-step draft path (R09, R10). The Letters mark style
  still draws each label's two letters (`qu`); they are no longer typed
  codes.
- **Existing drafts.** A draft that holds a code typed while the feature
  still existed and the helper was off, or pasted, keeps it as literal text;
  nothing migrates or rewrites drafts, and the text is published as written.
  No code is a GitHub emoji shortcode (gemoji 4.1.0 has none of the twelve),
  so GitHub and the Markdown reader both show it as written.
- **The remembered authoring preference.** The profile's Label helper stays,
  off by default, with its stored value and `bpr tour hooks` listing
  unchanged. With nothing left to turn on in the editor, it now decides only
  whether a comment workshop request carries the Review feedback guidance
  ([[BPR-1091]]), as it already did; its Settings sentence says so.

- **AM1-01:** Qt, reply-in-place and composed tests show `:qu:` and the
  other codes typed at the start of a comment, with the helper on, stay as
  typed with no label, no list and no control edit (the negative control for
  the removed behaviour), while the chip, its menu, ⌘L and ⌘0 still set every
  label and Plain in one edit.

## Owner amendment 2 (2026-10-16): Plain has no key; ⌘0 is Actual Size

Accepting [[BPR-1566]] on 2026-10-16, the owner noted, verbatim: "I'm really
not sure why plan comment gets a hotkey, I think we can remove that." and,
through the coordinator, that ⌘0 should then behave as it does everywhere
else, View › Actual Size, even with the label menu open. [[BPR-1569]] carries
it out.

- **Removed.** ⌘0 as Plain comment inside the open label menu, and the key
  shown beside Plain, in its tooltip and in its accessible description.
- **Kept.** Plain comment as the menu's first item, chosen by click, arrow
  keys and Return or Space, like every label.
- **⌘0 with the menu open** closes the menu and runs the window's Actual
  Size if it is enabled; it never changes the label or the draft.

- **AM2-01:** Qt tests deliver ⌘0 through the window system to the open
  menu: the window's Actual Size runs once and the menu closes, the label and
  draft are unchanged on a labelled and a plain comment (the negative control
  for the removed key), a disabled Actual Size does nothing, and Plain is
  still reached by arrows and by click.

## Owner amendment 3 (2026-10-17): the label helper is Comments › Comment workshop

The owner asked on 2026-10-16 for the "Label helper" to sit next to the Review
feedback guidance it controls; after amendment 1 its only effect is whether a
comment workshop request carries that guidance ([[BPR-1091]]). Two candidates
were rejected: a switch in the writing hooks panel ("It makes no sense for that
to be under agents & context. Just give it it's own section for comments or
something.") and a Comment workshop group on Reviews & queue ("I think reviews &
queues is too much of a big grab bag of settings and we should find a better
organization."). From three Settings organisations the owner picked the one by
task, with the middle page named Review: Home & queue, Review, Comments.
[[BPR-1597]] carries it out and folds in [[BPR-1568]].

- **Moved.** The helper is Settings › Comments › Comment workshop ›
  **Feedback guidance** ("Send with comment workshop requests", with an info
  glyph), beside **Guidance text**, a link to the Review feedback writing
  instructions, with the group's own Reset. It is no longer a Comment labels
  row, and the label rows' Reset no longer covers it.
- **Kept.** The stored preference (same profile record, column and values,
  off by default; an existing `on` stays on), `bpr tour hooks` and its
  `label_helper` field, and what a workshop request carries. Restore default on
  the Review feedback hook restores only the text; the hook's line says On or
  Off and names Comments › Comment workshop.

- **AM3-01:** Qt and composed tests show the switch on Comments › Comment
  workshop and nowhere else, following the published record, writing the stored
  value, named for assistive technology and found by Settings search, and a
  workshop press carrying the guidance only while it is on.

## Acceptance criteria

- R09: Ordinary and conventional human-only authoring work without any agent or mandatory label.
- R10: Controls and Markdown share one revisioned draft; undo, stale edits and repeated prefix edits are lossless.
- R11: Preview, saves and classification publish nothing; explicit existing publication authorization remains required.
- R12: Native suggestion bodies survive all unrelated transformations, including empty and multiline replacements.
- R13: Applicability follows exact native capability/anchor facts; fallback content never pretends to be an applicable patch.
- R14: All three native review decisions remain explicit and use existing native submission/recovery paths.
- R15: Preflight is useful, scoped, draft-aware and uncertainty-aware, without automatic verdicts or invented merge enforcement.
- R16: Defect tracking and verification remain explicit; public labels and remote resolution do not drive private lifecycle transitions.
- R17: Private severity/category/provenance do not leak into public comment text or public counters.
- R18: Available private-note/finding promotion paths compose through the shared draft; absent deferred paths are not fabricated.
- R19: Tour preparation, normal comments, ordering, native links and independent hook purposes are preserved.
- R42: Crashes, conflicting edits and indeterminate transmission cannot duplicate publication or bind a revised body to an old action.
