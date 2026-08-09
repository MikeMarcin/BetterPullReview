---
id: BPR-0187
title: Record a real public pull request as a replayable fixture
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0186]
---

# Record a real public pull request as a replayable fixture

The existing public-path fixture is 1,143 lines of hand-written payloads. It is
correct for what it does — driving the automated run deterministically — but it
is weak ground for a *visual acceptance* judgement, because the content was
written to satisfy the code that reads it.

The product owner designated `microsoft/DirectXMath#336` for this. It reads
unauthenticated, which was confirmed before this card was written:

```
curl -sS -H "Accept: application/vnd.github+json" \
  https://api.github.com/repos/microsoft/DirectXMath/pulls/336
```

returns a public, open, 2-file, +221/-0, 2-commit pull request. No credential
is involved, and no request this card makes is a mutation.

## Why a recording rather than a live read

A test that reaches the network is a test that fails when the network does, and
a fixture that changes when somebody pushes to a branch is not a fixture. The
recording is fetched once, committed, and replayed. The safety rules already
name recordings as an accepted surface for exactly this.

## What must not be recorded

Only public content, and only what the review surface needs. Do not record
rate-limit headers, tokens, cookies, or any `Authorization` header — there is
no credential in an unauthenticated read, and a recording that has a place to
put one invites a later fetch that fills it in.

Attribute the source in the fixture: repository, number, head OID, and the
retrieval date. A committed blob of somebody else's code with no provenance is
the kind of thing that is impossible to audit later.

## Recorded, 2026-08-08

`tests/fixtures/github/pull-requests/directxmath-336/recording.json`, with its
provenance in the README beside it. Three unauthenticated `GET`s — the pull
request, its files, its commits — trimmed to what a review surface reads.

| | |
|---|---|
| head | `c5d86c3dfc28fc661cc03a151a3643f41da7a12d` |
| base | `0332efa39ab80fd1417d5ba120abbde6373e15ed` |
| shape | 2 files, +221 −0, 2 commits, open |
| patch | 233 lines across a header and a 220-line `.inl` addition |

Everything else the API returns was left out — avatars, node identifiers,
reaction counts, and the two dozen URL fields on every object. The recording
contains exactly **one** URL, the provenance line, and that is asserted rather
than intended. There is no `Authorization` header because an unauthenticated
read has none, and no place to put one.

`github_recorded_fixture_contract` reads the file rather than trusting the
intent behind it: head and base OIDs, the recorded shape, a patch on every
file, no credential-shaped string, and exactly one URL. Each shown non-vacuous
by drifting the head, emptying a patch, inserting a bearer token, and adding a
second URL.

## The recording is weaker presentation evidence than this card assumed

Measured from the recording itself, 2026-08-08:

| file | removed | added |
|---|---|---|
| `Inc/DirectXMath.h` | 0 | 1 |
| `Inc/DirectXMathMatrix.inl` | 0 | 220 |

**`microsoft/DirectXMath#336` is a pure addition.** No line is removed anywhere
in it, which means:

- no removed-line band is ever painted;
- no replacement pairs a left line against a right one;
- **no intraline edit exists**, because an intraline edit is a difference
  *within* a line that appears on both sides;
- no moved code, for the same reason.

So of the diff surface's painted vocabulary, this fixture exercises the added
band and nothing else. It is real code, which is what this card wanted, and it is
narrow evidence for judging a *diff* viewer, which is what this card wanted it
for.

That does not make the recording useless. Real 220-line C++ with real line
lengths and real formatting is still better ground for judging syntax colouring,
line height, and how a long addition reads than hand-written payloads are. It
just cannot answer the questions that need two sides.

**It also does not unblock [[BPR-0214]]**, which is the case that card explicitly
asked to be checked either way rather than assumed. Serving this recording would
leave intraline emphasis exactly as unobserved as it is now.

### What this implies for the remaining half

Wiring 1,143 lines of fixture to serve content that paints one band out of five
is a poor trade if the goal is presentation evidence. Two honest options, and the
choice is the product owner's since they designated this pull request:

1. **Serve it anyway** for what it does cover, and record a second recording with
   a replacement for what it does not.
2. **Record a second pull request first** — one with removals and edits within
   lines — and serve whichever better repays the wiring.

Either way the finding is that one recording cannot be the presentation fixture,
because a pull request that only adds cannot show what a diff does when code
changes.

## Half done, deliberately

**Nothing reads the recording yet.** Serving it through
`github_public_path_fixture` — 1,143 lines that synthesise payloads and a git
pack — is the other half, and it is a different kind of work: the fetch needed
a network and the wiring does not. Splitting them means a network outage cannot
block the second, and means the recording exists to be wired rather than being
blocked behind wiring that has not been designed.

