---
schema: "implement-rfc.card.v1"
id: "BPR-1084"
title: "Preserve and compose native suggested changes"
status: "Parked"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0050"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-10","milestone":"authoring","acceptance":["R12","R13","R32"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"R12","outcome":"pass","revision":"cc094c1d4","evidence":{"kind":"command","detail":"core_feedback_suggestion_preservation: 25 header edits x 9 fence bodies (multiline, deletion, range, CRLF/tab, Unicode, two fences) x 6 headers through header edit, composition edit, draft plan and tour-stop plan (6,750 runs) plus the pack header-edit cases keep every byte after the header and every anchor; control (CR stripped after the header) fails. application_label_authoring_contract: issue-with-patch reaches the fixture with the exact body, path, side and line. Limitation, recorded on the card for its own card: the desktop QPlainTextEdit draft and reply editors read a CRLF body back as LF, so a CRLF body written elsewhere loses CRs on its first desktop edit; the core keeps CRLF."}},{"criterion":"R13","outcome":"pass","revision":"cc094c1d4","evidence":{"kind":"command","detail":"decide_suggestion_applicability (core, pure): no_patch / native / textual_only with reason, from classify_review_body and plan_review_publication, never relocating; core_suggestion_applicability covers all 18 placement x capability x currency cases and the pack's issue-with-suggestion, suggestion-without-patch and unsupported-native-patch cases; the composed course shows suggestion-without-patch publishing prose only and an unsupported line refused incompatible_body with no file post, text kept and then marked text only. Controls fail."}},{"criterion":"R32","outcome":"pending","revision":"b62420807","evidence":{"kind":"observation","detail":"The suggested-change mark is a 16 px ring around a plus over a minus, solid for native and dashed for text only (macOS look pass with frontend-design, b62420807); qt_review_draft_editor checks the ring at 1x and 2x with a control. Awaits the owner's look: attended question BPR-1084-suggestion-mark-v001, session ed68b8f1."}}]
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

**Note: Windows run at `cd9b87e19`** (holds `35b8d4fcb`; station `bpr-main-win`,
MSVC 14.51, Qt 6.11.2, `windows-dev` Debug, whole tree with no `warning C` or
`warning LNK` line; no remote mutation). The shared [[BPR-1084]] and
[[BPR-1085]] selection passes 19 of 19 with `-j4`:

- core: `core_feedback_suggestion_preservation`,
  `core_suggestion_applicability`, `core_review_preflight` and
  `core_review_submission`;
- Qt: `qt_review_draft_editor`, `qt_review_submission_editor_scale_{100,125,200}`
  and `qt_conversation_cards_view`;
- application: `application_review_submission_{contract,application_host}`
  and `application_review_submission_composition_{public_path,scale_125,scale_200}`
  (33–35 s), `application_label_authoring_contract`;
- `github_review_comment_permalink`;
- `application_review_round_trip_composition_contract`,
  `architecture_boundaries` and `test_network_isolation_manifest`.

There was no MSVC issue to fix, and no row was refused inside the sandbox,
so no pin was needed.

## macOS look pass and owner look v001, 2026-10-06 (bpr-main-mac)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on the
Mac, in worktree `/Users/jaeger/bpr-wt-look` off `origin/main` with the shared
CPM cache, `frontend-design` loaded.

- **macOS rows** at `cd9b87e19`, before any change: the 17 focused rows of
  both cards pass (`core_feedback_suggestion_preservation`,
  `core_suggestion_applicability`, `core_review_preflight`,
  `application_review_submission_{contract,application_host}`,
  `qt_conversation_cards_view`, `qt_review_draft_editor`,
  `qt_review_submission_editor_scale_{100,125,200}`,
  `application_review_round_trip_composition_contract`,
  `application_label_authoring_contract`,
  `application_review_submission_composition_{public_path,scale_125,scale_200}`,
  `architecture_boundaries`, `architecture_boundary_contract`). No macOS-only
  failure.
- **Look** (`8b6a8e2ba`). Reviewed in
  Light, Dark and both high-contrast themes at 1x and 2x. The first build drew
  a rounded box, which is the label marks' shape (BPR-1083: labels square,
  states round), and the text-only strike read as a "%" at 1x. The mark is now
  a ring in the marks' 1.5 px border around a plus over a minus, solid for
  native, broken into the accepted 2.5/1.5 dashes for text only, both in the
  text ink (text only no longer drops to the placeholder grey). The bars sit
  on pixel centres, crisp at 1x with a clear pixel between them.
  `paint_suggestion_mark` draws it for the widget and for tests.
- **Check.** `qt_review_draft_editor` asserts at 1x and 2x that the mark is
  round, the native ring unbroken and the text-only ring dashed, and the glyph
  the same in both with nothing struck across it. Control: a rounded-rect
  frame fails "the mark is round".
- **Integration.** After rebasing onto `origin/main` (`06a3c530b`, which
  brought the CRLF editor fix), 40 rows pass, including the 17 above and every
  appearance, colour, theme, label-mark and attended row; format check and
  clang-tidy on the changed units clean; `DOCUMENTATION_OK`.

### Owner look, session v001

Pending: R32's owner look, not self-certified. One question, Accept or
Reject: `BPR-1084-suggestion-mark-v001` (revision `frozen-d9c5c2573-v001`,
fingerprint `1d3609783bfa…`), "The suggested-change mark reads right at 1x in
every theme: a plus over a minus in a solid ring when GitHub takes the change
as its own suggestion, the same ring dashed when it is text only, with its
words only in the tooltip." It carries a 1x sheet of all four themes beside
BPR-1083's issue marks.

- Session `ed68b8f1a60b465843afab5fc71393b0` (new and short, per AGENTS.md),
  state `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1084-1085-look-v001`,
  repo root the main checkout; host started detached (`nohup`), URL
  `http://127.0.0.1:55376/` (token in the host's `serve.log` in the state
  directory), ready at event cursor 8.
- Frozen root `/Users/jaeger/bpr1084-1085-look-review-v001.Jerfb4`: the
  `git archive` of `d9c5c2573` (pre-rebase; its `src/` and `tests/` are
  identical to the published `b62420807`) built there with the dev settings (Debug, tests
  on), source read-only; executable SHA-256 `cc9922e72e13…`. Offscreen on it,
  `--attended-preflight-look native`, `text-only` and `review` each printed
  their `ATTENDED_LOOK_SHOWN` line (native ring; dashed ring after the
  fixture's 422; chip "Yours on GitHub: 2 open blocking / Yours, not sent: 1
  blocking / Others: 1 open blocking (not yours)").
- Launch: `python3 /Users/jaeger/bpr1084-1085-look-review-v001.Jerfb4/launch.py`
  (native), `--text-only`, `--review`. It checks the build and fixture tree
  first and reports an early exit within seconds.

Next: read the decision. Accept satisfies R32 and the card finishes once its
other criteria stand; Reject comes back with the change in its note for a v002.
