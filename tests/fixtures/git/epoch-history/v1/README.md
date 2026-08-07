# Deterministic epoch-history fixture

This secret-free SHA-1 graph drives the public GitHub.com and GHES epoch
paths. `head_one` and `head_two` are siblings of `base`, so replacing the
advertised pull-request head is a proven non-fast-forward update. The second
pack is self-contained but deliberately omits `head_one`; retention of that
old head after the replacement therefore comes only from BetterPullReview's
private cache refs.

Every commit uses author and committer
`BetterPullReview Fixture <fixture@betterpullreview.invalid>`, timezone
`+0000`, the timestamp in `manifest.json`, and the recorded message. Each tree
contains one `100644 review.cpp` entry whose content is recorded in the
manifest. A deterministic semantic regeneration recipe is:

1. Initialize a temporary bare SHA-1 repository with system/global config
   disabled.
2. Write each recorded content value with `git hash-object -w --stdin`.
3. Write the corresponding one-entry tree with `git mktree`.
4. Use `git commit-tree` with the recorded identity, timestamp, message, and
   parent. Both heads name the base as their sole parent.
5. Create one self-contained reachable-object pack for `base + head_one` and
   another for `base + head_two` with `git pack-objects --revs --stdout`.

Pack byte streams are frozen in the test-only public fixture source and bound
to exact sizes and SHA-256 digests by the manifest. Git/zlib versions may
encode a semantically identical regenerated pack differently, so the
contract verifies exact frozen bytes separately from exact regenerated Git
object identities and topology.

## Constraints the content must satisfy

The line counts are not the whole contract. Two properties of the *text* are
load-bearing, and BPR-0197 established both by breaking them:

**Every line is unique.** The comparison is computed by the application, not
read from this manifest. A repeated line lets the longest-common-subsequence
match somewhere else and reshapes the alignment while every recorded count
stays correct.

**The replacement's two sides must not pair line by line.** The core pairs
lines whose similarity clears `SimilarityThresholdPermille`, and a paired line
splits the four-into-seven replacement into equal-width segments. The
`--diff-workbench-probe` course reveals the first replacement segment at 0.2
and 0.8 and requires the right pane to advance further than the left, which is
true only while that segment really is four lines against seven. An earlier
regeneration whose two sides both read as calibration code paired on
`read_scale`, produced a one-line-to-one-line first segment, and failed that
assertion with no indication of why. The two sides now share no identifier.
