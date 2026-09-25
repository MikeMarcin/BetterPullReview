---
schema: "implement-rfc.card.v1"
id: "BPR-0904"
title: "The mutation fixtures' transport ignores method and path, so no row can fail a wrong request URL"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-007 scout, surveying BPR-0896","found_at":"424a64cc","run":"run-20260916-stations","dispatch":"dsp-007","milestone_plan":{"id":"bpr-0904-request-shapes","path":"docs/rfcs/plans/bpr-0904-request-shapes/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 Wrong request path fails a row, then restoration passes","outcome":"pass","revision":"9df1255df0a9733891aca44fdb0207da95fb28fa","evidence":{"kind":"command","detail":"Temporarily compiling GET /users made both observe_then_immediate_revalidate_then_submit and dispositions_and_old_reviews exit 1 at unscripted request #2. Compiler restored byte for byte; final focused CTest 5/5 pass."}},{"criterion":"AC2 Every review_mutation_operation has an independent exercised request-shape pin","outcome":"pass","revision":"9df1255df0a9733891aca44fdb0207da95fb28fa","evidence":{"kind":"command","detail":"All 11 enum values: exact methods and GitHub.com/GHES paths, plus kind/name for five GraphQL operations. Exactly one compiled fixture per pin/server. Appended unpinned probe enum fails the contract TU at expected_request_shape with -Werror,-Wswitch; restored."}},{"criterion":"AC3 Measured row impact is recorded","outcome":"pass","revision":"9df1255df0a9733891aca44fdb0207da95fb28fa","evidence":{"kind":"command","detail":"Historical keyed conversion: 0/20 existing in-process rows and both restart courses broken. Current 24 rows (21 mutation, 3 submission) pass individually using canonical private TMPDIR; 0 request-shape failures, no fixture answers changed. Baseline and final focused CTest 5/5 pass. Initial standalone submission root precondition failures recorded."}},{"criterion":"AC4 Egress validator guarantee is accurate","outcome":"pass","revision":"9df1255df0a9733891aca44fdb0207da95fb28fa","evidence":{"kind":"command","detail":"Header, round-trip contract, current card diagnosis and transport comments state canonical consistency and compiler/provenance limitations. No product behavior changed; submission negative control reaches unscripted refusal after canonical comparison passes."}},{"criterion":"Documentation and style","outcome":"pass","revision":"9df1255df0a9733891aca44fdb0207da95fb28fa","evidence":{"kind":"command","detail":"DOCUMENTATION_OK and 64 documentation regression tests pass with installed MASK helper PYTHONPATH. Format and git diff --check pass; focused clang-tidy exits 0, five displayed pre-existing header warnings, no changed-code findings."}}]
---

# BPR-0904: The mutation fixtures' transport ignores method and path, so no row can fail a wrong request URL

## Defect

This card exists because of how [[BPR-0896]] was found: by reading, during
other work, and not by any test. It could not have been found by a test.

`fake_transport` in the review-mutation suites answers from a scripted queue of
completions and **ignores the request's method and path entirely**. A fixture
therefore cannot fail because the product asked the wrong server for the wrong
thing; it can only fail on what it does with the answer it is handed. The
scripted answer arrives whatever was asked.

The consequence is measurable rather than theoretical. Across the whole
mutation surface, exactly **two** assertions pin a request path:

- `tests/github_contract/github_mutation_contract_tests.cpp:635` — the review
  submission list GET,
  `/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100`;
- `tests/github_contract/live_review_mutation_provider_tests.cpp:568` — the
  REST root-comment reconciliation, asserted only as
  `ends_with("/comments?per_page=100")`, inside
  `test_observe_then_immediate_revalidate_then_submit`.

[[BPR-0879]]'s thread observation additionally pins its GraphQL body and both
accepted roots at `github_mutation_contract_tests.cpp:437-459`, which is the
shape the rest of the surface should have and does not.

Everything else — every reply, every resolve and unresolve, every
reconciliation derived from a mutation's own path — is unpinned. [[BPR-0896]]
is what that costs: a request that no real server can answer, on the operation
a reviewer uses most, sitting in a suite that was green throughout.