The head OID assertion should survive that work. A re-fetch that drifted must
be a failure and not a silent change to content a presentation judgement was
made against.

## Required direct evidence

- The recording replays without a network, proven by running the suite with no
  route to `api.github.com` rather than by trusting that nothing calls out.
- The recorded head OID is the one the fixture serves, so a re-fetch that
  drifted is a failure rather than a silent content change.
- The workbench shows the recorded review, and the diff content in the panes is
  the diff the recording carries.
- The recording contains no `Authorization`, `Cookie`, or `Set-Cookie` header,
  asserted by reading the recording rather than by having intended not to write
  one.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Measured, 2026-08-08 — serving a recording is not a JSON problem

Two facts found while trying to serve `uiforetw-74`, and they change this
card's shape.

**Nothing reads the pull-request recordings.** `grep -rn "recording.json" src/`
finds nothing. `recording.json` is read by
`tests/scripts/recorded_pull_request_fixture_tests.py` and by no part of the
application. Recording a fixture and serving one are separate tasks, and only
the first has happened — for either recording.

**The diff content the reviewer sees does not come from the GitHub API.** The
served fixture is `tests/fixtures/github/contracts/v1/<name>/manifest.json`, ten
recorded HTTP exchanges. Its `responses/files.json` carries exactly
`additions`, `deletions`, `filename`, `status` — **no patch**. The file content
the comparison surface diffs comes from `deterministic_git_fixture`: two
base64-embedded git packfiles with hardcoded commit and blob SHAs, imported
through the installed-Git adapter so the public path exercises real pack import.

So a recorded pull request cannot be served by writing JSON. It needs the real
blobs at base and head as git objects. And a `recording.json` as captured
**cannot supply them**: a patch is hunks with context, not whole files, so the
before and after contents are not reconstructable from what was recorded.

### What this card actually needs

1. Extend the recording to carry file contents at base and head, fetched as
   blobs, alongside the patches already recorded. This is still unauthenticated
   public reads.
2. Build a git pack from those blobs, or have the fixture initialise a real
   repository from them, so `deterministic_git_fixture` has something other than
   its hardcoded synthetic graph to offer.
3. Add the fixture selection — a `github_public_path_fixture` value, a directory
   name, and a `--github-fixture` name — and generate the ten-interaction
   manifest with the recorded metadata.

Step 2 is the one with substance. Steps 1 and 3 are mechanical.

## Steps 1 and 2 done, 2026-08-08

**Step 1.** `uiforetw-74`'s five files each carry `base_content_base64` and
`head_content_base64`. All five are modifications, so both sides exist. The
blobs are held against the recording rather than trusted: each pair is diffed
and must produce exactly the additions and deletions the API reported, and all
five reconstruct exactly. Shown non-vacuous by appending one line to a head
blob, which fails with *"blobs diff to +4 -1, and the recording says +2 -1"*.

**Step 2.** `scripts/build_recorded_pull_request_pack.py` builds those blobs
into a two-commit repository and packs it to
`tests/fixtures/github/pull-requests/uiforetw-74/objects.pack`.

| | |
|---|---|
| base | `8b70ee179fe55b705ee4e613ecbc89a0acebc1bc` |
| head | `e40058be7d5fe319342dffb567ce30e42bc6f4de` |
| size | 32,229 bytes |

Deterministic: author, committer and both timestamps are pinned, and two runs
produce byte-identical packs, so a test can name these identifiers rather than
discover them. The generator refuses to write a pack whose diff does not
reproduce the recording file by file — run against `directxmath-336` it fails
correctly, because that recording has no blobs to build from.

**Step 3 remains**, and it was attempted and reverted. What it needs, measured
by doing it:

`scripts/build_recorded_pull_request_contract.py` is committed and works. It
writes the ten-interaction manifest and its responses from `recording.json`, so
the number, title, file list and commit identifiers cannot drift from the
recording. The generated manifest matches `github-com`'s shape exactly — same
top-level keys, same ten operations in order, same interaction keys, every
response file present.

The C++ side is three small edits and they all compile: a
`github_public_path_fixture::uiforetw_74` value, its directory name, its server
contract, and a `--github-fixture uiforetw-74` name. `-Wswitch` catches the
sites, which is why there is no risk of missing one.

Two things stop it, and neither is guesswork:

1. **A capability fixture is required per fixture name.** Without
   `tests/fixtures/github/capabilities/v1/<name>/` the run reports
   `STARTUP_FAILED` before anything else. Copying `github-com`'s gets past it,
   but copying is the wrong answer — the capability set a recorded fixture
   should claim is a decision, not a duplication.
