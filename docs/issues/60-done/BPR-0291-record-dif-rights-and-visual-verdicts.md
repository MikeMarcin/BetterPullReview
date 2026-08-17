---
id: BPR-0291
title: Record final Dif rights and attended visual verdicts
status: Done
rfc: RFC-BPR-0019
depends_on: [BPR-0290]
---

# Record final Dif rights and attended visual verdicts

Bind the final production sources to their exact legal/provenance disposition,
then obtain attended approval of the actual generated motion and appearance.
The rights and visual decisions remain separate immutable subjects.

## Required direct evidence

First validate a final rights record against every reference, neutral-master,
source-frame, manifest, generator, and generated-product digest. Then build an
offline attended visual package containing every source frame and generated
cell at 1x, real-speed clips at 64/72/96 logical pixels, seam and handoff
previews, body/center/baseline overlays, appearance plates at 1.0/1.25/2.0,
and the static rest pose beside every animated representative.

The visual package asks exactly five independent approve/reject questions:
original Dif identity and foreign-identity exclusion; full clip motion at
shipping sizes; registration/seam/handoff; four appearance contexts at three
scales; and the reduced-motion rest fallback. Any affected byte change
invalidates its verdict.

## Completion evidence

- Both returned decisions validate against their immutable manifests and exact
  subject digests, with no missing or extra subject.
- Rejection creates a new repair card or revision; no earlier digest carries
  forward and no adjacent runtime claim is promoted.
- Accepted decisions and validation reports are durable, secret-free repository
  evidence, while generated product assets remain distinct from review media.

## Presented attended package

The immutable offline page is
`docs/verification/reviews/dif-final-v001/offline-package/index.html`, batch
`bpr:/attended-review/betterpullreview/c93e59ae499277a8d2e854f6`. It contains
six independent decisions in the required order:

1. final production-source rights,
   `sha256:13646e6c2807fdbd49e0a467b6b50962cdfe6e6e2ed20b67bf2c729363128fdd`;
2. original Dif identity and foreign-identity exclusion,
   `sha256:a57e5da8f73c3d1519eb6df420957343a3a993b45f0e0a6580617194243108ab`;
3. full clip motion at 64/72/96 pixels,
   `sha256:6ac1f35de6457f6971746f9d94a9afaf2085912367a2a763cca3737d8da7cb5a`;
4. registration, calm seam, and greet handoff,
   `sha256:232406dc5aa201c9fc20da33ce1c744a506b877aba5b5514976af8787eae8893`;
5. four appearance contexts at three scales,
   `sha256:6e8c3a57dfa2c07d34d02528c8f2581c3e67468ee3d0f201119f57a3c89610d1`;
6. the awake static reduced-motion fallback,
   `sha256:2289597f3b6d892090a1e1b13c1607c8a8d05da6392918e3e967f55419c7e121`.

The rights subject binds the accepted BPR-0287 authority, exact external input
identities, final rights grant, provenance, manifest, complete source/generated
digest set, renderer, and scoped foreign-material audit. The five visual
subjects bind every source-derived runtime cell, declared-speed clips at all
three shipping sizes, seam/handoff courses, registration overlays, all 36
appearance plates, and the rest comparison. The package contains 51 files and
is self-contained; no private path, credential marker, or `file://` URL appears
inside it.

The official 17-test attended-review builder suite passes. The input SHA-256 is
`c6408f05b800d3a2f2ed414b9a3978f5a03abdc1ec65881710bd13c71d451efd`,
the manifest SHA-256 is
`e193929eb467185f9224c0af411af645e4d57b061d46601f1bf3fdf103853718`,
and the page SHA-256 is
`96a5c8b343228a38b529bb8f4f3982d110a35792574e5ce6cc939b4268320504`.

## Recorded verdict

The product owner returned one complete verdict for batch
`bpr:/attended-review/betterpullreview/c93e59ae499277a8d2e854f6`. The official
validator accepted the exact export as a complete manifest-bound verdict. All
six subjects are accepted: final production-source rights and the five
independent identity, motion, registration, appearance, and reduced-motion
visual criteria. No subject is rejected, omitted, added, or reordered.

The byte-preserved export is
`docs/verification/reviews/dif-final-v001/human-verdict.json`, SHA-256
`e2c0c426732076312ae03fd55349ff1f721cf2e18bdca96f346ae4443d1cfe9d`.
The durable validation record is
`docs/verification/reviews/dif-final-v001/verdict-validation.json`. It binds
the 38,085-byte manifest, 2,108-byte verdict, and official validator hashes and
records `valid complete verdict`.

The accepted identity subject includes the owner's note that the plus/minus
card should not be part of the core base sprite. This does not reject the exact
RFC-BPR-0019 art-direction subject, but it is a mandatory downstream
composition constraint: RFC-BPR-0020 may not infer approval to make that prop
inseparable from Dif's base. Any asset-byte revision must receive a new
digest-bound verdict.

The export expressly leaves package release, RFC-BPR-0019 Auto-Verification,
RFC-BPR-0020 authorization, runtime composition, and the runtime mascot
unapproved. This card therefore closes only the exact final-source rights and
attended visual gate it owns.
