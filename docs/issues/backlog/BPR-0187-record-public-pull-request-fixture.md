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
