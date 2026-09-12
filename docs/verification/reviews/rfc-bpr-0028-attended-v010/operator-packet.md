# RFC-BPR-0028 attended session, v010: one-pane Read and Review

Status: **one candidate ready and protected at sequence 366; owner verdict pending; no host or watch running**.

The tests-disabled, locally signed bundle was built from `83d7e676` after
[[BPR-0729]]'s source commit `3dcbf237`. Its executable sha256 is
`a6dc1565908cfacc5eb7d573d8d05983c51aa10780db2052e1c138682870eaf2`;
`codesign --verify --deep --strict` passed. The build and page digests are in
[session-identity.json](session-identity.json). The bundle is at
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app`.

The one pending candidate is `BPR-0729-one-pane-layout`, revision
`app-release-a6dc1565-2026-09-12.6`, fingerprint
`70a2a9417b2532450cf66f71228fbabe1f627de748ed8ea25767cecec875228c`.
It asks **one question**: whether the owner accepts this one-pane layout on
the signed bundle, including Review in one pane and the empty-side ribbon
shapes shown in the packet. The accepted
[direction v7](../../../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane)
is the design reference. [Open the offline review page](offline-package/index.html)
to inspect both same-source rendered examples and save a verdict. The page
is self-contained and retains partial decisions; import its exported JSON
through the attended-review CLI to record an offline verdict.

To inspect the running app, launch the exact signed bundle:

```sh
open /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app
```

On an active comparison, choose **View → One pane**, then switch from Read
to Review. The Review example on the review page shows removed and added
rows with side-specific thread cards. The other example shows a replacement,
pure deletion and pure insertion with one-sided ribbons. Those two images
are **offscreen tests from the same source**, not captures of the signed
bundle. If the owner needs an empty-side case live in the bundle to judge it,
they can reject this candidate and name the file or path to use; no approval
is inferred from the images or a successful launch.

The candidate is ready in session `cbd48b91000b00f1e72e1ad62c7c384c`.
For a live session, run:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve
```

Then arm the driver watch with `wait --after 366`. No host was
started while preparing this packet. The bundle resource and the exact
BPR-0729 source paths stay protected until the decision is consumed or
development is explicitly resumed.

Automated evidence at the source is in
[BPR-0729](../../../issues/40-parked/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md):
14/14 final focused selectors, three composition/public contracts, boundaries,
format, strict tidy and documentation pass. The three-scale composed GitHub
thread crops were inspected. The pure-deletion scroll regression was red
before correction and green at the exact row after it. RFC-BPR-0018 and
RFC-BPR-0028 remain open; the epoch-handle criterion [[BPR-0646]] waits for
the separately authorized [[BPR-0743]] epoch work. The owner's BPR-0735
answer, “unsure, don't care,” is recorded without treating it as a pass or
asking it again here.

No step in this packet writes to GitHub.
