---
id: BPR-0381
title: Make a timed-out public-path wait name the condition that failed
status: Ready
rfc: RFC-BPR-0010
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
