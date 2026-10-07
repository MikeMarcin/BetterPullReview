---
schema: "implement-rfc.card.v1"
id: "BPR-1085"
title: "Integrate reviewer-scoped preflight with native review decisions"
status: "Done"
rfcs: ["RFC-BPR-0048"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-11","milestone":"authoring","acceptance":["R14","R15","R42"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"R14","outcome":"pass","revision":"91a82602e","evidence":{"kind":"command","detail":"Comment, Approve and Request changes keep their exact native event paths (three exclusive buttons per the owner's preflight B, BPR-1079): the composed review-submission course submits approve exactly once with my blocker open and request_changes exactly once after reading the list; request, idempotency key, grant, journal, host and provider unchanged; mutation-branch pin unchanged. 19/19 focused rows re-verified on bpr-main-linux."}},{"criterion":"R15","outcome":"pass","revision":"91a82602e","evidence":{"kind":"command","detail":"prepare_review_decision / review_decision_changes bind draft, account, target, head, summary, disposition and selected drafts; a change to any invalidates the prepared action (refused before and after the existing write confirmation, naming what changed), while resolutions, other people's labels and unselected drafts do not; core_review_preflight and the composed stale-selection/summary cases pass, with failing-before controls (15 mutations recorded in the ledger)."}},{"criterion":"R42","outcome":"pass","revision":"91a82602e","evidence":{"kind":"command","detail":"project_review_preflight is reviewer-scoped: other reviewers' blockers appear only under Others, an unknown author is uncertainty never mine, and the projection carries no disposition; the composed course shows a label edit, a relabel to blocking and the last resolution submit nothing and leave the chosen Approve in place."}},{"criterion":"owner-look","outcome":"pass","revision":"frozen-d9c5c2573-v001","evidence":{"kind":"observation","detail":"Owner Accept, no note: attended session ed68b8f1a60b465843afab5fc71393b0, candidate BPR-1085-preflight-v001 (fingerprint 6a263ac89772...), decision 3824636b-5a54-461a-8c59-9ea00c13541e, event 10; preflight published in b62420807. Platforms: Linux 19/19 at 91a82602e, Windows 19/19 at cd9b87e19, macOS 40 rows at b62420807 in all four themes. Linux/Windows runs of b62420807 are owed station commands on the card; R33's native matrix stays unrun."}}]
---

# BPR-1085: Integrate reviewer-scoped preflight with native review decisions

Pack alias: **PACK-CC-11** (live `BPR-1085`, allocated at import).

## Outcome and work

Add the selected preflight projection to the current review submission flow. Distinguish own published blockers, selected pending comments, advisory label guidance and unavailable coverage. Retain explicit native decisions and the existing provider/submission journal.

## Acceptance

- Comment, Approve and Request changes use their normal exact native event paths.
- Changes to body/selection/head/account invalidate only applicable prepared actions.
- No label or last-resolution event triggers a review verdict; other reviewers are not attributed to the submitter.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation (2026-10-06)

Built on `bpr-main-linux` in `/home/mmarcin/BetterPullReview` on `main`.
Engineering owner `rfc-implementer`, model `claude-opus-5-5`, effort `high`.
Product commits `902d64132` (core) and `281e552ed` (application, Qt,
composition, course and pin reseal). The run ledger's BPR-1085 section has
the full evidence.

- **Core.** `bpr/core/review_preflight.hpp` defines two functions.
  - `project_review_preflight` reads the shared feedback population, its
    annotation set and the selected unsent drafts for the submitter's exact
    provider identity. It returns four separate groups: `Blockers`, `Pending`,
    `Guidance` and `Uncertain`. Other reviewers are counted in `Others` and
    never added to `Blockers`.
  - `prepare_review_decision` and `review_decision_changes` bind one Submit
    gesture to the draft, target, account, disposition, summary and selected
    drafts, and say which of these changed.
- **Application.** `application::observe_review_preflight` reads each
  accepted snapshot once. Conversation identities are the ones Discuss uses.
- **Composition.** The existing `review_submission_workflow` receives the
  observation and the window's unsent comment draft. It presents the
  preflight only for the draft's own account and head. Submit carries the
  decision the editor showed, and the workflow refuses it if anything bound
  has changed. The request, idempotency key, grant, journal, host and
  provider are unchanged.
- **Qt.** Comment, Approve and Request changes are three exclusive buttons,
  as the owner chose in preflight B. The "Your blocking feedback" chip shows
  marks and counts, and opens a list whose rows reveal the item they name.
  Approve carries an amber dot, and its tooltip says approving does not
  resolve my blocking comments. Exact strings are in the
  [delivery contract](../../ux/conventional-comments-feedback.md#review-submission-preflight-bpr-1085-rfc-bpr-0048).

### Acceptance

| Criterion | Status | Evidence |
|---|---|---|
| Comment, Approve and Request changes use their normal exact native event paths. | Passing (fixture) | `application_review_submission_composition_public_path` submits each decision once through the existing host and journal (3 + 3 submissions). Approve with my blocking feedback open sends `approve`. Request changes after inspecting it sends `request_changes` once. |
| Changes to body/selection/head/account invalidate only applicable prepared actions. | Passing | `core_review_preflight` covers the summary, disposition, selected draft (label edit or deselection), head, account, pull request and draft changes. An unselected draft, a resolution or another reviewer's label changes nothing. The course refuses a Submit from a stale selection and from a stale summary, then submits the current view once. |
| No label or last-resolution event triggers a review verdict; other reviewers are not attributed to the submitter. | Passing (fixture) | In the course, the server's label edits, a label changed to blocking, and the resolution of my last blocker submit nothing and keep the chosen Approve. Another reviewer's blocker shows only under "Others". |
| Owner look at the preflight surface | **Open** | It follows existing patterns; the frontend-design skill is not installed on this station. R33's native matrix has not been run. |

Each new check has a failing-before control, listed in the ledger. No live
GitHub was used, and Linux is the only platform run.

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
CPM cache, `frontend-design` loaded. The macOS run of this card's focused
rows at `cd9b87e19` is recorded on [[BPR-1084]]: all pass, none macOS-only.

- **Attention role** (owner, 2026-10-06: "a proper palette role (e.g.
  attention) with per-theme defaults on Appearance › Colours, rather than
  fixed hex, per RFC-BPR-0034"). `core::appearance_role::state_attention`
  joins the vocabulary after the label tints (60 roles), so no stored ordinal
  moves; a palette or theme document without it reads the built-in value.
  Built-ins `#AD7A00` Light, `#D29922` Dark, `#8A5300` High contrast light,
  `#FFBC66` High contrast dark, each at least 3:1 on its surface and code
  background (`#BF8700` was 2.93:1 on Light). Appearance › Colours lists it as
  "State attention" under State stale (58 offered); the list is now ordered by
  group and then the core's order. RFC-BPR-0034 records the direction and
  C5's count. The composition passes the theme's value through
  `workbench_window::set_attention_colour` to
  `review_submission_editor::set_attention_colour`; the fixed hex is gone.
- **Chip icons.** The draft icon uses the marks' 16 px square, 1.5 px border
  and accepted dashes with a pencil; the "?" uses the marks' ring.
- **Attended look** (tests-on): `--attended-preflight-look
  native|text-only|review` on `conventional-comments-v1` stages two of the
  fixture's thread roots as the fixture account's blocking feedback, writes a
  blocking comment with a suggested change through the window's own Comment
  action (text-only: one Publish the fixture refuses with its synthetic 422),
  and for `review` opens Compose review. Nothing reaches GitHub.
  `application_attended_launch_contract` refuses the look without its file,
  on another fixture or with an unknown name.
- **Pins.** Round-trip full-application and production-manifest pins
  resealed for the composition's one `set_attention_colour` call, the tests-on
  accessor and `main.cpp`'s tests-on look; mutation branch unchanged.
- **Checks with controls.** `qt_appearance_profile` (old `#BF8700` fails
  3:1), `qt_review_submission_editor_scale_100` (a fixed hex fails "a theme's
  own attention amber repaints the dot"), `qt_colour_role_list` (index order
  fails "the attention amber's row sits directly under State stale"); counts
  updated in `core_theme_exchange`, `qt_appearance_role_display`,
  `qt_colour_role_list`, `qt_appearance_settings_panel`; the theme fixture
  gains `state_attention`. Product commit `b62420807`.
- **Integration** after rebasing onto `origin/main`: 40 rows pass; format
  check and clang-tidy clean; `DOCUMENTATION_OK`. Linux and Windows have not
  run these changes.
- **Not changed, for the coordinator:** BPR-0648/BPR-1290's open-thread amber
  is still a fixed `#D29922` passed by the composition
  (`conversations->cards()->set_colours(..., QColor{ 0xD2, 0x99, 0x22 })`).
  It could take `state_attention` too, which would change Light's thread
  amber to `#AD7A00`; that is an accepted surface, so it was left alone.

### Owner look, session v001

Pending: owner-look, not self-certified. One question, Accept or Reject:
`BPR-1085-preflight-v001` (revision `frozen-d9c5c2573-v001`, fingerprint
`6a263ac89772…`), "The review submission preflight reads right in every
theme: three decision buttons, the "Your blocking feedback" chip (filled
issue mark with my open count, dashed draft mark with my unsent count, ? for
uncertain) and Approve's amber dot, now the State attention colour on
Appearance > Colours." It carries a 2x sheet of all four themes.

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

Next: read the decision. Accept satisfies owner-look and the card can finish;
Reject comes back with the change in its note for a v002.

## Owner verdict on look v001, 2026-10-06: Accept

Session `ed68b8f1a60b465843afab5fc71393b0`, read with `status --after 8`:
event 10 records **Accept** on `BPR-1085-preflight-v001` (revision
`frozen-d9c5c2573-v001`, fingerprint `6a263ac89772…`, decision
`3824636b-5a54-461a-8c59-9ea00c13541e`, saved 2026-10-07T02:18:28Z, actor
human) with no note; event 11 hands back with nothing pending. The owner look
is satisfied on the preflight published in `b62420807`, Approve's dot in the
State attention role included. R14, R15 and R42 stand on their recorded
evidence. The session was released and its frozen root deleted (recorded on
[[BPR-1084]]).

### Platform evidence

- **Linux** (`bpr-main-linux`): 19 of 19 focused rows at `91a82602e` (above).
- **Windows** (`bpr-main-win`, MSVC 14.51, Qt 6.11.2, `windows-dev`): the
  shared selection 19 of 19 at `cd9b87e19` (above).
- **macOS**: the focused rows at `cd9b87e19`, 40 rows after the look change
  (`b62420807`) in all four themes, and the owner's look (above).

### Owed station runs (not blocking)

Linux and Windows have not run `b62420807`'s `state_attention` role, the chip
icons and the resealed pins. The criteria name no per-platform run, so this is
owed, not a blocker. The commands are on [[BPR-1084]]; add
`application_composition_contract` and
`application_review_round_trip_composition_contract` for the resealed pins.
R33's native matrix stays unrun, as recorded above.


## Windows station run (bpr-main-win, 2026-10-06)

At `fe64f5541`, a clean tree, on Windows 11 with MSVC:

- **Tree.** `build/windows-dev-tour-cli`, the `windows-dev` preset with
  `-DBPR_BUILD_TOUR_CLI=ON`, Debug.
- **Build.** Rebuilt with no `warning C` line.
- **Runner.** The network-isolated runner, `ctest -j4`. The Qt rows ran on
  the offscreen platform, so nothing touched the desktop.

One pass ran BPR-1290's expression and BPR-1084/1085's together. All 32 rows
pass, 32 of 32: 27 contained and 5 under their existing `WINDOWS_UNCONTAINED`
pins.

This card's owed rows pass:

- [[BPR-1084]]'s 16 rows, including the appearance rows that carry the
  `state_attention` role and the chip icons;
- `application_composition_contract` (1045.7 s, uncontained);
- `application_review_round_trip_composition_contract`, for the resealed
  pins.

The Windows half of the owed run is discharged. R33's native matrix stays
unrun, as recorded above.
