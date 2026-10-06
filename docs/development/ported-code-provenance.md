# Ported code provenance

First-party BetterPullReview code that follows code the owner wrote for other
projects. The owner licensed any such port under BetterPullReview's licence, in
chat on 2026-10-05: "theyre all mine, as far as your concerned any port just
gets licensed to the BPR license". So no third-party notice is needed; this
file records where each port came from. Vendored third-party code keeps its
own `PROVENANCE.md` under `third_party/`.

## `bpr::core::fuzzy_match` ([[BPR-1289]])

`src/core/include/bpr/core/fuzzy_match.hpp` and `src/core/src/fuzzy_match.cpp`
rank Settings search results.

- **Algorithm:** the owner's `gct::strings::fuzzy_matcher`, in the private
  repository `containers`, `source/gct/strings/fuzzy_match.hpp` at `a7454c4`,
  as [[BPR-1289]] records it. That covers the exact, whole-query prefix and
  word-prefix tiers; the subsequence weights for characters, word starts, soft
  boundaries, adjacency, skips and a late start; and the Damerau-Levenshtein
  typo rescue (one edit up to four letters, two beyond, 2,500 per edit).
- **Folding:** case, Latin diacritics and full-width forms, after the owner's
  AllTheThings `Sources/ATTCore/FuzzyMatcher.swift` (`MikeMarcin/AllTheThings`
  at `f755aeb`, MIT, "Copyright (c) 2026 Michael Marcin").
- **No source text was copied.** The Linux station that implemented it could
  not reach either repository, so the code was written in BetterPullReview from
  the card's description. The word-prefix tier, the first-letter rule for
  typos, and listing typo matches only when nothing matched directly are this
  implementation's own choices, tested in `tests/core/fuzzy_match_tests.cpp`.
- Entole's `Completion.cpp` and FindInMedia's `is_caption_spelling_variant`,
  also named on the card, were not used.
