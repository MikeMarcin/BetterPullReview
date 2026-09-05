---
schema: "implement-rfc.card.v1"
id: "BPR-0503"
title: "Qt: finding queue and before/after comment workshop"
status: "Done"
rfcs: ["RFC-BPR-0012"]
depends_on: ["BPR-0498","BPR-0500"]
---

# Qt: finding queue and before/after comment workshop

A finding queue presents delivered findings with their anchor state and offers
exactly the six actions `docs/ux/ai-experience.md` names: open evidence, ask
follow-up, convert to local draft comment, convert to local draft defect, add
to private notes, and dismiss. The workshop lives inside
`src/adapters/qt/include/bpr/qt/review_draft_editor.hpp`, keeps the reviewer's
original draft intact, and shows a proposed revision beside it. Both obey the
editor's existing rule: the only way out of this widget is a semantic command.

## Required final state

- The queue shows severity, confidence, category, claim, evidence,
  consequence, and the anchor state, so a reviewer sees that a finding did not
  land before acting on it.
- Each of the six actions emits one intent naming the exact finding. The
  widget performs no promotion itself and reaches no store.
- The workshop displays the original and the proposed revision at the same
  time. Accepting emits one `review_draft_command`; rejecting leaves the
  original byte-identical.
- Neither widget holds a provider, endpoint, store, or process member.
- Surface decision, recorded rather than assumed: no Markdown rendering
  surface exists anywhere in the tree, so the before/after view is plain text
  in the editor's existing presentation. Adding a renderer to the Qt adapter
  for one surface is the cost the implementing slice would have to justify in
  the ledger if it decides otherwise.

## Non-goals

- The one-command promotion rule and its refusals — BPR-0498.
- Chat, chips, and the stale banner — BPR-0502.
- Decode, bounds, and delivery — BPR-0500.
- Publishing — RFC-BPR-0008's controls, unchanged by this card.

## Required direct evidence

- A new `qt_finding_queue_view` registered in
  `tests/adapters_qt/CMakeLists.txt` beside `qt_review_draft_editor`, with the
  workshop cases extending the existing `qt_review_draft_editor` suite; run
  `ctest --preset dev -R '^(qt_finding_queue_view|qt_review_draft_editor)$'
  --output-on-failure`.
- A table-driven case over all six actions asserts one intent each carrying
  the exact finding identity; a seventh action added without a case fails the
  count.
- Accepting a workshop revision emits exactly one `review_draft_command` and
  performs no store; rejecting leaves the original compared byte for byte.
- Both texts are asserted on two separately presented values, so a single
  buffer that overwrote the original fails.
- A field-reading guard fails to compile when a provider, endpoint, or store
  member is added to either widget.
- Each assertion is shown to fail when the behaviour it constrains is
  neutered.

## Shape inherited from BPR-0498 — 2026-09-01

`promote_agent_finding` promotes *onto* an existing draft or defect (an
`edit` of a named draft, a `reclassify` of a named defect) because the core's
command vocabularies create neither; only the private note is a payload of
its own. The "convert to local draft comment" and "convert to local draft
defect" actions therefore create the target through the existing editor and
ledger paths first and then apply the promotion command; a target anchored
anywhere but the finding's line is refused as `anchor_elsewhere`, and a
boundary-anchored finding can only become a note.

## What this slice delivered — 2026-09-02

`src/adapters/qt/include/bpr/qt/finding_queue_view.hpp` and its `.cpp` are new;
`review_draft_editor` gained the workshop and had its members gathered into one
`review_draft_editor_members` aggregate so the same guard technique BPR-0502
established applies to it.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The queue shows severity, confidence, category, claim, evidence, consequence, and the anchor state, each as its own presented value | `ctest --preset dev -R '^(qt_finding_queue_view\|qt_review_draft_editor)$' --output-on-failure` | Pass, 0.20 s. Ten disclosed fields are swept; each must move its own field and nothing else. Freezing the presented confidence fails `the confidence field did not move`; freezing the anchor state fails `the anchor state field did not move`; giving every queue row one finding's identity fails `every delivered finding is listed, including the unanchored one` | this commit |
| Exactly six actions, each emitting one intent naming the exact finding, with no promotion and no store | same suite | Pass. A table-driven case covers all six against `finding_queue_action_count`; each emits one `finding_action_intent` carrying the session, the ordinal, the finding, and the verdict. Dropping one action's intent fails `add_to_private_notes must emit exactly one intent, no more and no fewer` | this commit |
| A seventh action added without a case fails the count | rebuild with a seventh enumerator | Compile failure: `static assertion failed ... every declared finding action must have a case`, `expression evaluates to '6 == 7'` | this commit |
| The workshop shows the original and the proposed revision at once as two separately presented values | `ctest --preset dev -R '^qt_review_draft_editor$'` | Pass, 0.45 s. Two `QPlainTextEdit` surfaces, each asserted against its own source. Pointing both at one buffer fails `the original, the proposal, and the editable body are three separately announced surfaces`; deleting the proposal's presentation fails `and the proposed revision is presented as a second, separate value at the same time` | this commit |
| Accepting emits exactly one `review_draft_command` and reaches no store | same suite | Pass. One `edit` naming the proposal's draft, carrying the proposed text byte for byte, with the receipt path untouched and the body unwritten. Emitting two fails `accepting a revision emits exactly one review draft command`; emitting zero fails the same line | this commit |
| Rejecting leaves the original byte-identical | same suite | Pass. The body and the accepted draft projection are compared byte for byte against a fixture carrying a tab and a trailing space. Writing the proposal back on rejection fails `rejecting a revision asks for nothing at all` | this commit |
| Neither widget holds a provider, endpoint, store, or process member | rebuild with a `void*` store member added to each | Compile failure in both guards: `type 'const qt::finding_queue_view_members' decomposes into 15 elements, but only 14 names were provided` and `type 'const qt::review_draft_editor_members' decomposes into 20 elements, but only 19 names were provided`. Each aggregate is also pinned by `sizeof` against a probe, so a member added to the class outside it fails too | this commit |
| Nothing regressed | `ctest --preset dev -R '^(qt_side_chat_panel\|architecture_boundaries\|core_agent_finding_promotion)$'` | Pass 3/3: `architecture_boundaries` 6.18 s, `core_agent_finding_promotion` 0.07 s, `qt_side_chat_panel` 0.35 s | this commit |
| Every negative control fails, and every restore is byte-exact | 20 scripted mutations, each rebuilt and re-run | 20 of 20 failed by name; 0 survivors; every touched file restored with a matching md5 | this commit |

The surface decision is recorded in `docs/verification/RFC-BPR-0012.md` under
**Surface decision (BPR-0503)**: plain text, because the comparison is between
the reviewer's bytes and the agent's bytes and a renderer would show neither,
and because a rendered surface has no bytes for "byte-identical" to be about.

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0) and ran
`ctest --preset dev -R '^(qt_finding_queue_view\|qt_review_draft_editor\|qt_side_chat_panel\|architecture_boundaries\|core_agent_finding_promotion\|application_agent_finding_protocol\|qt_composed_github_scale_100)$' --output-on-failure`:
7/7 — `qt_finding_queue_view` 0.07 s, `qt_review_draft_editor` 0.22 s,
`qt_composed_github_scale_100` 0.94 s. `clang-format --dry-run --Werror`
clean on all six touched files; `DOCUMENTATION_OK`; `git diff --check`
clean. The `PASS_REGULAR_EXPRESSION` the editor suite gained is kept: a
suite that cannot fail closed is not evidence. The worker's risk that a
dispatch could swallow `anchor_elsewhere` is carried to [[BPR-0504]].
