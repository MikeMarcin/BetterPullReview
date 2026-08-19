---
id: BPR-0393
title: The workbench public path's marker regex outran BPR-0368's instrumentation
status: In Progress
rfc: RFC-BPR-0006
depends_on: []
---

# The workbench public path's marker regex outran BPR-0368's instrumentation

`application_diff_workbench_public_path` fails at `HEAD`, and has since
`9d63c4d`. Found while verifying an unrelated slice, which is the wrong way to
find a red test.

```
172 - application_diff_workbench_public_path (Failed)
Required regular expression not found.
```

`tests/end_to_end/CMakeLists.txt:557` asserts one contiguous marker block joined
by `[\r\n]+`, so every line the application prints between the first and last
marker has to be named in the pattern. BPR-0368 added two new lines to the
status dump — `SYNTAX_DELIVERY:<hits>:<coalesced>:<dropped>:<failures>:<terminals>`
and `SYNTAX_DISPLAYED:<left>:<right>` — between `SYNTAX_CLASSIFICATION:` and
`APPEARANCE_SETTINGS:`, and changed zero characters of that regex. A contiguous
matcher treats an inserted line as a mismatch, not as extra output.

The observed run prints `SYNTAX_DELIVERY:1:1:0:0:2` and `SYNTAX_DISPLAYED:0:0`.

## What this card is and is not

It is the regex repair, so the `dev` matrix stops carrying a failure that has
nothing to do with whatever is being changed.

It is **not** the syntax defect. `SYNTAX_DISPLAYED:0:0` on this course means
neither displayed pane carries classification, which is exactly what
[[BPR-0390]] exists to fix. This card must not tighten the marker to `1:1` and
call the defect closed: it would convert a red test into a red test for a
different reason and take BPR-0390's deterministic check away from it. The
pattern here matches the marker's *shape*; BPR-0390 owns making the value true
and then binding it.

## Required final state

- `application_diff_workbench_public_path` passes at `HEAD`.
- The regex names both new markers, so a third inserted line fails loudly rather
  than being absorbed.
- `SYNTAX_DISPLAYED` is matched by shape, with the reason recorded, and
  [[BPR-0390]] still owns tightening it.

## Verification

```sh
ctest --preset dev -R '^application_diff_workbench_public_path$' --output-on-failure
```
