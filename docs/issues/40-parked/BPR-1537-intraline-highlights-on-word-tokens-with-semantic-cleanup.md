---
schema: "implement-rfc.card.v1"
id: "BPR-1537"
title: "Intraline highlights on word and punctuation tokens with semantic cleanup"
status: "Parked"
rfcs: ["RFC-BPR-0082", "RFC-BPR-0004"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-15 (relayed by the coordinator)","owner_note":"I feel like the quality of intra-line diffs has gotten a lot worse from a human perspective since we changed algorithms.","governing_rfc":"RFC-BPR-0082","related_cards":["BPR-1481","BPR-1480","BPR-1485","BPR-1467"],"reservation":"docs/issues/_reservations/BPR-1537--BPR-1537--945227dd29b1.json"}
parked_on: "human-review"
---

# BPR-1537: Intraline highlights on word and punctuation tokens with semantic cleanup

The owner reported on 2026-10-15: "I feel like the quality of intra-line diffs
has gotten a lot worse from a human perspective since we changed algorithms."
[[BPR-1481]] moved a similar pair's intraline ranges from the engine's
budgeted character LCS table to dtl's O(NP) comparison over the pair's Unicode
scalars ([RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md)).
Both are minimal character edits with no cleanup; RFC-BPR-0082's candidate
table already names "semantic cleanup" as the way to change what is
emphasized, and its intraline row makes that a look-and-judge criterion.

This card diagnoses what a reader sees, then computes intraline ranges over
word, number, whitespace and punctuation tokens with dtl as the sequence
engine (still per line only), cleans the result up so small incidental
equalities between edits do not split the emphasis, and falls back to
emphasizing the whole changed extent when the pair shares too little. Spans
stay on UTF-8 boundaries of the original line
([RFC-BPR-0004 §9.2](../../rfcs/RFC-BPR-0004-diff-computation-line-mapping-grouping.md)),
pairing is unchanged, and the core stays pure, deterministic and metered.

## Acceptance

- **AC1, diagnosis:** A corpus of realistic changed line pairs (renames,
  inserted arguments, string edits, reordered tokens, whitespace and
  punctuation changes, mostly rewritten lines; from repository fixtures and
  this repository's history) is run through the pre-switch engine, the current
  one and the candidate, side by side, and the human-visible failure classes
  are named with examples.
- **AC2, engine:** Intraline ranges come from dtl's common tokens of the pair,
  followed by a deterministic cleanup and a low-similarity fallback. Every
  span starts and ends on a token boundary of valid UTF-8; the cost stays
  counted on the engine's step meter and bounded; dtl stays in
  `src/core/src/diff_intraline.cpp`; no new library.
- **AC3, tests:** Corpus golden tests pin the candidate's spans, with a
  negative control that the character-level output fails; the existing diff
  and intraline suites, `core-only`, the affected `dev` tests, clang-format 22,
  tidy on changed lines and the documentation gate pass.
- **AC4 (human):** The owner accepts, on a frozen build and offscreen renders
  of the old, current and candidate algorithms: "intraline highlights read
  better than before".

## Implementation

Core only: `diff_compute.cpp`'s `intraline_ranges` and
`diff_intraline.cpp`. Qt painting is unchanged (other cards own diff paint
for selection visibility). Never drive or capture the owner's desktop; the
owner looks at the frozen build themselves.

## Diagnosis (2026-10-15)

**What changed and when.** On 2026-10-13 three cards replaced the engine's
algorithms: [[BPR-1480]] (rapidfuzz-cpp pair scoring, same score as before),
[[BPR-1485]] (linked xdiff as the primary line matcher) and [[BPR-1481]]
(`2f3d76525`, intraline ranges from dtl's O(NP) over each similar pair's
Unicode scalars). Before BPR-1481, `intraline_ranges` traced the budgeted
character LCS table (`lcs_matches`, ties toward the earliest match). Both are
minimal character edits: there was never a tokeniser, word boundary, cleanup
pass or rewrite fallback, and BPR-1481 changed only which of several equally
short character alignments is drawn, plus giving emphasis to pairs whose
budget had run out. The AC06 render set the owner accepted
(`/Users/jaeger/bpr-attended/rfc-bpr-0082-intraline-v001`) came from the
evaluation corpus (benchmark and Git sources) and showed the two character
alignments side by side, so it could not show what a word-level emphasis
would look like.

**Corpus.** 8,104 one-for-one replaced line pairs mined from this repository's
last 3,000 non-merge commits (C++, Python, Markdown, CMake, JSON), of which
6,759 pair as similar (character similarity of at least 600 permille, the
engine's threshold), plus a 37-pair curated corpus by category (now the golden
test) and the UIforETW#74 and line-details fixtures. Old = the pre-BPR-1481
table's traceback, current = dtl over scalars (`main`), candidate = this card.

| Over 6,759 similar pairs | Old table | Current dtl | Candidate |
|---|---|---|---|
| Emphasised regions per pair | 3.36 | 3.40 | 1.28 |
| Pairs with 4 or more regions | 29.8% | 30.2% | 1.6% |
| Pairs whose emphasis cuts into a word | 64.5% | 64.5% | 11.2% |
| Gaps of at most 2 bytes between regions, per pair | 1.86 | 1.91 | 0.01 |
| Emphasised share of the pair's bytes | 13.6% | 13.6% | 18.6% |

Old and current differ on 1,108 pairs (16%); where they differ, dtl draws more
regions on 343 pairs against 87, and cuts more words on 379 against 224: dtl's
furthest-reaching path picks scattered common letters more often
(`[BPR]-[1527 ]r[epa]i[r]`, `c[]p[p"]`). So the switch made a bad shape a
little worse; the shape itself is the regression a reader sees.

**Failure classes** (every one present in both old and current):

1. Character confetti: renames light letters, `[res]u[l]t[]` against
   `[o]u[]t[come]`, `[conve]r[s]a[tion]s` against `[th]r[e]a[d]s`.
2. Incidental anchors: shared spaces, punctuation and common letters hold
   rewritten text together, `s[ho]w[]s[] []a[]n[] [error]`; hashes become
   stripes, `[9013]e[16]8[3]3[a]6…`.
3. Split identifiers and numbers: `re[ad]`/`re[view]`, `13[6]`/`13[7]`,
   `std::[s]i[ze]_t`/`std::[u]i[nt64]_t`, `count[_p]ublished`.
4. Split operators: `[!]=` against `[]=[=]`, `<[=]` against `<[]`.
5. Too little: one letter of a rewritten word (`T[ints]`/`T[heme]`); too much
   noise elsewhere, never a whole word.
6. No "line rewritten" fallback: a rewritten comment or condition is a
   scatter of fragments, `pat[]t[]ern.[fron]t[]()`.
7. Grapheme splits: ZWJ emoji are cut apart, `[👩]‍[💻]`.

## Design

`src/core/src/diff_intraline.cpp` (still the one diff source that names dtl,
still per line only, no new library):

- Tokens: lowercase run, capitalised word, uppercase acronym (`HTTPServer` is
  `HTTP`, `Server`), digit run, `_` alone, a run of spaces and tabs, an ASCII
  operator run (`!=`, `->`, `::`), and any other character as its grapheme
  cluster; wide (East Asian) characters are one token each. Classes come
  from `utf8::classify_word`, `grapheme_end` and `grapheme_width` (utf8proc).
- Equal tokens are numbered; the common token prefix and suffix are matched
  directly and dtl compares the middle, metered as before; a token grid past
  dtl's 2,000,000-point bound uses the budgeted table over tokens.
- Cleanup, linear: an equality between two changes joins them when it is no
  longer than the larger side of either (diff-match-patch's semantic rule) or
  lies inside one word on both lines, and only when both lines change beside
  it; a change replacing part of a word on both lines takes a one- or
  two-scalar rest of that word; under 40% of the shorter line's non-space
  scalars kept means a rewritten line, emphasised first change to last;
  a spacing-only change is trimmed to the blanks that changed.
- Evaluation components that supply matched scalars are used as they are.
- No diff algorithm revision bump: intraline results live only in the diff
  host's memory cache ([[BPR-1497]]).

## Built and verified (macOS arm64, branch `bpr-1537-intraline-tokens`)

Commits `2429f4ea4` (engine and tests) and `7bda4c32f` (fixture, serving,
docs, reseal) over `main` at `1bd314121`.

- `core_diff_intraline`: changed words; the 37-pair golden corpus (renames,
  arguments, strings and prose, reordering, operators, spacing, numbers and
  hashes, rewrites, case, symbols, CJK, accents, emoji) with an in-test
  character-level control that must fail 28 of 37 and match the other 9; the
  budget, large-pair and stop-inside-dtl tests rewritten for tokens.
  **Negative control:** the same test built against `main`'s three engine
  sources fails at the first golden pair (`⟦res⟧u⟦l⟧t⟦⟧` for `⟦result⟧`).
- `core_diff_compute` (validator fixture now two multibyte words),
  `core_diff_semantics` (clip test on a spaced word),
  `core_diff_compute_components` (a declining component keeps the engine's
  edits; supplied character matches are used as they are) updated.
- `core-only` (Debug, `-g0`) at `2429f4ea4`: all 275 rows other than the benchmark (which passed in the diff run) pass after linking `.agents` into the
  worktree (the three documentation rows fail without it, as on every
  worktree); `coding_format_check` passes (clang-format 22); clang-tidy on the
  changed sources reports only the existing non-`std::exception` throw
  pattern.
- `dev`: `qt_diff_workbench_scale_{100,125,200}`, `qt_diff_axis_scale_*`,
  `application_github_public_path_uiforetw_74`,
  `application_uiforetw_selected_diff_contract`,
  `application_diff_workbench_public_path`,
  `application_line_details_public_path`, `github_recorded_fixture_contract`,
  `github_contract_fixture_security` and
  `application_review_round_trip_composition_contract` (resealed for the
  four test-build fixture tables) pass.
- `python3 scripts/verify_documentation.py --root .` and
  `tests/scripts/verify_documentation_tests.py` pass.

New fixture `intraline-v1` (`scripts/build_intraline_fixture.py`,
`bpr-fixtures/intraline#37`): five files of everyday changed lines, served by
`--github-fixture intraline-v1`.

## Attended review (AC4, pending)

Frozen build `/Users/jaeger/bpr-1537-intraline-v001.ipjf7vxp` from `7bda4c32f`
(launcher `--check` passed: fixture tree present, offscreen launch reached
`ATTENDED_FILE_SHOWN:src/review/comparison.cpp`, executable SHA-256
`f6ddc3fa…f645e`). Launch: `python3 /Users/jaeger/bpr-1537-intraline-v001.ipjf7vxp/launch.py`.

Session `cb6a3d6ecf93aa13c7276f81c5e83475`, state
`/Users/jaeger/bpr-attended/bpr-1537-intraline-v001/state`, repo root the
worktree `/Users/jaeger/.codex/worktrees/bpr1537-intraline/BetterPullReview`,
candidate `BPR-1537-intraline-words` (fingerprint `b0426e07…d739f`), event
cursor 3. One Accept/Reject question: "Intraline highlights read better than
before". Offscreen renders of the same ten lines (old table, current dtl,
candidate) through the product's `diff_workbench` are in
`/Users/jaeger/bpr-attended/bpr-1537-intraline-v001/renders`; offline page
`review.html` beside them.

## Continuation

- On Accept: merge `bpr-1537-intraline-tokens` into `main` (re-check the
  composition contract pins against `main`'s then-current `src/app` and
  reseal on the merged tree if another reseal landed), rerun `core-only` diff
  rows and the dev rows above, and finish the card with AC1 to AC4.
- On Reject: read the note, adjust the cleanup on the named lines (the golden
  corpus carries each category), and prepare a new revision in a new short
  session.
- Remove the frozen root with `scripts/freeze_attended_build.py remove` once
  the review is over.
