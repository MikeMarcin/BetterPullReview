---
schema: "implement-rfc.card.v1"
id: "BPR-0118"
title: "Apply the code line height, or retire the setting"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0116"]
---

# Apply the code line height, or retire the setting

`code_line_height()` returns the correct derived measurement and is unit
tested, but no surface consumes it, so the RFC's criterion that line height
"persist and preview immediately" is met only in the first half. BPR-0116
closed the tab-width half of that shortfall and established why this half could
not be closed with it.

## What was measured

`QPlainTextDocumentLayout` ignores line height in every mode Qt offers. A probe
on macOS with Qt 6.11.1 measured the first block's `blockBoundingRect().height()`
in a `QPlainTextEdit` over a three-line document:

| Applied to every block | Measured block height |
|---|---|
| nothing (baseline) | 19 |
| `ProportionalHeight` 200 | 19 |
| `FixedHeight` 40 | 19 |
| `MinimumHeight` 40 | 19 |
| `LineDistanceHeight` 40 | 19 |
| `topMargin` 7 and `bottomMargin` 5 | 19 |

Block margins are ignored as well, so the customary workaround is unavailable.
Nothing reachable from outside the layout changes the block height.

## Why this is a decision and not a task

Honouring the setting means replacing the code panes' text-rendering strategy.
The two candidates each cost something real:

- **`QTextEdit`** honours `ProportionalHeight`, but `QTextDocumentLayout` lays
  out the whole document rather than the visible blocks. RFC-BPR-0005's
  virtualization evidence rests on `QPlainTextEdit` and records 68 window
  widgets at 60, 6,000, and 60,000 lines. Adopting `QTextEdit` puts that
  bounded-widget claim and the large-file figures at risk to satisfy one
  typography setting, and would require re-establishing both.
- **A custom `QAbstractTextDocumentLayout`** keeps the widget but takes on
  painting, hit testing, selection, and scrolling geometry. That is a large
  surface to own correctly, and every part of it is load-bearing for the
  comparison surface's measured axis and ribbons.

A third option is legitimate and should be considered on its merits: **retire
the setting**. A code font's line spacing would then be whatever the font
supplies. That is honest, costs nothing, and removes a control the product may
not need — but it is a product decision about what BetterPullReview offers, not
an implementation shortcut.

Do not close this by making the setting appear to apply. The delivering slice
declined to fake it, and that judgement stands: a setting that looks applied and
is not defeats the next reader's attempt to find the bug.

## Required direct evidence

- Whichever route is chosen, a line-height change visibly alters code line
  spacing on the composed surface and the applied value matches
  `code_line_height()` — or the setting and `code_line_height()` are removed
  together, along with the RFC criterion that names it.
- If the rendering strategy changes, RFC-BPR-0005's bounded-widget and
  large-file figures are re-measured at 60, 6,000, and 60,000 lines and shown
  to still hold, or the claim is restated with the new numbers.
- `measured_geometry_change::line_spacing` becomes reachable from production
  code, or is removed with the setting.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## The decision

The product owner chose **retire** on 2026-08-06, presented against the two
re-platforming routes and their costs. Recorded here because the reasoning is
the durable part: a typography setting is not worth putting RFC-BPR-0005's
bounded-widget claim and its 60/6,000/60,000-line figures at risk, and it is
not worth owning painting, hit testing, selection, and scrolling geometry by
hand.

## Why deleting the accessor was not enough

The obvious reading of "retire the setting" is to delete
`appearance_profile::code_line_height()` and stop. That would have left
`code_font_settings::Font.LineHeightPercent` in place, because the code font
embeds the shared `font_settings`. The field would still be offered by the
type, still validated by `core::validate_typography`, still round-tripped by
whatever persists settings — and still impossible to apply.

That is precisely the state this card was written to forbid: a setting that
looks applied and is not. So the retirement is structural.

`LineHeightPercent` moved off `font_settings` onto a new `ui_font_settings`,
which is what `typography_settings::Ui` now holds. `font_settings` keeps
family, size, and weight, which both fonts share. **`code_font_settings` now
has no field a line height could occupy**, and neither does the record it
embeds.

## What was removed

- `core::font_settings::LineHeightPercent`, replaced by
  `core::ui_font_settings::LineHeightPercent`.
- `qt::appearance_profile::code_line_height()`.
- The line height carried by `qt::code_typography_event`. Its only subscriber,
  `diff_workbench::adopt_code_typography`, read the tab stop distance and
  nothing else, so the value was published to no one.
- `qt::measured_geometry_change::line_spacing`, which no production path ever
  raised. The remaining flags were renumbered rather than left with a hole; the
  enumeration is transient Qt-adapter state and is not persisted anywhere.
- The RFC's own code line-height goal and acceptance criterion, amended in
  place in the RFC's voice, with the measurement and the decision date. It is
  not deferred, and no later RFC inherits it.

`line_height_of` and `ui_line_height()` survive, now typed against
`ui_font_settings`. The UI line height was not retired and was not in scope.

