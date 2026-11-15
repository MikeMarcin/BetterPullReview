# Tour push fixture v1

Synthetic public test data only; no remote repository, credentials or network.
[[BPR-2117]], RFC-BPR-0106 AC04. The reading fixture's two six-line files
(`review.cpp`, `secondary.cpp`): a base, a first head that changes every line,
and a second head pushed on the first that changes exactly `review.cpp` line 2
and `secondary.cpp` line 5. A tour accepted at the first head then has a stop
whose own lines change, a stop whose link changes, and a stop that does not.

`initial.pack` holds the base and the first head; `replacement.pack` holds the
base and both heads. Served by the reading descriptor when
`TourPushCourse` is set (`application_review_tour_publication_push`).

The manifest pins commits, blobs, trees and SHA-256 pack digests. Generate
deterministically with `python3 tests/scripts/verify_tour_push_fixture.py
--generate`; verify without writes by omitting `--generate`.
