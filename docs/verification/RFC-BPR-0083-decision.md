# RFC-BPR-0083 decision record

[[BPR-1460]] applies [RFC-BPR-0083](../rfcs/RFC-BPR-0083-git-metadata-readers.md)'s
decision rules to each Git metadata component on the evidence in the
[verification ledger](RFC-BPR-0083.md). Mandate `git-metadata-readers-r1`;
immutable RFC_BASE_SHA `9a4f61976bba23727acd77d10c23c041f87208f0`. Evidence is
macOS (arm64, Git 2.50.1); the Windows and Linux halves of H2 and H5 belong to
[[BPR-1462]] and [[BPR-1463]] and are pending.

## Owner answers relied on

- **Q1** (2026-10-13): core may link only glaze, utf8proc, zlib and fmt; any
  other code vendored into core needs the owner's approval for that code.
- **Q2** (2026-10-13): option (a). A reftable checkout is refused now under fix 1
  ([[BPR-1454]], [[BPR-1461]]); whether to read it is decided under C3 below.
- **Q3** (GPL-2.0 with a linking exception in the shipped binary): **not asked.**
  The RFC asks it only if L passes H1–H3, H5 and H6 for some component; L passes
  none.

## Candidates against the hard gates

| Candidate | H1 | H2 | H3 | H4 | H5 | H6 | Eligible for |
|---|---|---|---|---|---|---|---|
| K, first-party readers fixed by BPR-1454 | pass: every divergence classified ([ledger](RFC-BPR-0083.md#bpr-1455-and-bpr-1456-the-differential-harness-and-canary-course)) | pass on macOS; Windows and Linux pending | pass: SHA-1 and SHA-256 in the shipped build | pass: first-party code | pass on macOS (`core-only`, architecture checks) | pass: every case under 1 s ([ledger](RFC-BPR-0083.md#bpr-1457-fuzz-oracles-adversarial-timing-and-scale)) | every component |
| L, libgit2 1.9.7 | fail: silent divergences in every component (a version 0 `refStorage`, a config Git refuses), plus C3 depth, C5 literal-prefix `**`, C6 directory and quoted patterns | fail: follows symlinked rule files and includes out of the root, opens an outside FIFO, opens paths itself | fail: SHA-256 only in the experimental build | open: needs Q3 and legal review (GCC-exception wording, unlisted LGPL xdiff) | not measured beyond macOS | fail: exponential wildmatch for C5 and C6 | none |
| V, a vendored wildmatch | (matches K in the C locale; folds Latin-1 under a UTF-8 locale) | not separately evaluated | n/a | dual terms (GPL-2.0 with exception, Rich Salz's acknowledgement) | needs the owner's approval to sit in core (Q1) | fail: exponential on star patterns | none as is |
| G, gitoxide | desk check only | — | — | — | fail: second toolchain, FFI and crate packaging | — | none (rejected) |

Details and measurements: [L](RFC-BPR-0083.md#bpr-1458-candidate-l-libgit2),
[V and G](RFC-BPR-0083.md#bpr-1459-candidates-v-and-g).

## Verdicts

| Component | Verdict | Gate or rule that decided it | Evidence |
|---|---|---|---|
| C1 index | **Keep K** | Rule 1: K passes H1, H2 (macOS) and H6; no candidate is eligible (L fails H1, H2, H3, H6 and refuses split and sparse indexes K reads) | harness C1, `review_cycle_git_readers` rows `index-checksum` and `index-extension`, scale bench |
| C2 repository format | **Keep K** | Rule 1; L fails H1 (reads a version 0 `refStorage` and a config Git refuses), H2 (follows includes out of the root) and H3 | harness C2, row `format`, `core_review_cycle_manifest` |
| C3 HEAD and references | **Keep K; do not read reftable now** | Rule 1; L fails H1 (symbolic depth), H2 and H3. Rule 2's capability case, reading reftable under Q2, has no candidate: libgit2 1.9.7 has no reftable code, nothing exists to vendor, and G is rejected | harness C3, rows `reftable`, `outgoing-reftable`, `head-symbolic`, `head-stale`, harness `head-symbolic-too-deep` |
| C4 Git directory discovery | **Keep K** | Rule 1; L is not eligible (it opens the repository through config reads that follow includes, H2) and K wins ties | harness C4, canary course |
| C5 ignore | **Keep K** | Rule 1; L fails H1 (literal-prefix `**` keeps a path Git ignores), H2 (follows symlinked rule files) and H6; V fails H6 and needs the owner's approval | harness C5 and C5-capture, H6 table, V differential |
| C6 `filter` attribute | **Keep K** | Rule 1; L fails H1 (directory and quoted patterns), H2 and H6 | harness C6, canary course |

**Rule 3** (a replacement must not slow the scale bench or raise its memory by
more than 10%) does not apply: nothing is replaced. K's own figures (index parse
787 ms with the trailer checked, ignore walk 358 ms, 135 MB, Debug on a loaded
machine) are the baseline for any later proposal.

**Rule 4.** Each component keeps exactly one product implementation, the
first-party readers. The differential harness (`git_metadata_harness`), the
reader rows (`review_cycle_git_readers`), the bench row
(`review_cycle_git_reader_scale`) and the fuzz oracles stay as regression gates.
libgit2 remains acquirable only behind `BPR_EVALUATE_LIBGIT2` for re-evaluation;
the shipping configuration acquires nothing new.

**Nothing moved**, so the system architecture and the workspace adapter's
description need no change beyond what [[BPR-1454]] already recorded in the
[domain model](../architecture/domain-and-state-model.md) (the named refusals and
`core.ignoreCase`).

## What could change a verdict

- **K's H2 on Windows and Linux** ([[BPR-1462]], [[BPR-1463]]). A failure there
  is a defect to fix in K: no other candidate is eligible.
- **Reading reftable.** Git 3.0 is announced to make reftable the default for new
  repositories, so refusals will grow. The ways to read it are a first-party
  reftable reader in core, which AGENTS.md says needs the owner's agreement
  before a major format parser is hand-written, or a library release that reads
  reftable and passes these gates (libgit2 has a backend only on its main branch;
  2.0 is expected to change API and ABI). Re-run this harness against any such
  candidate.
- **libgit2 2.0.** If it ships SHA-256 by default, reftable and `O_NOFOLLOW`-style
  containment, H2, H3 and part of H1 could change; its wildmatch would still
  need Git's abort rules for H6, and Q3 would then be asked.