There is a second limitation. `validate_outbound_review_mutation_request`
checks **canonical consistency by recompiling and comparing**. It can reject
wire fields that disagree with the supplied semantic request and compiler;
it cannot independently detect a defect shared with that compiler. When the
compiler constructs the wrong endpoint, the same wrong endpoint passes this
check. It also does not prove that the supplied semantic request was unchanged
or authorized. The original spike's broader statement that this check cannot
fail was overstated; the compiler-defect blind spot is the defect here.

## Scope

- Decide what the fixture transport should do about a request it was not
  scripted for. The two candidates are opposite in spirit: make the queue
  keyed by method and path so an unexpected request has no answer and the row
  fails, or keep the queue positional and require rows to assert `Wire.Method`
  and `Wire.Path` themselves. The first makes every existing row a path pin
  for free and will surface unknown breakage on the first run; the second is
  smaller and pins only what someone remembers to pin. Measure how many rows
  the first breaks before choosing it — that number is the real cost, and it
  is also the most interesting thing this card can produce.
- Whichever is chosen, every mutation operation ends with its request shape
  pinned: method, path, and for a GraphQL root the operation name in the body.
  Enumerate the operations from `review_mutation_operation`
  (`github_mutation_contract.hpp:29-44`) rather than from a list in prose, so
  a new operation added later is visibly unpinned.
- Separately, decide whether `validate_outbound_review_mutation_request` can
  be made to validate against something other than its own recompilation. If
  it cannot, say so on the card and stop calling it a validation of the
  request — its real guarantee is that the request was not mutated after
  compilation, which is a narrower and still useful claim, but it is not the
  one its name makes.

## Acceptance

- A wrong request path fails at least one row. Demonstrate it by making the
  [[BPR-0896]] defect (or an equivalent deliberate breakage) red, then green.
- Every operation in `review_mutation_operation` has its request shape pinned,
  or the card names the ones that do not and why.
- The number of rows affected by the chosen approach is recorded, not
  estimated.
- The egress validator's real guarantee is stated accurately wherever it is
  described.

## Spike checkpoint, dsp-009 (bpr-linux/int-17): the first candidate, measured, not chosen

The first candidate is built as a spike on the `linux-lane` checkpoint commit
that carries this note, so the number below can be re-derived from the diff
that produced it. Nothing is chosen here, and [[BPR-0896]] is not fixed.

**What the spike is.** `fake_transport` in
`tests/github_contract/live_review_mutation_provider_tests.cpp` now answers by
the request's shape: `scripted_request { Method, Path, Operation }`, where
`Operation` is the GraphQL operation name parsed from a `{"query":"…` body
and empty for REST. Answers live in a per-key FIFO, so a course still answers
the same read twice with different snapshots. A request with no scripted
answer fails the row at that request, printing
`FAILED: unscripted request #N: <METHOD> <PATH> [<Operation>] (operation
<enum>, body …)` and the answers the row did script. The binary gained
`--list` and `--only <row>` because a row fails by exiting, and a count of
failing rows needs each row alone.

**Conversion rule, so the measurement is the fixture author's intent and not
this spike's.** Every `Completions.push_back(helper())` became
`answer(<key the helper's name denotes>, helper())`: `viewer_response` →
`GET /user`, `target_response` → `GET /repos/MikeMarcin/BPR-Test/pulls/1`,
`comment_list_response` → `GET …/pulls/1/comments?per_page=100`,
`comment_response` → `POST …/pulls/1/comments`, `thread_observation_response`
→ `POST /graphql ObserveThread`, `thread_resolution_response(r)` →
`POST /graphql ResolveThread`/`UnresolveThread`. Ten raw inline completions
carried no helper name and were keyed by the request their position and the
row's own comment name; those ten are the only places the spike's judgment
enters, and each is a `github_mutation_wire_error`/`_cancelled` at the POST
or the first read of an `observe_target`, or the inline wrong-login viewer
answer.

