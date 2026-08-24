# Conversation-to-code boundary, v001

This batch asks the bounded product-direction question left by [[BPR-0414]]
after exact conversation anchoring was implemented in commit
`8c7504dd80b28308d29e4d37254004ba3708821b`. The capture is a render from the
real composed application using the repository's recorded GitHub.com contract
fixture. It is not a drawn mockup and contains no live or private GitHub data.

The selected root is a current right-side thread on `review.cpp:42`. The drawer
is filtered to that thread and its reply. The right pane has revealed line 42,
the ordinary source-line selection remains visible, and the narrow blue mark at
the pane's left margin is the conversation anchor. The comment body remains in
the drawer, so neither natural-layout source document gains a synthetic or
inline row.

## Boundary and limits

- The checked-in image was produced by the registered, network-isolated CTest
  course `application_diff_workbench_public_path`, not by invoking the binary
  outside its test containment.
- The course uses the tests-enabled `github-com` fixture, the offscreen Qt
  platform, `QFusionStyle`, and device-pixel ratio 1.0. It proves the composed
  layout and exact selected state, not target-native styling or live GitHub
  behavior.
- Production places a current `RIGHT` anchor only when the provider's
  `headRefOid` matches the accepted head exactly. Production fails closed for a
  `LEFT` anchor because GitHub's pull-request left coordinate is based on the
  merge base while BetterPullReview's left pane is the accepted base tip, and
  the provider response does not carry the merge-base identity needed to prove
  equality.
- Endpoint-agnostic Qt tests exercise exact left placement for a future caller
  that can supply matching coordinates. That is widget-contract evidence, not
  a claim that the current GitHub production path places left anchors.
- The provider rejects any root with a missing/unknown side and any current
  `LINE` root missing its current line. An outdated `LINE` root may instead
  carry only `OriginalLine`. Replies inherit their root anchor. Defensive Qt
  tests still prove that
  synthetic invalid, outdated, or file-level projections receive no guessed
  marker.
- No owner verdict is recorded until a complete digest-bound export from the
  generated page is returned and validated against its manifest.

## Regenerating the capture

From the repository root, build the committed implementation and run the exact
registered course:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure \
  -R '^application_diff_workbench_public_path$'
cp build/dev/tests/end_to_end/diff-workbench-public-path.png \
  docs/verification/reviews/conversation-code-boundary-v001/captures/conversation-anchor.png
```

CTest requires the contiguous evidence block containing `PUBLIC_PATH_OK`,
`GITHUB_FIXTURE_OK:github.com`, `DIFF_WORKBENCH_OK`,
`DIFF_INTERACTION_EVIDENCE_OK`, `CONVERSATION_ANCHOR_EVIDENCE_OK`, and
`RENDER_OK`. The complete environment and artifact identity are recorded in
`capture-provenance.json`.

## Building and validating the offline review

```sh
python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py \
  build \
  --input docs/verification/reviews/conversation-code-boundary-v001/offline-review-input.json \
  --out docs/verification/reviews/conversation-code-boundary-v001/offline-package

python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py \
  validate-verdict \
  --manifest docs/verification/reviews/conversation-code-boundary-v001/offline-package/attended-review-manifest.json \
  --verdict /path/to/exported-verdict.json
```
