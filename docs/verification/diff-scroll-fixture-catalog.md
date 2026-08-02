# Diff and Scroll Fixture Catalog

Fixtures must use repository-approved synthetic or redistributable text. Do not commit private source merely because it appeared in a local comparison recording.

## A. Semantic diff fixtures

### A1. Identical text

- same lines and final newline;
- one equal segment;
- no change groups or ribbons;
- 1:1 mapping.

### A2. Right-only insertion in the middle

- at least five equal lines before and after;
- insertion span of several lines;
- empty left span at exact boundary;
- one insertion group;
- scroll pinning proof.

### A3. Left-only deletion in the middle

Mirror A2.

### A4. Insertions/deletions at start and end

Prove boundary zero, final boundary, end caps, and complete reachability.

### A5. Unequal replacement 4:7

- both spans nonempty;
- no internal anchors in one variant;
- expected measured slope ratio.

### A6. Unequal replacement 7:4

Mirror A5.

### A7. Replacement with internal anchors

Construct:

```text
3 paired lines
0:4 insertion
5 paired lines
2:2 replacement
4:0 deletion
10 paired lines
```

Prove the semantic result is refined enough for block-following scroll rather than one coarse hunk ratio.

### A8. Repeated similar lines

Exercise deterministic tie breaking and weak-pair rejection.

### A9. Whitespace and tabs

Cover exact, ignore-trailing, and configured whitespace modes. Preserve original display text and tab geometry.

### A10. Line endings and final newline

- LF versus CRLF under both significant and ignored policy;
- missing final newline on either side;
- no phantom source line.

### A11. Unicode

- multibyte UTF-8;
- combining marks or grapheme-adjacent edits;
- intra-line ranges remain valid text boundaries.

### A12. Empty files

- empty/empty;
- empty/nonempty;
- nonempty/empty;
- valid boundary and scrollbar behavior.

### A13. Rename with content change

Stable file identity is preserved independently of path; diff key still includes exact blobs.

### A14. Binary/unavailable

No text segments, no fake ribbons, explicit diagnostic.

### A15. Moved block

Primary path remains deletion plus insertion; move relation metadata exists; ordinary center ribbons do not cross.

### A16. Canonical-record validator corpus

The checked-in `core_diff_model` target supplies the BPR-0072 record-level
foundation for A1-A5, A12, A14, and A15:

- exact table cases for empty/equal/right-only/left-only/replacement/mixed and
  moved-relation records;
- an entire added file with no left blob or invented left source line;
- zero and `UINT32_MAX` half-open boundaries without arithmetic overflow;
- retained normalized option policy and exact authority/repository/PR/anchor/
  blob/file/options/algorithm identity replay;
- malformed coverage, span, kind, group, pair, move, diagnostic, and identity
  mutations that the public validator rejects;
- seeds 1 through 512 generating deterministic canonical alignments and one
  rejected malformed derivative per seed.

This is semantic-record evidence, not a claim that the BPR-0073 text-diff
engine or the BPR-0074 mapping/navigation surface already exists.

## B. Measured geometry fixtures

Use a deterministic test pane that supplies explicit line-boundary coordinates, then repeat selected cases through the live pane implementation.

### B1. Equal fixed-height lines

Both sides move 1:1.

### B2. Right insertion

Left begin/end coordinate equal; right interval nonempty. Assert constant left projection across several `q` samples.

### B3. Unequal line heights

Different font metrics or synthetic visual heights. Assert max-extent construction and endpoint convergence.

### B4. Different viewport heights

Each side uses its own rail coordinate while sharing `q`; ends remain reachable.

### B5. DPI and zoom changes

Invalidate measured geometry exactly once per settled change and reattach ribbons.

### B6. Divider resize

X geometry changes; document Y mapping and semantic selection remain unchanged.

### B7. Future fold/wrap invalidation seam

Even if folding/wrapping remains disabled, prove the invalidation API can replace measured geometry without changing semantic IDs.

## C. Input fixtures

### C1. Traditional wheel

`angleDelta` with platform wheel-line setting.

### C2. Smooth trackpad

Small `pixelDelta` sequence with phases and subpixel accumulation.

### C3. Natural-scroll inversion

Honor the Qt event semantics once; do not double invert.

### C4. Left/right receiver equivalence

Run the same input sequence over each pane and compare the `q` trace.

### C5. Forward/reverse repetition

Repeat forward and reverse through insertion and replacement segments many times. Assert no cumulative origin drift.

### C6. Shared scrollbar

Slider move, page step, single step, minimum, maximum.

### C7. Programmatic reveal

Previous/next change and selected line use the shared controller.

### C8. Horizontal isolation

Horizontal wheel/shift-wheel or scrollbar changes one pane without changing `q`.

## D. Async and lifecycle fixtures

### D1. File switch before completion

Old file result arrives last and is rejected.

### D2. Epoch switch before completion

Old epoch pair result is rejected.

### D3. Options switch before completion

Old whitespace/algorithm result is rejected.

### D4. Repository/PR switch

Identical path cannot cause cross-repository result adoption.

### D5. Close during work

Completion after widget destruction is harmless.

### D6. Cache key collision attempt

Same path and line count but different blobs/options must miss.

## E. Scale fixtures

### E1. Large mostly-equal file

At least tens of thousands of lines with sparse changes. Prove visible candidate selection does not scan every segment per frame.

### E2. Many small changes

Large segment count with a small viewport. Record visible/near-visible path count.

### E3. One very large insertion

Prove pinning, bounded paint, and complete traversal.

### E4. Rapid input and resize

Coalesce repaint while preserving accumulated final position and correct geometry generation.

## Expected fixture metadata

Each fixture should record:

- exact left/right text digest;
- diff options and algorithm revision;
- expected segment/group/pair IDs or stable canonical representation;
- expected mapping cases;
- expected comparison-axis samples where applicable;
- expected diagnostics;
- whether rendered evidence is required.
