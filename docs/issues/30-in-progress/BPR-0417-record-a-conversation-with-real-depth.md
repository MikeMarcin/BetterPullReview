---
id: BPR-0417
title: Record a conversation fixture with real reply depth
status: In Progress
rfc: RFC-BPR-0005
depends_on: [BPR-0416]
---

# Record a conversation fixture with real reply depth

[[BPR-0211]]'s own precondition, unmet since it was written: "a conversation deep
or wide enough to be a problem is recorded, so the question is asked against real
content rather than synthesised depth."

Measured during [[BPR-0413]] rather than assumed. No recorded pull request in
this repository carries a conversation at all — `directxmath-336` and
`uiforetw-74` both have top-level keys `commits, files, pull_request,
recorded_from, recorded_on, repository` and nothing else. The only conversation
content anywhere is three contract fixtures
(`tests/fixtures/github/contracts/v1/{github-com,ghes-3.18,uiforetw-74}/responses/review_threads.json`);
the first two carry three threads of one, two, and one comments with exactly one
`replyTo` edge, and the third carries none. **Maximum depth in this repository is
one reply.**

So [[BPR-0418]]'s map cannot be evidenced today. Every render would be of
content invented to make the view look necessary, which is the failure mode the
board already refuses elsewhere.

## What this card must produce

A fixture whose reply structure is deep and wide enough that a flat list is
genuinely hard to follow — the condition [[BPR-0211]] named. It must be honest
about its provenance:

- If recorded from a real public pull request, record it the way the existing
  recordings are, with `recorded_from` and `recorded_on`, and pick a pull request
  whose conversation is genuinely deep rather than one padded to look so.
- If constructed, say so in the fixture's README and never let a later card cite
  it as recorded evidence. A constructed fixture is legitimate for exercising the
  model; it is not evidence that the view solves a real problem.

Either way it exercises the cases [[BPR-0416]] answers: a chain several deep, a
first comment with a null `replyTo`, a `replyTo` naming an absent comment, and
siblings at the same depth.

## Verification

```sh
ctest --preset dev --output-on-failure -R 'github_conversation|github_fixture'
python3 scripts/verify_documentation.py --root .
```

## What deepening the shared fixture actually costs — 2026-08-22

Attempted and backed out, so the next session prices this from a measurement
rather than a guess. The obvious route — add a deep thread to
`tests/fixtures/github/contracts/v1/github-com/responses/review_threads.json`,
which is the fixture the application actually loads — was tried at a nine-comment
thread five replies deep. It reached green on two gates and broke eight.

| gate | why it broke |
|---|---|
| `github_contract_fixture_security` | requires the `github-com` and `ghes-3.18` copies to be **byte-identical**; the two hosts must return equivalent semantics |
| `github_read_contract` | `canonical_normalized_read()` in `tests/github_contract/github_read_contract_tests.cpp` is an independent C++ description of the same fixture, asserted equal to the normalized read |
| `application_github_public_path_github_com` | `REVIEW_LEDGER_FAILED` |
| `application_fixture_verdict_contract`, `application_diff_workbench_public_path`, `application_diff_workbench_shutdown_public_path`, `application_epoch_persistence_github-com`, `application_epoch_maintenance_public_path` | not individually diagnosed before the change was reverted |

The first two are exactly the gates that should fire, and both were satisfiable:
mirroring the file fixed the first, and generating the C++ block from the same
data that generated the JSON fixed the second. The ledger failure is the
informative one. `review_ledger_round_trip`
(`src/app/src/github_application.cpp:9880-9925`) does not read conversations at
all — it returns false until a diff is selected and the retained material
matches. So the extra content perturbed the **sequencing** of a fixed-length
public-path course rather than any conversation expectation, which is a much
larger surface than a data edit and is not what this card is for.

### Two routes, now priced

- **Deepen the shared contract fixture.** Truest to the product, because the
  depth then flows through the real public path. Costs: a byte-identical mirror,
  a regenerated C++ fact block, and reconciling every public-path course whose
  step budget or ordering the extra content disturbs. That last part is the
  unknown and it is the whole cost.
- **A separate deep-conversation fixture consumed only by the map.** Cheap and
  contained, and honest if it is labelled constructed — but it does not prove
  anything about the shipped public path.

This card should not choose between them alone. [[BPR-0418]] is the consumer,
and the choice belongs with whoever has that surface in hand: a fixture with no
consumer gets reshaped by its first one. What is settled is that the first route
is a slice of its own, not a data edit, and that the second is available
immediately if the map only needs content to render.

## Constructed fixture route claimed — 2026-08-24

The current macOS session claims the contained route after re-reading the
consumer and the repository's capture path. Existing recorded-pull-request
tooling stores only pull-request, file, and commit observations and generates
an empty review-thread response. A genuinely recorded deep conversation would
require authenticated GraphQL capture plus a new recording schema, generator,
contract tree, fixture security registration, public-fixture descriptor, and
public-path course. That is a cross-layer feature and credentialed acquisition,
not this fixture card.

The shared `github-com`/`ghes-3.18` contract fixtures remain unchanged. This
slice will add a versioned test-only projection fixture outside `contracts/v1`,
label it constructed in both machine-readable data and its README, and forbid
later claims that it is recorded product evidence. Its valid topology will
contain a null-parent root, a chain at least five replies deep, and siblings at
multiple depths. Missing-parent and cycle shapes will be separate hostile Qt
projection cases explicitly marked as impossible through the validated GitHub
read path; they prove row preservation, not provider acceptance.

The fixture will have an independent contract test for schema, provenance,
depth, siblings, root semantics, and hostile-case labeling. BPR-0418 may load
the exact projection identities in a rendered test without adding a production
loader or perturbing fixed public-path sequencing. This claim performs no
credential access, network request, public observation, GitHub interaction, or
remote mutation.

## Data and direct-contract checkpoint — 2026-08-24

The versioned constructed fixture now exists at
`tests/fixtures/github/conversations/deep-thread-v1/`. Its nine-node valid tree
has one root, a five-reply-edge primary chain, and sibling branches at reply
depths one and three. Separate non-provider-reachable projections contain the
one absent-parent and one two-comment-cycle shape used by Qt's defensive row
preservation. The README and machine-readable provenance both forbid a
recorded-GitHub or shipped-public-path claim.

The independent Python contract is 10/10 green. It locks canonical JSON, exact
schema/provenance, stable identity order, null-root semantics, depth, sibling
breadth, parent ordering, hostile reachability labels, the absent-parent and
cycle shapes, and valid/hostile separation. The code-simplification pass kept
those explicit checks and only normalized new Python class names to repository
conventions. Focused conversation, read-contract, fixture-security, network-
isolation, formatting, documentation, and hygiene gates also pass while the
test is registered in the working graph.

That registration is deliberately not part of this checkpoint commit. Adding
one configured default test changes the generated CTest plan that exact BPR-0256
packet `9ecf4427…ef7a` requires the owner to decide and then reinspect without
drift. Excluding the new test only when the live option is enabled would hide a
safety test to preserve a count and is not an acceptable repair. BPR-0417 stays
`30-in-progress/`: after the BPR-0256 decision is recorded and its attended
course either completes or is declined, restore the ordinary network-isolated
CTest registration, rerun the focused matrix, obtain independent review, and
close this card. No BPR-0256 packet byte or generated live-plan byte changed in
this checkpoint.
