# BPR-1199 mandate r1

Assignment: `bpr1199-comparison`, milestone `private-comparison`, branch
`codex/bpr1199-comparison`, baseline `de496298e705e72acc32707a4f1b7f6bd5f5101f`.
The implementer owns this isolated worktree and this card only, as a
nondelegating leaf under the coordinator-implementer strategy.

The owner authorized available disjoint milestones, unattended progress and
landing completed slices. The coordinator explicitly permits sanitized local
owner-log inspection and a proportionate diagnostic correlation repair. No
live private PR/API/Git access, remote mutation, main checkout edit, push,
release or trust/version/clone-bound change is authorized here. Native builds
use `CMAKE_BUILD_PARALLEL_LEVEL=2` and `-j2`; interactive Qt checks run headlessly
and serially. Frozen BPR-0966 candidates remain untouched.

The original AC1–AC4 remain intact. A diagnostic repair does not establish the
cause of the private failure, prove supported private rendering, reproduce the
actual underlying failure, or supply the owner AC4 verdict. Those obligations
remain pending until direct evidence exists. An attended observation may be
prepared when it is concrete, with one question per criterion.

Design decision: reuse the existing numeric `window_session` diagnostic field
because epoch requests are only unique within their owning window session;
no private authority or repository text is needed to separate them.
