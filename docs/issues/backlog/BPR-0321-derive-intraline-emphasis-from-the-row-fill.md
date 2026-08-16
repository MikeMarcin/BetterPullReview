---
id: BPR-0321
title: Derive intraline emphasis from the fill the row actually carries
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0320]
---

# Derive intraline emphasis from the fill the row actually carries

Reported by the product owner while accepting [[BPR-0320]]'s strength
selection:

> There's one weird thing going on though with the selected lines. If the line
> fill is blue the intraline emphasis fill should also be a blue, currently it
> puts red/green there which makes it more difficult. Same for yellow.

The report is accurate and the cause is one line.
`diff_workbench::append_intraline_emphasis` chooses its colour from the pane it
is painting and nothing else:

```
src/adapters/qt/src/diff_workbench.cpp:1548
    const auto color = aSide == core::diff_side::left
        ? Colors.IntralineRemoved
        : Colors.IntralineAdded;
```

`aSide` is the pane, not the row's state. So a row wearing the selection wash
or the moved band still gets the added-green or removed-red mark stamped into
it. The emphasis is answering "which pane am I in" when the reader is asking
"which characters changed in this row".

The result is worse than merely inconsistent. On a selected row the reader sees
three fills stacked — the row band, the blue wash over it, and a red or green
patch inside that — and the patch is the only one whose hue has nothing to do
with the other two. That is why it reads as noise rather than as emphasis.

## Required final state

- The emphasis fill is derived from the fill the row is actually wearing, so a
  blue row is emphasised in a deeper blue and a moved row in a deeper yellow.
- The added and removed cases keep exactly the fills [[BPR-0320]]'s attended
  selection accepted. This card changes which fill is chosen, not the accepted
  added/removed values.
- Derivation is a core decision with a published rule, not a Qt colour tweak.
  The core already owns the separation policy the derived fill has to satisfy;
  it should own the derivation too.
- Every derived fill meets `emphasis_separation_minimum_tenths` against the
  fill it is painted over, and the text on it meets the emphasis floor for its
  variant.
- The Qt painter reads the row's current fill rather than the pane.
- A test fails before the fix for a selected row and for a moved row.

## Non-goals

- Do not change the added or removed emphasis colours; they are digest-bound to
  batch `bpr:/attended-review/betterpullreview/ebe2ee306b89f453d1de6704`.
- Do not change the selection wash alpha or the row fills.
- Do not fold in [[BPR-0322]]'s colour-vision repair; that pair is a separate
  defect with a separate cause.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_appearance_contrast|qt_appearance_profile|qt_diff_workbench_scale_(100|125|200)|application_uiforetw_selected_diff_contract)$'
python3 scripts/verify_documentation.py --root .
```

A rendered capture of a selected row and a moved row carrying intraline
emphasis is required, and it needs an attended verdict before this card is
terminal. The captures that settled BPR-0320 do not show either state.
