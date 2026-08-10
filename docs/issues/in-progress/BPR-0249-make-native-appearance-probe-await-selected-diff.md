---
id: BPR-0249
title: Make the native appearance probe await the selected diff
status: In Progress
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
