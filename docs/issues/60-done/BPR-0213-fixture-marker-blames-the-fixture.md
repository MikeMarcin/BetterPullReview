---
schema: "implement-rfc.card.v1"
id: "BPR-0213"
title: "GITHUB_FIXTURE_FAILED is printed when the fixture is fine"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: []
---

# GITHUB_FIXTURE_FAILED is printed when the fixture is fine

`main.cpp` prints `GITHUB_FIXTURE_OK` or `GITHUB_FIXTURE_FAILED` from `passed`,
which by that point has been narrowed by presentation, shutdown, epoch delivery,
native-vault and CLI facts as well as the fixture's own. A run whose presentation
failed and whose fixture was perfect therefore prints
`GITHUB_FIXTURE_FAILED:github.com`.

That is not a cosmetic naming quibble. It cost [[BPR-0154]] three recorded
occurrences and two wrong hypotheses — a stalled `open` stage, then a false
fixture presentation fact — before anyone decoded the evidence bits and found the
fixture had been working the whole time.

## The information already exists

`PUBLIC_PATH_GROUPS:core=… fixture=… maintenance=… render=…` names which
narrowing group rejected the run, and `PUBLIC_PATH_VERDICT` names the failing
core fact. Both were in the log the whole time. The problem is that the
*loudest* line is the misleading one, and a reader who trusts the marker names
never reaches the groups line.

Which is exactly the trap the groups line was added to close: its own comment
says reporting only the first group *"meant a failure in any later one printed a
verdict in which everything looked fine, which is exactly what made BPR-0133
unattributable."* That fix was right and incomplete — it added the truthful line
without disarming the untruthful one.

## What the fix must do

Say what failed. A marker naming the fixture should reflect the fixture's own
facts, and the run-level verdict should be named for the run.

Do not solve it by deleting `GITHUB_FIXTURE_FAILED`: several CTest
`PASS_REGULAR_EXPRESSION` gates match `GITHUB_FIXTURE_OK`, and a marker vanishing
would turn those into silent passes-by-absence. Whatever replaces it has to be
matched by those gates deliberately.

## Fixed, 2026-08-08

`fixtureFactsPassed` is computed from the fixture's own facts alone, and the
marker is printed from that. `passed` still narrows by it, so the run-level
verdict is unchanged — only the attribution moved.

**No assertion was lost.** Every CTest gate matching `GITHUB_FIXTURE_OK` already
requires `PUBLIC_PATH_OK` beside it in the same regex, so the run verdict was
never resting on the fixture marker. Naming it honestly cost nothing, which is
why the six gates needed no edit.

### The test needed two verdicts that could disagree

`application_fixture_verdict_contract` gets that from a render capture aimed at
an unwritable path: the fixture does all its work and passes, and
`passed = passed && renderSaved` fails the run. That produces, offscreen and
deterministically:

```
PUBLIC_PATH_FAILED
GITHUB_FIXTURE_OK:github.com
RENDER_FAILED
```

which is exactly the shape that used to be impossible to print. Restoring the
old behaviour — the marker printed from `passed` — fails two of its assertions
with a clean build.

The contract also checks the ordinary case still reports both, because a marker
that had become unconditionally true would be caught by the existing gates only
while it is still printed at all.

### One thing the test got wrong first, recorded because it is easy to repeat

The flag takes `github-com` and the marker prints the fixture's own
`github.com`. An earlier draft used one constant for both and failed on a
mismatch that had nothing to do with the behaviour under test.

## Superseded — required direct evidence

- A run whose fixture facts are all correct and whose presentation fails does
  **not** print a marker blaming the fixture.
- A run whose fixture genuinely fails does print one, so the marker keeps its
  teeth.
- Every CTest gate matching the old marker is updated, and each is shown to fail
  when its condition is neutered rather than passing because a pattern stopped
  matching anything.
