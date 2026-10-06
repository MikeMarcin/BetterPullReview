# Ported code provenance

First-party BetterPullReview code that follows code the owner wrote for other
projects. The owner licensed any such port under BetterPullReview's licence, in
chat on 2026-10-05: "theyre all mine, as far as your concerned any port just
gets licensed to the BPR license". So no third-party notice is needed; this
file records where each port came from. Vendored third-party code keeps its
own `PROVENANCE.md` under `third_party/`.

## `bpr::core::fuzzy_match` ([[BPR-1289]])

`src/core/include/bpr/core/fuzzy_match.hpp` and `src/core/src/fuzzy_match.cpp`
rank Settings search results. The Linux station first wrote them from the
card's description of the owner's matchers, which it could not reach. On
2026-10-06 the macOS station compared them with the sources below and ported
what follows. All four are the owner's own code, and every commit to them is
the owner's. Revisions are the last commit to touch each file.

- **containers `gct::strings::fuzzy_matcher`** (the base).
  `/Users/jaeger/Documents/Personal/containers/source/gct/strings/fuzzy_match.hpp`,
  private repository `code.mikemarcin.com:containers`, file at `a7454c4`
  (checkout at `2f5ebf2`); no licence file. Ported: the exact (200,000), prefix
  (160,000) and segment-prefix (140,000) tiers, with a word as a segment; the
  dynamic programme over subsequences (200 per letter, +500 at a segment
  start, +300 at a soft boundary or camel hump, +180 for an adjacent letter,
  −15 per skipped letter); the extra +500 for a first letter at the label's
  start; the −2 late penalty on the match's end position, as the source
  charges it rather than on its start; and the unrestricted Damerau–Levenshtein
  distance (Lowrance and Wagner) over word-prefix windows as the typo rescue
  (one edit up to four letters, two beyond, −2,500 each). Left: the per-term
  segment-prefix tier (with words as segments, two such terms would outscore
  an exact match), wildcards, source masks, the frequency and recency boost,
  reusable workspaces, the SIMD affix trim and the character-mask early reject.
- **Entole `score_candidate`.**
  `/Users/jaeger/Documents/Personal/aito/code/entole_developer_tools/modules/completion/src/Completion.cpp`,
  lines 342 to 419, private repository `aito`, file at `962b48774` (checkout at
  `5081d5b33`); no licence file. Ported: matched ranges for highlighting, its
  consecutive positions merged into runs (`append_range`). Left: its greedy
  first-fit subsequence, which containers' programme supersedes, and its plain
  edit distance.
- **AllTheThings `FuzzyMatcher`.**
  `/Users/jaeger/Documents/GitHub/AllTheThings/Sources/ATTCore/FuzzyMatcher.swift`,
  `MikeMarcin/AllTheThings`, file at `f755aeb` (checkout at `2ba1b33`), MIT,
  "Copyright (c) 2026 Michael Marcin". Ported: case-, diacritic- and
  width-insensitive folding (`normalize`), here as a table for Latin-1, Latin
  Extended-A, full-width forms and Greek and Cyrillic capitals rather than
  Foundation; the substring tier between a word's start and scattered letters
  (`stringExplanation`); spans reported on the source text rather than the
  folded text; the shorter label first within a tier; and its anchors, which
  stop a token's letters being picked out of unrelated words
  (`acronymExplanation`, `shortTokenSubsequenceExplanation`). Here the anchors
  are two rules: the first letter starts a word or follows a soft boundary,
  and a letter in another word than the one before it does too. Left: the
  query language (negation, fields, wildcards) and path, extension and kind
  scoring.
- **FindInMedia `is_caption_spelling_variant`** (legacy Rust).
  `/Users/jaeger/Documents/GitHub/FindInMedia/crates/fim-db/src/lib.rs`, lines
  6236 to 6271, `MikeMarcin/FindInMedia`, file at `f1943cb` (checkout at
  `2639978`); `license = "MIT"` in the workspace `Cargo.toml`, no licence file.
  Ported: the consonant signature (ASCII letters and digits other than a, e, i,
  o, u and y) and its eligibility rule (3 to 12 letters and digits, at least
  two consonants). A typo window qualifies when it keeps the term's first
  letter or, by this test, has the term's consonants, so a vowel slip in the
  first letter is forgiven ("eppearance" finds Appearance). Left: its plain
  two-row Levenshtein (`levenshtein_distance`, line 6257), which containers'
  Damerau–Levenshtein supersedes.
- **BetterPullReview's own choices**, tested in
  `tests/core/fuzzy_match_tests.cpp`: whole-query tiers before per-term
  matching, and a ranking that lists only the nearest reach any candidate made
  (a literal run of the query, then its letters scattered, then a typo). The
  Linux implementation began this rule for typos.
- No source text was copied verbatim; the port is written to BetterPullReview's
  coding standard.