2. **The generated payloads do not satisfy the provider.** With the capability
   fixture in place the run reaches the connect stage and fails with
   `GitHub returned data that BetterPullReview could not safely use` —
   `malformed_response`, evidence `00100000:13:0:-1`. So the response shapes
   need more than matching `github-com`'s key sets; something the provider
   validates is absent or wrong, and finding it is a payload-by-payload job.

And one gate that must be part of the change rather than an afterthought:
`github_contract_fixture_security` **enumerates every fixture file by name**. It
failed on the six files added here, exactly as designed. A new fixture is not
complete until that list includes it, and updating that list is a security
decision — the gate exists so fixture content cannot be added without somebody
looking.

The wiring was reverted rather than left half-applied, so the tree is green. The
generator is kept because it is correct and reusable, and because writing it
again is the least interesting part of the remaining work.

### Step 3 is not mechanical. That was said twice and was wrong twice.

Two further measurements, both from running it rather than reading it.

**The payload shapes needed fixing one level down, and a shallow check hid it.**
Comparing top-level keys said the generated responses matched `github-com`. A
comparison of the *nested* shapes found `pull_request.json` missing `base.repo`
and `head.repo` (`full_name`, `id`) and `user.node_id`, and `commits.json`
missing `author.node_id`. The generator now writes them. This is worth stating
because the shallow comparison had already been reported as evidence that the
shapes matched, and it was not.

**The fixture is written around one synthetic pull request identity.**

```cpp
aContext.Authority.PullRequest != core::pull_request_id{ 3001U }
|| aContext.Authority.Number   != core::pull_request_number{ 17U }
    .Owner = "octo-org",
    .Name  = "review-repository",
```

`github_public_path_fixture.cpp` hardcodes the owner, the repository name, the
pull request number and its identifier. A second fixture serving
`google/UIforETW#74` cannot satisfy those, and the run still fails at connect
with `malformed_response` after the payload shapes were corrected.

So the real step 3 is **parameterising the fixture over the pull request it
serves**, not adding a directory beside the existing one. That is a change to
`github_public_path_fixture.cpp`'s safe-target and authority handling, and it
touches the checks that keep a fixture from being pointed at something it should
not be — which is exactly the code that must not be loosened casually.

**And a design question nobody has answered yet.** `objects.pack` contains
commits `8b70ee17…` and `e40058be…`, built from the blobs. The recorded pull
request names GitHub's real commits `7c519e97…` and `9b01c587…`. Those cannot be
reproduced without the original trees, parents and author records, which the
recording does not carry. So either the served metadata uses the generated SHAs —
and the fixture stops matching the pull request it is named after — or the
recording grows enough to rebuild GitHub's exact objects. That decision comes
before any of the wiring is worth finishing.

## Step 3 slice 1 done, 2026-08-08 — and the evidence was misread

**Owner decision, 2026-08-08.** The design question above is answered: the
fixture serves the **generated** commits (`8b70ee17…` base, `e40058be…` head),
the real↔generated mapping is recorded in the fixture README, and the
divergence is asserted in the contract test. The recording is *not* grown to
rebuild GitHub's real objects.

Validated rather than accepted: rebuilding `7c519e97…`/`9b01c587…` is not
"grow the recording", it is "commit a clone". A commit OID hashes its trees,
parents and author records; `git index-pack --strict
--check-self-contained-and-connected` plus `fsck --connectivity-only` require
every named ancestor to be present; and the `shallow` escape hatch is forbidden
by cache validation. The only cheap variant loosens those gates, which is
prohibited.

### Correction: `00100000:13:0:-1` was never the authority check

This card attributed the reverted attempt's connect failure to payload shapes
and then to the hardcoded fixture identity. The hardcoded identity is real and
is still ahead. The evidence code was misread.

`GITHUB_FIXTURE_EVIDENCE` is eight booleans then
`RecordingFailure:AccountReadCount:LastReadOperation`. `00100000` is
`BootstrapTransportSelected` alone, `AccountReadCount=0`,
`LastReadOperation=-1` — the failure was the **first bootstrap read**, before
any payload was parsed. Counting `contract_failure_code`
(`github_read_contract.hpp:135-160`) from zero, `13` is **`unexpected_headers`**,
not `malformed_response`.

Measured on both sides:

- `build_recorded_pull_request_contract.py` wrote `"safe_headers": {}` and
  `"body": None` for all ten interactions.
- The application compiles three REST headers (`rest_headers()`,
  `github_read_contract.cpp:423-439`) and three GraphQL headers plus a document
  body (`:441-457`).
