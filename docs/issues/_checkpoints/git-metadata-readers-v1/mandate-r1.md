# Git metadata readers mandate r1

Assignment: coordinator/integrator execution of RFC-DRAFT-0019 at
`docs/rfcs/drafts/git-metadata-readers/`, adopted as RFC-BPR-0083.
Immutable RFC_BASE_SHA: `9a4f61976bba23727acd77d10c23c041f87208f0`.
Selected recipe: coordinator-integrator revision 1; integrator route
rfc-integrator claude-opus-5-5 high.

Owner decisions carried by the assignment:

- Q1 (answered 2026-10-13): core may link only the owner-approved pinned pure
  libraries glaze, utf8proc, zlib and fmt. Any other third-party code vendored
  into core needs the owner's explicit approval for that code; a vendored
  matcher or reftable reader in core is eligible pending that approval.
- Q2 (reftable handling, a/b/c) and Q3 (GPL-2.0 with linking exception) are open.
  No Q2 option is picked: the reftable part of fix 1, C3's reftable capability
  and AC07 stay blocked on Q2 ([[BPR-1461]]). A candidate that passes but needs
  an open answer is recorded as eligible pending that answer.

Boundaries: candidate libraries only through the verified-archive pattern behind
an evaluation-only option; no live GitHub calls; tests may run Git to build
fixtures and as the oracle, the product never runs Git. macOS is measured here;
Windows and Linux are station cards. Main is advanced only by fast-forward after
merging the latest main and re-running the focused checks. No push.

Amendment (2026-10-13, owner via coordinator): Q2 answered (a). Refuse a
reftable checkout now under fix 1 with a test; decide later, under C3, whether
to read it. The reftable part of fix 1 is unblocked and implemented in
[[BPR-1454]]; [[BPR-1461]] carries AC07's refusal evidence. Q3 remains open and is
asked only if libgit2 passes.