## What this card does not claim

That the UI line height is applied. It is not, and neither is anything else in
`appearance_profile`, because BPR-0180 measured that the application composes
none of it. BPR-0184 owns the settings gesture and must decide whether the UI
line height reaches a surface or follows the code one out.

The difference is that the code line height was proven inapplicable. The UI
line height is merely unwired, which is a different problem with a different
answer.

## Evidence

- `ctest --preset dev` — **119/119**.
- `ctest --preset core-only` — **54/54**, configured and built fresh.
- Focused: `core_appearance_policy`, `core_appearance_contrast`,
  `application_appearance_host`, `qt_appearance_profile`,
  `application_native_appearance_probe_contract` — 5/5.
  `qt_diff_axis_scale_100/125/200` — 3/3.
- `core_dependency_boundary` and `core_boundary_contract` — 2/2 after the fix
  recorded below.
- `coding-format-check` — 275 files, passed.
- `verify_documentation.py` — `DOCUMENTATION_OK`. `git diff --check` clean.

The core-only run is the one that matters most for this slice's shape: the
retirement moved a field between core types, and `core-only` links neither Qt
nor SQLite, so it proves the new `ui_font_settings` split carries no
presentation dependency with it.

### Two breaks, each landing on its own assertion

**The structural rule.** Restoring `LineHeightPercent` to the shared
`font_settings` fails the compile on exactly the assertion that exists to catch
it:

```
tests/core/appearance_model_tests.cpp:306:3: error: static assertion failed due to
requirement '!carries_a_line_height<bpr::core::font_settings>': the shared font
record has no line height, so no font gains one by embedding it
```

That it is a compile failure rather than a test failure is the point. The rule
is about what the types can express, so the only way to violate it is to change
a type, and the only honest place to catch it is the compiler. The other two
assertions in that group would both still have passed — `ui_font_settings`
still carries one and `code_font_settings` still does not name one — which is
why the third exists.

**The runtime rule.** Removing the UI line-height check from
`validate_typography` builds with **0 errors** and then fails:

```
FAILED: exactly one value is rejected
1/1 Test #23: core_appearance_policy ...........***Failed    0.12 sec
```

Restoring it returns the test to `Passed`. The build succeeding is the part
worth recording: a break that fails to compile proves nothing about whether the
test was watching.

### The core boundary guard caught this slice

The first full run failed `core_dependency_boundary`:

```
Functional-core boundary violation in src/core/include/bpr/core/appearance_model.hpp:
(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*
```

The violation was a **doc comment**. Explaining why the code font has no line
height, it named the Qt layout class the measurement was taken against. No Qt
type was included, referenced, or depended on — the identifier appeared in
prose.

The fix was the comment, not the scan. Two reasons, and the second is the one
that matters:

1. The rule is defensible as written. The core's documentation should not
   explain itself in terms of a presentation framework, because a reader of the
   core should not need to know which one is on the other side of the boundary.
   The comment now states the rule and says the measurement belongs to the
   adapter that took it.
2. Adding a comment-stripping exclusion would have been a change to a guard
   made solely to admit the change under test. That is never an acceptable
   shortcut here, and it is particularly bad for a text scan: the value of
   scanning text rather than parsing it is that it cannot be talked out of a
   match, and every exclusion is a hole with no test behind it.

Worth noting that this is the same regex an earlier slice had to *anchor*,
after `Q[A-Z]` unanchored matched `QUEUE` inside an include guard. It has now
produced one false positive and one true positive. This was the true positive.

### Four timeouts, and why they are not this slice

The same run reported four timeouts: `epoch_maintenance_integration`,
`application_composition_contract`, `application_diff_workbench_public_path`,
and `application_epoch_persistence_github-com`.

The suite took **4,537 s** against a normal ~700 s — a 6.5x slowdown, on a host
reporting load averages of 3.61/4.37/4.68 with seven logged-in users. The
suite is serial, so competing load stretches every test against a fixed
timeout.

That explanation is plausible, and plausible is not evidence, so the four were
re-run serially:

```
epoch_maintenance_integration ..............   Passed   25.55 sec
application_composition_contract ...........   Passed   56.94 sec
application_diff_workbench_public_path .....   Passed   12.19 sec
application_epoch_persistence_github-com ...   Passed   44.80 sec
Total Test time (real) = 139.50 sec
```

All four are far inside their budgets — `application_composition_contract`
takes 56.94 s against a 600 s timeout. No timeout was adjusted, and none should
be: the budgets are not the problem, and widening them to absorb host load
would hide the next real hang. This is the second time this host has produced
exactly this pattern; `application_composition_contract` timed out the same way
during BPR-0163 and then passed standalone in 60.34 s.

There is also no mechanism by which retiring a typography setting could reach
`git_cache_contract` or `epoch_maintenance_integration`, which is worth stating
but is the weakest of the three arguments and is not what settles it. The
standalone run is.
