# Profile window restoration fixture v1

Synthetic public test data only; no remote repository, credentials or network.
The SHA-1 graph contains one root base and two sibling heads. Both six-line
files (`review.cpp` and `secondary.cpp`) change at each head. Each line declares
a unique integer, so source lines 1, 2 and 3 are independently selectable.

`initial.pack` contains base and head1; `replacement.pack` contains base and
head2 but deliberately excludes head1. The replacement therefore cannot supply
the retained older head: successful restoration proves the private cache kept it.
Each pack is self-contained and passes installed Git's strict import.

The manifest pins commits, blobs and SHA-256 pack digests. Generate deterministically
with `python3 tests/scripts/verify_profile_window_restoration_fixture.py --generate`;
verify without writes by omitting `--generate`. GitHub.com recorded contracts are
reused by an isolated serving overlay that projects these two changed files.
