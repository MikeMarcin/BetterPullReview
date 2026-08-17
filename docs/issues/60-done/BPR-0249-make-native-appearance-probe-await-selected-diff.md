---
id: BPR-0249
title: Make the native appearance probe await the selected diff
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0227]
---

# Make the native appearance probe await the selected diff

The digest-bound BPR-0114/BPR-0220 foreground run failed before its human
focus checkpoint. The native appearance course calls the fixture-only diff
request once, treats any not-yet-ready precondition as terminal failure, and
therefore emits misleading downstream Dark-to-Dark, zero-cause, and focus
markers without ever measuring appearance.

The failed run also showed why process exit is not evidence: its visible epoch
summary named the expected comparison while the one-shot helper still returned
false, and the log does not identify which authority, revision, blob, option,
or pending-request precondition refused it.

## Required final state

- Drive the native appearance course through the shipped BPR-0227 file-row
  selection and selected-diff composition instead of a direct fixture diff
  helper.
- Treat unavailable-but-progressing selected-diff state as pending within the
  existing bounded automated-run budget; never convert the first pending turn
  into terminal presentation failure.
- Bind the accepted material to the exact fixture authority, base/head OIDs,
  file identity, blobs, and diff options before the course can reach the human
  focus checkpoint.
- Emit a typed, causal precondition/refusal marker when the bounded course
  cannot reach that exact selected diff. Do not emit appearance verdicts for a
  course that never entered the appearance stage.
- Preserve the real active/frontmost requirement, Light-to-Dark baseline,
  exact geometry-attribution markers, shutdown, and the two exact `== 1U`
  expectations. This card does not decide BPR-0220's geometry result.
- Add a public real-executable contract plus a cut of the real selection wire
  so the fixture-only helper cannot satisfy the positive course.

## Focused verification

```sh
ctest --preset dev --output-on-failure \
  -R '^(application_native_appearance_probe_contract|application_workbench_composition_contract|application_uiforetw_selected_diff_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

## Non-goals

- Do not automate or weaken the frontmost-window requirement.
- Do not change the operating-system appearance without the existing owner
  authorization and operator procedure.
- Do not relax geometry attribution, accept Dark-to-Dark, or infer a BPR-0220
  verdict from an offscreen run.
- Do not regenerate the attended packet until the final executable bytes for
  the remaining portable work are settled.

## Implemented state

The native appearance course now selects `review.cpp` through the shipped Files
view and workbench intent wire. It waits for the real selected-diff publication
and binds the exact fixture authority, base/head revisions, file identity,
blob OIDs, default options digest, algorithm revision, and rendered semantic
selection before the owner-focus checkpoint. The fixture-only diff helper is
counted and must remain unused.

Pending startup remains pending within the run budget. Refusals name their
cause, and a course that never enters the appearance stage emits no appearance
verdict or render markers. A one-millisecond regression proves early budget
expiry reports `awaiting_workbench_observation`, derives helper use from the
real counter, shuts down normally, and produces no evidence files. The real
selection-wire cut refuses while the positive course reaches the exact bound
comparison.

The focused matrix passed 3/3: native appearance, workbench composition, and
UIforETW selected diff. Formatting, changed-file clang-tidy, documentation, and
`git diff --check` pass. Independent critical review reproduced one false
early-timeout cause, drove the one-millisecond correction, and returned CLEAR
with no remaining blocking or high-risk finding.

The frontmost Light-to-Dark run and BPR-0220 geometry verdict remain open. The
attended packet must be regenerated only after the remaining portable changes
settle, so its executable digest is final for that run.