- `strict_recorded_transport::perform` compares with `aRequest.Headers !=
  expected.Request.Headers`, so an empty table is a mismatch, not a wildcard.

So the ordering is: fix headers first, identity second. The payload-shape work
already recorded above was real and is kept; it simply was not what that
evidence code named.

### What slice 1 landed

The generator is wire-exact. Verified against the `github-com` fixture rather
than against the source that produces both: same operation set, identical
`safe_headers` for every operation, identical GraphQL query, variables
parameterised to `google`/`UIforETW`/`74`.

Served metadata carries placeholder OIDs (`1`×40 base, `2`×40 head) in
`pull_request.json`, `commits.json`, and the ListChecks path — the convention
`github-com` already uses, substituted at load time by the existing
`replace_all`. **Zero new C++ mechanism.** Checked by reading bytes: the real
OIDs appear nowhere in the generated tree.

The generator now refuses a recording whose commit list is not exactly the
head, because the pack builder produces a two-commit repository and any other
commit would be named but absent. Shown non-vacuous both ways: a synthesised
second commit is refused with exit 1 and no directory written, and
`directxmath-336` is refused for this reason as well as for having no blobs.

A note on the measurement, not the code: that refusal was first read through a
pipe, which reported the pipe's exit status as `0`. The true exit is `1`. A
gate measured through `head` or `tail` reports the pipe.

### Where step 3 stands

| slice | state |
|---|---|
| 1 — wire-exact generator | **done**, commit `b87cbad` |
| 2 — commit the fixture tree, extend the enumerating security gate | next |
| 3 — descriptor extraction, behaviour-neutral | not started |
| 4 — the `uiforetw_74` value and its wiring | not started |
| 5 — the end-to-end run | not started |
| 6 — catalog, docs, and this card | not started |

Slice 2 carries a security decision: `github_contract_fixture_security`
enumerates every fixture file by name and must be restructured to a table keyed
by directory without weakening a single check.

### Slice 2's constraints, measured before writing any of it

Generating the tree into `contracts/v1/uiforetw-74/` and running the gate:
`FAIL: test_fixture_tree_is_exact_and_every_byte_is_sanitized`, exit 1, all
eleven files named individually. The gate works. The tree was removed again,
because a red gate must not be committed.

Six of the twelve tests are identity-dependent and two facts about them decide
the restructure. Both were measured, and both make the obvious approach wrong.

**1. A shared expected-field table cannot be reused, and loosening it is a real
weakening.** `EXPECTED_RESPONSE_FIELDS` is asserted with **exact equality**
(`:797-801`), and `json_fields` recursively unions every key in the document.
`uiforetw-74` has legitimately empty `reviews`, `issue_comments`,
`search.items`, and `check_runs`, so its field sets are strictly smaller than
`github-com`'s. Reusing the table fails; relaxing the assertion to a subset
check would weaken it for *every* fixture, including the existing ones. The
answer is a per-fixture exact table, which keeps exact equality intact.

**2. The trap: the artifact key-name scanner is not where it looks.**
`byte_security_violations` runs over raw bytes and is driven by whatever the
tree-shape test iterates, so a new directory is covered the moment it is
admitted to `expected_files` — no registration needed. But
`artifact_field_violations`, which catches forbidden JSON **key names**
(`_links`, `headers`, `request`, `response`, `cwd`, `environment`, `command`,
`local_path`) that carry no forbidden byte substring, is invoked from exactly
one place: `:803`, inside
`test_responses_have_exact_fields_and_equivalent_semantics`, which hardcodes
the two directory paths at `:777-782`.

So adding `uiforetw-74` to the tree-shape test alone makes the gate **green
while the new fixture's JSON keys are never checked at all**. The raw-byte scan
still runs, which is exactly what would make the omission invisible. Routing
the new directory through the artifact-field check is not optional polish; it
is the difference between extending the gate and hollowing it out.

**3. That test also assumes two mirrored hosts.** It byte-compares
`github-com` against `ghes-3.18` for every operation except
`SearchPullRequests` (`:786-795`). `uiforetw-74` is a single identity with no
mirror, so it cannot join that comparison — it needs the per-operation field
and artifact checks without the cross-host byte equality.

Non-vacuity for slice 2 must therefore include: planting a forbidden key name
(not a forbidden byte) in a `uiforetw-74` response and watching the gate catch
it. If it does not, the restructure hollowed out the check and the green run
means nothing.

### Why the estimate was wrong before

This card was previously read as "record a public pull request", and recording
one made it look nearly done. The recording is real and useful — it is what
[[BPR-0214]] needed to establish that a fixture with intraline emphasis is even
obtainable — but the product still has no path from a recording to a rendered
comparison, and that was never measured until now.
