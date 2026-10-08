---
schema: "implement-rfc.card.v1"
id: "BPR-1090"
title: "Author canonical default convention guidance and hook ownership map"
status: "Parked"
rfcs: ["RFC-BPR-0051"]
depends_on: ["BPR-1076", "BPR-1086"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-16","milestone":"hooks-cli","acceptance":["R34","R35","R36","R37"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"R34","outcome":"pass","revision":"54b4b89eb","evidence":{"kind":"command","path":"tests/application/writing_delivery_tests.cpp","detail":"application_writing_delivery: the shipped review-feedback-v1 default names all twelve catalog IDs with their meanings, blocking/non-blocking/if-minor, plain prose, uncertainty kept as questions, issue not a GitHub Issue, the suggestion label vs a GitHub suggested change and fenced suggestion block, and Request changes as a separate action; no model is needed and every core feature is unchanged with the label helper off (nothing sent). Controls fail."}},{"criterion":"R35","outcome":"pass","revision":"54b4b89eb","evidence":{"kind":"command","path":"tests/application/tour_cli_tests.cpp","detail":"tour_cli_composition: bpr tour hooks lists problem, solution, outline, comment, feedback with versions, customized and active, plus label_helper, directly and through a running owner; feedback edit survives a new process and leaves the tour hooks unchanged; reset restores the shipped text. sqlite_workbench_settings_storage: v15 migrates keeping the four overrides, revision and window summary fractions, feedback empty (default); override stored as written; reset stores empty. Tour hooks unchanged and still ask for no label. Ledger hook ownership map uses the real names and sources."}},{"criterion":"R36","outcome":"pass","revision":"54b4b89eb","evidence":{"kind":"command","path":"tests/application/writing_delivery_tests.cpp","detail":"The feedback hook builds no writing prompt and decodes no writing result; assist context --hook feedback is invalid_hook; the Settings Test is disabled and test_writing_hook refuses it before any launch. No new grant, provider, network or publication path (round-trip pins resealed, mutation pin unchanged)."}},{"criterion":"R37","outcome":"pass","revision":"54b4b89eb","evidence":{"kind":"observation","detail":"Card scope: the default guidance tells the agent to keep the reviewer's label, uncertainty and suggestion bytes, to return a local draft candidate only, and not to reveal private severity, provenance or model identity (asserted in application_writing_delivery). Enforcing this on workshop candidates is BPR-1091's."}},{"criterion":"BPR-1090:owner-look","outcome":"pending","revision":"54b4b89eb","evidence":{"kind":"freeform","detail":"Attended session df50c8413c8a1b3a29db57916d6b8941 (state /Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1090-feedback-row-v001), candidate BPR-1090-feedback-row-v001 on the frozen build /Users/jaeger/bpr1090-feedback-hook-look-v001.VnZ20v; one Accept/Reject question on the Review feedback Settings row."}}]
---

# BPR-1090: Author canonical default convention guidance and hook ownership map

Pack alias: **PACK-CC-16** (live `BPR-1090`, allocated at import).

## Outcome and work

Refine the supplied fragment against the actual canonical shared skills and effective imported hook configuration. Identify relevant feedback/workshop tasks and excluded tour problem/solution/outline tasks. Record version and override behavior without overwriting consumer copies.

## Acceptance

- All twelve labels and native review/suggestion distinctions are taught without a model requirement.
- The hook ownership map uses real installed task names and canonical sources.
- User/project overrides and disabled modes have explicit non-destructive upgrade behavior.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Owner decision, 2026-10-08 (chat): a fifth profile hook, "Review feedback"

Question, as the coordinator relayed it: "Add a fifth profile writing hook,
'Review feedback', holding the default conventions guidance (editable in Agents
& context and via `bpr tour hooks`, off when the label helper is off) — instead
of putting it in the shared AgentSkills repo …?"

Owner's answer, verbatim: "Accept (Recommended)"

This settles the canonical home the run ledger left open (Hook and skill
owners, R35): the guidance is a BPR-owned profile hook under the existing
writing-hook settings authority, not a shared AgentSkills edit. Tour hooks keep
their purposes. Sending the guidance with workshop requests is BPR-1091's.

## Implementation (2026-10-08, macOS)

Product commit `6ae4dbb77`, pins resealed in `54b4b89eb`, rebased onto
`1e277adcc`. The run ledger's "Hooks: BPR-1090" section has the hook ownership
map and the decisions.

- **Core**: `writing_hook::feedback`, `writing_hook_count` 5,
  `review_preparation_hook_count` 4. No `writing_target` names the feedback hook.
- **Default**: `src/application/resources/review-preparation/review-feedback.md`,
  version `review-feedback-v1`, from the pack fragment. It teaches the twelve
  catalog labels by ID and meaning, `blocking`/`non-blocking`/`if-minor`, plain
  prose, uncertainty kept as questions, `issue` not a GitHub Issue, a
  `suggestion` label not a suggested change, and the review decision as a
  separate action. 3,165 bytes (an override may hold 8,192).
- **Accessor**: `application::workshop_feedback_guidance(hooks, label helper)`;
  sending it is BPR-1091's.
- **Store**: settings schema 16, `writing_feedback`, empty for the default.
- **CLI and owner channel**: `bpr tour hooks show|edit|reset feedback`; the
  listing gives each hook's `version` and `active` and the `label_helper`, and
  its text reads `feedback (shipped) · off while the label helper is off`.
- **Settings**: Agents & context › AI assistance lists Review feedback below a
  separator, with an On/Off line and Test unavailable (frontend-design loaded).

### Acceptance evidence

| Criterion | Evidence |
|---|---|
| All twelve labels and the native review/suggestion distinctions are taught without a model | `application_writing_delivery`: the default names each catalog ID with its meaning, the three decorations, "GitHub suggested change", the fenced `suggestion` block, Request changes and "does not create a GitHub Issue"; no model is launched anywhere in the course. |
| The hook ownership map uses real installed task names and canonical sources | Ledger table: `problem`, `solution`, `outline`, `comment`, `feedback` as `bpr tour hooks` names them, their resource files and columns; workshop and AI review have no text of their own. |
| Overrides and disabled modes have explicit non-destructive upgrade behavior | `sqlite_workbench_settings_storage` `test_feedback_hook_override_migrates_from_fifteen_and_survives`: a v15 file migrates keeping all four tour overrides, their revision and each window's summary fraction, and reads the feedback hook empty; an override is stored as written and survives a reopen; reset stores it empty; an oversize one is refused. `application_writing_delivery`: an empty override follows the default, an override replaces it, reset restores it, and the default text is pinned to its version. Label helper off sends nothing, on sends the effective text (`workshop_feedback_guidance`). |
| R35 inspectable hooks; tour hooks keep their purposes | `tour_cli_composition`: five hooks in stable order with versions; feedback off then on with the label helper; edit leaves the tour hooks unchanged and survives a new process; reset restores the shipped text; `assist context --hook feedback` is `invalid_hook`; through a running owner the fifth hook and `label_helper` are listed and an edit is refused. `qt_settings_window` (and 125/150/200%): the row's version, override, Restore and On/Off line, Test disabled. `application_review_tour_writing`: in the composed window the real Settings label-helper switch turns the row On and Off. |
| R36 no background inference or new authority | The hook adds text only; it builds no tour prompt and decodes no result (`application_writing_delivery`); the Settings Test refuses it before any launch. |

### Failing-before controls (each reverted)

| Mutation | Fails at |
|---|---|
| Migration drops `summary_fraction` | `sqlite_workbench_settings_storage`: "each window keeps its summary fraction" |
| Feedback override bound to the stop column | `sqlite_workbench_settings_storage`: "profile defaults persist" |
| `workshop_feedback_guidance` ignores the helper | `application_writing_delivery`: "the label helper off sends no guidance" |
| `build_writing_prompt` admits the feedback hook | `application_writing_delivery`: "no tour writing prompt is built from the feedback hook" |
| Default text edited, version kept | `application_writing_delivery`: "review-feedback-v1 names exactly this default text" |
| Listing omits "off" | `tour_cli_composition`: "the listing says the hook is off while the label helper is off" |
| `active` always true | `tour_cli_composition`: "a new profile follows the default, and with the label helper off it is not sent" |
| Test enabled for feedback | `qt_settings_window`: "the tour-field test is unavailable for the feedback hook, with the reason" |
| Composition stops handing the panel the label helper | `application_review_tour_writing`: "turning on the label helper in Settings turns the feedback guidance on" |

The new tests also fail to compile on the base (`writing_hook::feedback` did not exist).

### Gates

`coding-format-check` (clang-format 23.1.1) passes; `architecture_boundaries`
passes; round-trip pins resealed over `1e277adcc` (mutation pin unchanged).

### Limits

- Editing any hook advances the one profile hook revision, so a feedback edit
  makes a pending tour writing request stale (unchanged contract).
- macOS only; Linux and Windows runs are owed. No live GitHub, no model call.

### Owner look, session v001 (pending)

`BPR-1090:owner-look` is the owner's, not self-certified. One short attended
session asks one question, Accept or Reject: the Review feedback row reads and
looks right (below a separator, its built-in guidance and version, an On/Off
line that follows Reviews & queue › Comment labels › Label helper, Test
unavailable). Offscreen renders were checked first
(`BPR_SETTINGS_RENDER_DIR` with `qt_settings_window` writes
`writing-hooks-feedback-off.png`, `-on.png` and `writing-hooks-problem.png`).

- Session `df50c8413c8a1b3a29db57916d6b8941`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1090-feedback-row-v001`,
  repo root `/Users/jaeger/Documents/GitHub/BetterPullReview`, candidate
  `BPR-1090-feedback-row-v001` (revision `frozen-54b4b89eb-v001`, fingerprint
  `6b6b785c318b…`), ready, cursor 4. Host pid 2733, detached, on
  `http://127.0.0.1:62057/` (token in the coordinator's report).
- Frozen root `/Users/jaeger/bpr1090-feedback-hook-look-v001.VnZ20v`: the `git
  archive` of `54b4b89eb` built there (Debug, tests on); executable SHA-256
  `be9b40c5e70e…`. Offscreen, `--github-fixture conventional-comments-v1
  --attended-launch --attended-open-settings` printed `ATTENDED_SETTINGS_SHOWN`.
- Launch: `python3 /Users/jaeger/bpr1090-feedback-hook-look-v001.VnZ20v/launch.py`.
  It checks the executable digest and fixture tree first and reports an early
  exit within seconds. The fixture profile is private; the owner's own profile
  is not opened.

Next: read the session's decision. Accept satisfies `:owner-look` and the card
finishes; Reject comes back with what to change, for a v002 candidate.
