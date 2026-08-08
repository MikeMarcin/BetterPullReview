# microsoft/DirectXMath#336, recorded

A real public pull request, recorded once and replayed. BPR-0187.

| | |
|---|---|
| source | `https://api.github.com/repos/microsoft/DirectXMath/pulls/336` |
| retrieved | 2026-08-08 |
| head | `c5d86c3dfc28fc661cc03a151a3643f41da7a12d` |
| base | `0332efa39ab80fd1417d5ba120abbde6373e15ed` |
| shape | 2 files, +221 −0, 2 commits, open |

## Why a recording and not a live read

A test that reaches the network fails when the network does, and a fixture that
changes when somebody pushes to a branch is not a fixture. This was fetched
once, committed, and is replayed. The safety rules name recordings as an
accepted surface for exactly this.

## What was recorded, and what was left out

Three unauthenticated `GET`s — the pull request, its files, its commits — then
trimmed to what a review surface reads. Everything else the API returns is
somebody's public data this project has no use for: avatars, node identifiers,
reaction counts, and the two dozen URL fields on every object.

`recording.json` therefore contains exactly one URL, the provenance line above.
That is asserted rather than intended.

**No credential is involved.** These are unauthenticated reads of public
content, so there was no `Authorization` header to record and no rate-limit
header worth keeping. A recording with a place to put a credential invites a
later fetch that fills it in, so there is no such place.

## Why this file rather than the existing fixture

The existing public-path fixture is 1,143 lines of hand-written payloads. It is
correct for driving the automated run deterministically, and it is weak ground
for judging *presentation*, because its content was written to satisfy the code
that reads it. Nobody would notice a diff surface that renders synthetic content
plausibly and real code badly.

This recording is 233 lines of patch across a header and a 220-line `.inl`
addition — real C++, real formatting, real line lengths.

## Not yet wired

Nothing reads this file. Serving it through
`github_public_path_fixture` is the remaining half of BPR-0187 and is
deliberately separate: the fetch needed a network and the wiring does not, so
they are not the same kind of work and a network outage should not block the
second.

Whoever wires it should keep the head OID assertion below, so a re-fetch that
drifted is a failure rather than a silent content change.

## Verification

`github_recorded_fixture_contract` asserts, by reading this file rather than by
trusting the intent behind it:

- the recorded head OID is the one above;
- the shape matches the recorded counts — two files, two commits, +221 −0;
- every file carries a patch;
- the recording contains no `Authorization`, `Cookie`, `Set-Cookie`, bearer
  token, or credential-shaped string;
- the recording contains exactly one URL, and it is the provenance line.
