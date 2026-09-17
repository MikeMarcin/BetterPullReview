---
schema: "implement-rfc.card.v1"
id: "BPR-0871"
title: "dependency_modernization_contract reports a skip without naming the case or the reason"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux, in two successive full Linux sweeps","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0871: dependency_modernization_contract reports a skip without naming the case or the reason

## Defect

`dependency_modernization_contract` passes, and one of its twenty-six cases
silently does not run. Its whole output is:

```
....................s....
Ran 26 tests in 84.984s

OK (skipped=1)
```

The `s` and the count are the only evidence a case was skipped. Neither the
case nor the reason appears anywhere in the run, so a reader of a green sweep
cannot tell which guarantee was not checked — and a second case skipping later,
for a worse reason, would look exactly the same.

The skipped case is
`test_syntax_prefix_installs_one_library_per_configuration`, and its reason is
"fewer than two installed KSyntaxHighlighting configurations: not a
multi-configuration tree". That is legitimate on a single-config Ninja tree.
It was read out of the source by the Linux station, **not** out of the run,
which is exactly the gap.

The row is not cheap — 85 s on Linux, 23 s on macOS — so it is not a row people
re-run casually to find out what it did.

This is an observability gap, not a failure. It surfaced only because two
successive full sweeps recorded the row's output rather than just its verdict:
the earlier Linux sweep recorded "Pass, 64.48 s" and nothing more.

## Scope

- Make the run name what it skipped and why: raise the unittest runner's
  verbosity, or print each skip's case and reason after the run. Whichever is
  cheaper, the requirement is that the row's own output answers "which
  guarantee did this not check, and why".
- Check the other Python contract rows for the same shape. A row that can skip
  silently is worth finding before it does.
- Consider whether a skip should be visible in the sweep summary too, since
  the value here is for someone reading a whole sweep rather than one row.

Do not make the skip an error: single-config trees are the normal case and the
guarantee genuinely does not apply there.

## Acceptance

- The row's output names every skipped case and its reason, shown in a real
  run.
- A second skip appearing for a different reason is distinguishable from this
  one in that output.
- The row still passes on macOS, Linux and Windows, and still skips rather
  than fails on a single-configuration tree.
