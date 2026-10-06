---
schema: "implement-rfc.card.v1"
id: "BPR-1084"
title: "Preserve and compose native suggested changes"
status: "In Progress"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0050"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-10","milestone":"authoring","acceptance":["R12","R13","R32"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1084: Preserve and compose native suggested changes

Pack alias: **PACK-CC-10** (live `BPR-1084`, allocated at import).

## Outcome and work

Connect convention-assisted comments to existing suggestion composition/preview and exact native anchor checks. Preserve fences, replacement text and empty/multiline changes during prefix edits. Keep unsupported locations textual and provide the existing precise native/browser path rather than fake apply.

## Acceptance

- The fixture transformations leave every unrelated suggestion byte and semantic anchor unchanged.
- Native and unsupported/fallback contexts are distinguishable without guessed line relocation.
- Issue-with-patch and suggestion-without-patch both work in the public composition.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Scope decision — 2026-10-06

The pack's "connect to existing suggestion composition/preview" has no target
for ordinary pull-request comments. The only suggestion composer is Review
Cycles' (`suggestion_composer`, `review_cycle_suggestion`). A comment carries
a hand-typed ```` ```suggestion ```` fence, and native or fallback is decided
only at Publish (`classify_review_body` → `incompatible_body`). The integrator
approved this scope on 2026-10-06, as coordinator:

1. **R12 byte preservation:** a matrix through every label and prefix edit the
   core offers (multiline, deletion, range, CRLF and tab, Unicode, two fences),
   with every byte outside the header span and every semantic anchor
   unchanged.
2. **A decision before Publish:** a pure core decision — native, textual-only
   or no patch — from the body and the existing placement and capability
   facts, with no line relocation. It is shown in the comment draft editor
   **and in [[BPR-1290]]'s in-place Reply editor**, since replies can carry
   fences too. It is shown as a small mark or tooltip-level text only, per the
   owner's minimal-text preference.
3. **Public composition on the local fixture:** issue-with-patch,
   suggestion-without-patch and unsupported-native-patch. No live GitHub.

No new suggestion editor is built. The UI-visible change keeps the owner-look
criterion open for a Mac session. The frontend-design skill is not installed
on this station, so the presentation follows the existing editor patterns.

## Implementation — 2026-10-06 (bpr-main-linux)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on
`bpr-main-linux`, on `main` from the claim `9498da49e` and the scope decision
above. Product commits `d8d842ae9` (core) and `5929c089a` (Qt and the composed
course); the pin reseal and these records follow in the next commit.

### What was built

- **R12 preservation matrix** (`d8d842ae9`). `core_feedback_suggestion_preservation`
  runs 25 header edits (twelve relabels, Plain, each expectation, a label with
  a weight, `if-minor`, `security` and `blocking` in and out, a label with a
  decoration) over nine bodies under six headers (plain, `issue:`,
  `issue (blocking):`, the strong form, a custom decoration, a capitalised
  label) through all four entry points: `decide_feedback_header_edit`,
  `decide_feedback_composition_edit`, `plan_feedback_draft_edit` applied by the
  draft's reducer as a root and as a reply, and `plan_feedback_tour_stop_edit`
  applied by the tour owner. The bodies carry a multiline replacement with
  tabs, an empty (deletion) fence, the pack's `suggestion:-1+0` CRLF case, a
  four-backtick fence with a blank line and trailing spaces, a tilde fence with
  Markdown after it and no final newline, Unicode, two fences (the second a
  range), a spaced info string, and prose only. Each change is checked as: the
  bytes before the header kept, every byte after it kept, the header ending
  before the first fence, every fence byte for byte, the same
  `classify_review_body` kind and the same applicability; the draft's line,
  thread, identity and scope and the stop's ranges and destination unchanged.
  The pack's five header-edit cases run through the same four. 1,350
  body-and-edit pairs, 1,017 changes, 90 refusals, 6,750 entry-point runs.
- **The decision before Publish** (`d8d842ae9`).
  `core::decide_suggestion_applicability(body, suggestion_placement_observation)`
  returns `no_patch`, `native` or `textual_only` with a reason (`base_side`,
  `file_subject`, `outdated_line`, `unavailable_anchor`, `conversation`,
  `inline_comment_unsupported`, `invalid_observation`). A body is a patch when
  `classify_review_body` says it needs a line subject; a current head line is
  planned with `plan_review_publication` from `initial`, so native is exactly
  the policy's native attempt and text only exactly its `incompatible_body`
  refusal. No provider call, and no field a line could be moved through.
  `core_suggestion_applicability` covers each subject, negative controls (prose,
  inline code, another language's fence, two backticks, a bare fence after a
  label), all 18 placement × capability × currency combinations against the
  policy, and the pack's `issue-with-suggestion`, `suggestion-without-patch` and
  `unsupported-native-patch` cases from `native-integration-cases.json`.
- **The mark** (`5929c089a`). `qt::suggestion_mark` is a 16 px painted glyph,
  hidden with no patch: a plus over a minus in a box for native, the same struck
  through with a dashed box for text only (shape, not only colour). No visible
  text: the accessible name is "Suggested change, native" or "Suggested change,
  text only", and the tooltip and accessible description say, for native,
  "Suggested change: posted on this line as GitHub's own suggestion, which is
  applied on GitHub." and, for text only, "Suggested change, text only: <reason>.
  GitHub cannot apply it here. Your text is kept as written, and nothing moves
  to another line." It sits beside Publish in `review_draft_editor` and beside
  the label chip in the inline drawer's reply editor (`open_thread_card`). It
  gates nothing, emits nothing and changes no byte.
- **Facts it reads.** A root comment: its own line and side, placement unknown
  (as production publishes it). A reply: its thread's facts from the accepted
  projection, `qt::suggestion_placement_of` (GitHub's file subject, outdated
  flag, current line and side), carried on `thread_card::Suggestion`; the reply
  editor reads its card, and `workbench_window::present_reply_placement` passes
  the same to the draft editor (absent: no claim). After an `incompatible_body`
  refusal the draft editor marks that exact line text only
  (`inline_comment_unsupported`) before the next Publish.
- **Public composition** (`5929c089a`). The tests-on `--label-authoring-probe`
  over `conventional-comments-v1` continues after its labelled comment, in Read:
  an `issue:` comment with a multiline `suggestion` fence on the next free line
  is marked native before Publish, and the fixture server receives one native
  post whose decoded body equals the draft and whose path, side (`RIGHT`) and
  line equal its anchor; its thread returns as an issue. A `suggestion:` prose
  comment shows no mark and publishes exactly its prose. A patch on a line the
  fixture refuses an inline comment for (`BPR-1084 off-hunk` in its body,
  the fixture's synthetic 422) is marked native before the first attempt (no
  evidence yet), then refused with `incompatible_body` after one native attempt
  on that exact line: no file post, no thread, the text and line kept, and the
  mark now text only. A reply written in place in `PRRT_cc_1` with a range
  patch is marked native in the reply editor and in the draft editor.
  `LABEL_AUTHORING_EVIDENCE` grows from 9 to 15 bits, all 1.

### Verification (Linux x86_64, Debug `build/dev`, `build/core-only`)

- `core_feedback_suggestion_preservation`, `core_suggestion_applicability`,
  `core_feedback_{convention,header_edit,draft_edit,conversation,partition}`,
  `core_dependency_boundary`, `core_boundary_contract`, `architecture_boundaries`,
  `architecture_boundary_contract`: pass (dev and core-only for the core rows).
- `qt_review_draft_editor`, `qt_conversation_cards_view`,
  `qt_feedback_label_{marks,controls,threads}`, `qt_composed_github_scale_{100,125,200}`,
  `application_label_authoring_contract`, `github_review_comment_permalink`,
  `application_review_round_trip_composition_contract` (resealed): pass after
  rebasing onto `46f2a997e` (BPR-1290's reply label helper merged beside the mark).
- Integration pass at `c246626a8`: every `qt_`, `application_` and
  `github_review` row (343) with `-j4`, `DISPLAY` and `WAYLAND_DISPLAY` unset:
  341 pass. `application_diagnostic_controls_public_path` needs a display (it
  aborts loading the xcb plugin, not this card's code);
  `application_review_submission_composition_public_path` timed out waiting for
  shutdown under `-j4` load (43.6 s) and passes alone in 4.2 s.
  `coding-format-check` (clang-format 22.1.8), `git diff --check` and
  `verify_documentation.py` pass.
- Failing-before controls, each reverted and the row green again:
  - the header edit stripping CR after the header:
    `core_feedback_suggestion_preservation` fails "change-label-with-native-patch
    on issue (blocking): Bound the copy.\r\n\r\n```suggesti [header]: every byte
    after the header kept";
  - every fence called native whatever its subject:
    `core_suggestion_applicability` fails "the base side is text only";
  - the draft editor never presenting its mark: `qt_review_draft_editor` fails
    "a patch on a new right-side line is native, said in the tooltip";
  - the reply editor never presenting its mark: `qt_conversation_cards_view`
    fails "t-line: the mark says native";
  - the window not passing the reply's thread facts:
    `application_label_authoring_contract` prints
    `LABEL_AUTHORING_EVIDENCE:111111111111110` and "reply-mark inPlace=native
    editorMark=hidden".

### Limits and findings

- **Owner look: open.** The mark is a UI-visible change; the owner has not seen
  it. The frontend-design skill is not installed on this station, so the glyph
  and placement follow the existing editor and drawer patterns (16 px
  conversation glyphs, tooltips). R33's native matrix is not run.
- **Before the first attempt a root is marked native.** Production has no
  placement evidence before Publish (`observe_review_source` reports
  `review_placement::unknown`, and the off-hunk capability cache lives inside
  the GitHub session), so a root on a line GitHub will refuse says native until
  the refusal; the refusal never relocates or falls back for a suggestion.
- **No browser handoff exists** for a single comment or thread, so none is
  offered and none was invented; applying a native suggestion is GitHub's.
- **Base side is text only** by a conservative reading of GitHub's suggestion
  support (applied to head lines); not verified against a live host.
- **Finding, not fixed here: the Qt editors drop CR.** Both the draft editor
  (`QPlainTextEdit`) and the reply editor read back a CRLF body as LF, so a draft
  written with CRLF elsewhere (CLI, agent) loses its CRs on the first edit in the
  desktop, a label chip edit included. The core preserves CRLF (matrix above);
  the desktop editors do not. This is an R12 gap in the GUI composition path for
  the coordinator to place.
- **The composed course covers a reply on a current line only.** Outdated, file
  and base-side replies are covered by the Qt rows; the fixture has no such
  thread.
- **Platforms.** Linux only. macOS and Windows runs are owed.
- Live GitHub was not contacted; the fixture's 422 is synthetic.