**Acceptance answer: yes.** The [[BPR-0896]] defect goes red under the keyed
transport. No existing reply row reached `reconcile` (the only one stops at
the [[BPR-0879]] pre-flight refusal), so the spike adds
`test_a_replys_baseline_reconciliation_is_a_request_a_server_can_answer`: a
reply whose `ObserveThread` answer binds the designated pull request, with the
baseline reconciliation scripted only as `POST /graphql ObserveThreadComments`
(the shape BPR-0896 names; the envelope body is a placeholder that row will
own). The product's second request was `GET /graphql?per_page=100`, operation
`observe_review_comments`, empty body; the transport had no answer for it and
the row failed there with exit status 1. ctest row `live_review_mutation_provider`
(#239) failed on exactly that line, ctest exit 8; ctest row
`live_review_mutation_restart_recovery` (#240) passed under the same transport.
The red came from the transport's unscripted-request exit, not from the row's
own assertion, which sits after it.

**Cost measured so far: 0 existing rows broken in that suite.** Each of the
17 pre-existing rows run alone with `--only` exits 0 under the keyed transport
(16 in-process rows plus the four restart-recovery phases through row #240).
Every positional answer in this suite was already handed to the request the
product made, in order. The number is not yet the whole mutation surface:
`live_review_submission_provider_tests.cpp` has its own positional transport
(`perform_review_mutation` near line 168) that this checkpoint does not key,
and `github_mutation_contract_tests.cpp` has no transport at all (it pins
compiled requests directly, which is where the two path pins named above
live). Keying the submission suite is the next bounded step before the number
is final; the second and third scope points, and the egress-validator
statement, remain open.

## Spike checkpoint, dsp-009 (bpr-linux/int-18): the submission suite keyed, and the number final for every fixture transport

Built on the `linux-lane` checkpoint commit that carries this note, on top of
int-17's. Still nothing chosen; [[BPR-0896]] still not fixed.

**What was keyed.** `transport::perform_review_mutation` in
`tests/github_contract/live_review_submission_provider_tests.cpp` no longer
switches on `Wire.Operation`. That switch was never positional: it answered by
the operation the product *said* it was making, which is the product
describing its own request, the same self-reference this card names in the
egress validator. It now dispatches on `scripted_key(Wire)`, the int-17 rule
(method, path, and for a GraphQL root the operation name parsed from the
body), against four keys written as GitHub's REST paths for the fixture
target rather than copied from the compiler: `GET /user`,
`GET /repos/MikeMarcin/BPR-Test/pulls/1`,
`GET /repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100` and
`POST /repos/MikeMarcin/BPR-Test/pulls/1/reviews`
(`live_review_submission_provider_tests.cpp:239-254`, dispatch at `:273`,
unscripted exit at `:329-345`). A request matching none prints
`FAILED: unscripted request #N: <METHOD> <PATH> [<Operation>] (operation
<enum>, body …)` followed by the four requests the transport does answer, and
exits nonzero. The binary gained `--list` and `--only <row>` (`:719-743`).

**Where this deliberately departs from int-17's shape, and why.** int-17
replaced a scripted queue with a map of per-key deques. This suite's transport
is not a queue; it is a stateful simulated server: the `Reviews` list grows
on POST, `NewCount`, `Incomplete`, `WrongActor`, `WrongHead`, `MovedContext`
and `WrongRepository` shape each answer, and the restart-recovery kill
checkpoints fire from inside the POST handler. Pre-scripting those as deques
would rewrite every row's answers and put this spike's judgment into all of
them. Keying the dispatch and leaving the four handlers byte-for-byte as they
were keeps every row's scripting as its author wrote it, which is what makes
the count below the fixture author's cost and not the spike's. The
"scripted answers remaining" list became "this transport answers", because a
stateful server has no finite remaining set.

**Cost measured: 0 of 3 existing rows broken.** Rows resolved against
`ctest --preset dev -N` (exit 0): `live_review_submission_provider` (#241) and
`live_review_submission_restart_recovery` (#242). In-process rows by name,
each run alone with `--only`: `dispositions_and_old_reviews` exit 0,
`uncertainty_and_cancellation` exit 0, `historical_recovery` exit 0. ctest
#241 exit 0; #242 exit 0, which drives every kill course (prepared,
witnessed, published, resolved, published-moved) and the write/recover
courses (unresolved, moved, prepared) through the same keyed transport.
Configure exit 0, build exit 0 with no warnings, on the stock `dev` preset.

**Per-row evidence.** Under the keyed transport a row can only pass if every
request its product path made matched one of the four keys exactly, so a
passing row is its own evidence; the rows' existing trace assertions say
which requests those were. `dispositions_and_old_reviews` asserts the full
five-request sequence `observe_reviews, observe_viewer, observe_pull_request,
create_review, observe_reviews` (`:421`), which exercises all four keys.
`uncertainty_and_cancellation` asserts `Posts == 1` and a trailing
`observe_reviews` on every possible-send path (`:469`) and `Posts == 0` on
every pre-send refusal (`:504`), including the `Incomplete` course whose
`Link: next` header is refused rather than paged, so no second-page request
exists to go unscripted. `historical_recovery` asserts an empty trace on
discovery (`:534`, `:563-567`) and `Posts == 0` on reconcile. No row is
classified as a fixture that never declared its path, and none as a product
asking for something unintended; there is no row to classify and none is
unclassified.

**The zero is a pinned zero, not a silent one.** A throwaway probe, never
committed: with the viewer key changed to `GET /users` and the suite rebuilt,
`--only dispositions_and_old_reviews` printed
`FAILED: unscripted request #2: GET /user (operation observe_viewer, body
empty)` and exited 1; restored and rebuilt, the same row exited 0. The
diff on the lane is the restored one.

**Finalised number for the whole mutation surface: 0 existing rows broken.**
That covers the two suites that own a fixture transport, both now keyed by
the same rule: `live_review_mutation_provider_tests.cpp` (17 pre-existing
in-process rows plus the int-17 [[BPR-0896]] row; at this commit each of the
17 exits 0 alone with `--only`, the [[BPR-0896]] row exits 1 at
`GET /graphql?per_page=100`, ctest #239 exits 8 on that row and #240 exits 0)
and `live_review_submission_provider_tests.cpp` (3 in-process rows, each exit
0 alone; ctest #241 and #242 exit 0). Twenty pre-existing in-process rows and
both restart-recovery rows, none broken. The only red on the surface is the
row written to show [[BPR-0896]]. The number does **not** cover
`github_mutation_contract_tests.cpp`, which has no transport and pins
compiled requests directly (the two path pins named at the top of this card
live there), nor `gh_cli_transport_tests.cpp`, which is not a review-mutation
fixture transport and was not keyed, nor anything outside
`tests/github_contract/`.

**What this slice contributes to the second scope point, without closing
it.** The four operations this suite exercises now have their request shape
pinned by a row that demonstrably reaches them: `observe_viewer`,
`observe_pull_request`, `observe_reviews` and `create_review` (the `:421`
sequence). The mutation suite's keys pin the request shape of every answer a
row scripts, but which of those answers a passing row actually consumed is
not observed by either transport yet; that is exactly the design point below,
and the per-operation enumeration stays open until it exists.

**Design point for the eventual fix, recorded here and explicitly not a
defect.** A keyed transport should report, at the end of a row, any scripted
answer the product never asked for. A fixture answer nobody consumes is a
fixture that no longer describes the product: the row still passes, but one
of its pins is on a request that does not exist. In the mutation suite's
shape this is literal (a non-empty deque at row end); in this suite's
stateful shape it would be a key the row prepared for and the product never
asked. Not implemented in either spike; the third scope point and the
egress-validator statement also remain open.

## [[BPR-0896]] landed, 2026-09-23: the spike's red row is green

[[BPR-0896]] landed at `d8da166e`, so the statement above that the only red
on the surface is the row written to show it no longer holds. The probe row
`a_replys_baseline_reconciliation_is_a_request_a_server_can_answer` passes,
as does the whole suite and `live_review_mutation_restart_recovery`.

The red-then-green demonstration this card's first acceptance point asks for
was run as a negative control there. With the REST-derived reconciliation
temporarily restored, both ctest rows failed at
`FAILED: unscripted request #2: GET /graphql?per_page=100 (operation
observe_review_comments, body empty)`. Restored byte for byte, they passed.

Three changes touch this card's surface:

- The fixture transport now witnesses `reply_review_thread` as it witnesses
  `create_review_comment`.
- `review_mutation_operation` gained `observe_review_thread_comments`, and
  both suites' `operation_name` switches name it.
- The new reply rows pin their whole wire sequence (count, order, method,
  path, operation label, GraphQL document kind and name) independently of
  how this transport is keyed.

Nothing in this card's own scope was chosen or closed by that work.

## Execution record

### Chosen and verified, 2026-09-25 — assignment-r1

The independent BPR-0904 assignment from the coordinator adopts the bounded
[request-shape plan](../../rfcs/plans/bpr-0904-request-shapes/milestone-plan.json)
on branch `codex/bpr-0904-request-shapes`, based on
`9a0b44d56ab645de10afffd7e69a4ca0a3fe8891`. Implementation candidate:
`9df1255df0a9733891aca44fdb0207da95fb28fa`.

**Decision: retain the keyed transports already landed by dsp-009.** The
mutation suite keeps a FIFO per method/path/GraphQL-name key; the submission
suite keeps its stateful server handlers behind the same independent keys.
No transport behavior changed in this slice. The historical conversion cost
remains exactly 0 of 20 pre-existing in-process rows and both restart courses.
The current surface is 24 rows: 21 mutation and 3 submission, all passing
individually with `--only` under a private canonical `TMPDIR`. The five focused
CTest courses pass both before this slice and on the final candidate, including
both restart drivers. Current request-shape failures: 0 of 24 rows; no row's
scripted answers needed repair. The first direct submission runs failed at
`submission store opens` under macOS's symlinked default temporary path; using
the canonical private root supplied the same precondition as CTest, and all
three passed. This was a harness precondition, not a request-shape failure.

**Every operation is independently pinned and exercised.**
`expected_request_shape` in `github_mutation_contract_tests.cpp` exhausts the
actual `review_mutation_operation` enum without a default. For both server
contracts, the exchange course compiles and checks each of its 11 operations
against independent literal method/path expectations; the five GraphQL
operations additionally pin the document kind and exact operation name. The
course scans the enum's entire uint8 range and requires exactly one compiled
fixture for every pin. A new enum value without a switch case is a build error;
a new pin without a compiled fixture fails the course. No operation is omitted.

The earlier unused-answer suggestion is deliberately not implemented. Refusal
courses may prepare answers beyond the phase they are meant to stop at, and
the submission transport is a stateful server with four available handlers.
Only consumed answers demonstrate request shape. Exhaustive compiled-fixture
coverage supplies the missing per-operation proof without making unused
answers a new fixture error. Both transport comments now state that limit.

**Negative controls.** Temporarily changing the compiler's viewer path from
`/user` to `/users` made `observe_then_immediate_revalidate_then_submit` and
`dispositions_and_old_reviews` exit 1 at
`FAILED: unscripted request #2: GET /users (operation observe_viewer, body empty)`.
The submission transport's preceding canonical-consistency assertion still
passed, directly demonstrating that limitation. The compiler source was
restored byte for byte and all three targets rebuilt; the five focused courses
then passed. Separately, appending `bpr0904_unpinned_probe` to the enum and
syntax-checking the contract test failed at `expected_request_shape` with
`-Werror,-Wswitch`. The enum was restored byte for byte. The first expanded
GHES matrix failed because its whole-review fixture still named the GitHub.com
server; the final helper now binds every fixture to the selected server.

**Egress guarantee.** Keep the existing function and behavior as a canonical
consistency check. A second hand-maintained product compiler would duplicate
the same assumptions without an independent server oracle. The header and
[round-trip contract](../../architecture/review-round-trip.md) now explicitly
state that the check detects disagreement with compilation, not compiler
correctness, original semantic provenance or authorization. Independent wire
expectations and fixture refusal supply the request-shape evidence. No live
GitHub request, native transport qualification or new human acceptance is
claimed.

The [checkpoint](../../verification/BPR-0904.md) records commands, exact
candidate, remaining ownership and integration handoff.
