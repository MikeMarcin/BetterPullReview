# Source Find fixture

Synthetic public-path course for RFC-BPR-0077. No remote data or credentials.
`scripts/build_source_find_fixture.py` constructs both complete documents, builds
and verifies their deterministic installed-Git pack, and generates the recorded
read-only contract under `tests/fixtures/github/contracts/v1/source-find-v1`.
The committed artifacts are consumed directly; ordinary builds regenerate nothing.

`src/find.cpp` has 1,100 unchanged numbered rows, offscreen repeated occurrences,
Unicode supplementary scalars, simple-fold variants and CRLF lines. Earlier and
later prefixes distinguish source-side targeting and prohibit an invented
removed/added multiline bridge. `src/other.cpp` proves document invalidation.
