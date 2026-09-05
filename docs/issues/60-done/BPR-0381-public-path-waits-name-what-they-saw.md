---
schema: "implement-rfc.card.v1"
id: "BPR-0381"
title: "Make a timed-out public-path wait name the condition that failed"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: []
---

# Make a timed-out public-path wait name the condition that failed

[[BPR-0344]] took a week of elapsed investigation, three hosts' worth of
filesystem comparison, and a temporary instrumented build to establish a fact
the failing process already had in hand: the queue had zero rows.

The assertion that failed is a five-part `wait_until` in
`tests/end_to_end/queue_composition_public_path.cpp:132-145`. On timeout it
prints one string:

```
queue-composition-failed:first-head-is-exact-and-quiet
```

That names the *question*, not the answer. Nothing distinguishes "no row
appeared" from "the row appeared with the wrong bucket", and those have
completely different causes. BPR-0344's card was written around the wrong one
for its whole life, because a `rowCount()` of zero and a wrong `Bucket` are
indistinguishable from the outside and the card guessed the interesting one.

One `std::cerr` line answered it in a single run.

## Not only this contract

`wait_until`/`fail` of this shape recurs across `tests/end_to_end/`. Any
condition built from a conjunction has the same problem: the timeout says which
predicate was being waited on and nothing about which conjunct was false or what
value it held instead.

This is the same theme as [[BPR-0348]] — a diagnosis discarded at the moment it
was available, and reconstructed later at much greater cost — but in the test
harness rather than the product.

## Required final state

- A timed-out wait in the end-to-end public paths reports the observed state of
  the condition it was waiting on, not only the condition's name. For the queue
  case that means at least the row count, and the row's ref, role, matched-rule
  flag and bucket when a row exists.
- The report is bounded and does not turn a 20-second poll into 4000 lines. It
  is emitted once, on failure, from the state at the moment of the timeout.
- The mechanism is shared rather than pasted per call site, so a new wait gets
  it by default rather than by remembering.
- Nothing secret or repository-content-bearing enters the output. These are
  fixture pull requests, but the rule should hold by construction rather than
  by the fixtures happening to be synthetic.
- A test proves a timed-out wait names the failing conjunct. A gate that only
  ran when something else was already broken would not have caught the gap this
  card exists to close.

## Verification

```sh
ctest --preset dev -R '^application_queue_composition_contract$' --output-on-failure
```


## One mechanism, and it names the conjunct — 2026-08-19

### What the diagnostic says now

Breaking exactly the conjunct BPR-0344 spent a week guessing about — the bucket —
and letting the wait time out:

```
queue-composition-failed:first-head-is-exact-and-quiet
failed=bucket-is-quiet rows=1 remove-enabled=true
ref=github.com/octo-reviewer/octo-org/review-repository#17 role=none
matched-rule=true rule-exact=true bucket=quiet
```

`failed=` names the conjunct. `rows=1` is the fact whose absence cost the week —
one row, not zero, so the row-count hypothesis the card was written around is
answered in the first line. And `bucket=quiet` gives the observed value beside the
term that rejected it. That was run, not described: the conjunct was seeded, the
contract re-run, the output captured, and the seed reverted.

### The mechanism

`tests/end_to_end/public_path_wait.hpp` holds `wait_terms`, and a predicate is
written as a recorded conjunction:

```cpp
return aTerms.require("remove-enabled", action->isEnabled())
    && aTerms.require("row-count-is-one", model->rowCount() == 1);
```

`&&` short-circuits, so the last term recorded is the first false one and
everything before it was true. A wait written this way reports its failing conjunct
**because of how it is written**, which is the card's "by default rather than by
remembering". On timeout the predicate is evaluated once more into the caller's
recorder, so the report describes the state that ran out of time rather than an
earlier poll; on success the recorder is untouched, so a caller cannot print a
report for a wait that worked.

`public_path_poll.hpp` holds the Qt polling loop and the three budgets, which are
unchanged — 20,000 ms for the queue course, 30,000 for the agent course, and the
longer 45,000 at the seven sites that always asked for it, now a named constant
rather than a repeated literal. Both duplicate `wait_until`/`fail` pairs are gone.

### Output safety, by construction

Booleans, counts, and fixed-vocabulary names go through `note`/`note_name`.
Identifier-shaped text goes through `note_identifier`, capped at 96 bytes with
every byte outside printable ASCII replaced and `=` excluded so a value can never
look like another term. There is deliberately **no** free-text channel, so
repository content has nowhere to go.

That mattered concretely. The `typed-success` dump ended with the agent panel's
entire output text and the previous shape reported `evidence.WorkspaceRoot`, a
filesystem path — BPR-0344's own evidence contains a real `/home/<user>/…`. Both
are replaced by bounded facts, including an `output-bytes` count in place of the
output itself. Whole-report bounds are 24 terms and 2 KiB, and truncation reports
`elided=<n>` rather than stopping silently.

### The site that printed nothing

`agent_invocation_composition_public_path.cpp`'s cleanup-retry shutdown wait
returned `EXIT_FAILURE` having printed no tag at all — strictly worse than naming
the question. It could not simply call `fail`, because `fail` begins and waits for a
shutdown that has already begun. So report emission is now separate from `fail`'s
shutdown-and-return, and that site reports `cleanup-retry-shutdown` with the
shutdown state it observed.

### The test that proves it

`application_public_path_wait` is a Qt-free executable — the report formatting is
standard C++ precisely so it can be compiled without an application or an event
loop. Six cases: the first false term is named and terms after it are not invented;
a satisfied predicate names nothing; counts, flags, enumerators and identifiers are
reported; the byte bound holds with visible truncation; a hostile identifier with
newlines, tabs, a control byte, an embedded `=`, 400 filler bytes and a non-ASCII
tail is bounded and sanitized; and a hostile term *name* is sanitized too.

That last case earned its place immediately: it failed on first run, because
`render()` printed the failing term from an unsanitized copy while `append` had
bounded the other one. A newline reached the front of the report. Fixed, and the
line explaining why is in the header.

### Evidence

| Check | Result |
|---|---|
| `application_public_path_wait` | passed, 6 cases |
| `application_queue_composition_contract` | passed |
| the five `application_agent_invocation_*` contracts | passed |
| bucket conjunct seeded, contract re-run | `failed=bucket-is-quiet rows=1 … bucket=quiet` |
| seed reverted | 8/8 passed |
| `clang-format --dry-run --Werror` on all five files | clean |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

### Every site, not just the one that hurt

All 28 waits across the two files now name their terms. The only bare polls left are
the two inside `shutdown()`, whose failure its callers report.

The conversions are mechanical but not uniform, and two shapes needed care:

- **Waits nested in an `||` chain.** `typed-terminal-cleanup` and `typed-success`
  each combine two or three conditions with `||`, so a single recorder cannot say
  which arm failed. Each arm gets its own recorder and the surviving dump folds in
  their failing terms as `reached=` and `cleaned=`, so the report distinguishes "the
  terminal never arrived" from "it arrived and the workspace was not cleaned".
- **`restoreThenFail`.** `cleanup-retry-first-refusal` returns through a restore
  helper rather than through `fail`, so it reports before restoring — otherwise the
  state that failed would be gone by the time anything printed it.

Where a value is more useful than a boolean it is noted alongside: profile and note
counts, cleanup and cleanup-failure counts, output byte counts, defect counts, and
the panel status as a bounded identifier.
