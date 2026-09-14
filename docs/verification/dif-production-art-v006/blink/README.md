# BPR-0756 d8 blink material and timing candidate

Checkpoint: **unaccepted six-frame local candidate; static agent inspection passed;
actual-speed browser observation and human visual verdict remain pending.** No production
source, runtime, card, RFC, or closeout state changed. The core decides, the host
performs, Qt presents. Run `dif-takeover-20260914`, dispatch d8, mandate revision 5.

## Result and remaining judgment

The retained fresh closed-lid donor and local material correction remove the v005
pale lower-eye sliver in agent inspection at 64, 72, 96 and 256 pixels across light,
dark and black backgrounds. Each eye is registered independently. Donor gradients
are matched to the accepted patch boundary with an RGB plane; the donor supplies
texture and a narrowed crease. Half lids occlude the upper eyeballs while retaining
the original lower eyeballs. This completes the six authored source frames in the
specified open/half/closed/half/open/open order, with 100 ms per frame.

The native 1024-scale material still deserves close visual review: it combines two
separately fitted donor interiors with the neutral's surrounding texture. No numerical
lock establishes a seamless plush-material or natural-motion judgment. The final
catalog remains unaccepted. The single-decision [operator packet](OPERATOR.md) and
self-contained [timed preview](preview.html) are prepared; they contain no fabricated
verdict or global attended session. Other catalog clips are outside this assignment.

## Provenance and method

The accepted neutral is [the v001 master](../../dif-production-art-v001/dif-neutral-master-candidate.png),
SHA-256 `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
The built-in imagegen tool edited that exact image using the exact [prompt](PROMPT.txt).
Its unmodified retained [donor](closed-donor.png) is 1254×1254 RGBA, SHA-256
`573661259948227f86ce7966ae647696fd9f179df5f443859bbf8456ad0b49a9`.
The requested 1024 output size was not obeyed. Its full body is not selected.
[provenance.json](provenance.json) records the original built-in save path; model ID
and seed were not exposed and remain unknown. Reproduction means retained-byte
assembly, not generative prompt replay.

[prepare.py](prepare.py) resamples the retained donor to 1024, independently maps
its eye centers (383,641) to neutral centers (398,625), and samples at 1.25× horizontal
rate to narrow each crease. Ellipses at (398,389)/(625,389), radii (49,65), retain
the v005 strict eye boundary. Boundary-ring least-squares RGB planes compensate the
smooth donor shading difference; an inward feather blends only within those ellipses.
Half-lid coverage ends at native y=390 and leaves everything below exact neutral.
No alpha is imported from the donor. Native source poses are canonical PNGs.

The existing shipping compiler's `resize_box` and PNG encoder produce 256 cells,
including its explicit zero-RGB handling for rounded transparent samples. No competing
shipping generator was added. The eight-cell candidate strip adds the two blank
end cells around six content cells. Native accepted-neutral bytes reduce identically
for frames 0,4,5. Body/feet are copied; no bounding-box recentering occurs.

## Focused evidence

[checks.json](checks.json) pins all output payloads, input/recipe/compiler hashes,
registration coefficients and pixel counts. Half poses change 9,350 native pixels;
closed changes 19,801. Every frame has zero outside-eye changes, zero alpha changes
and zero RGB under alpha zero. The 256 alpha envelope is [47,22,210,223], preserving
baseline 223. Neutral frames 0,4,5 are exact; frames 1 and 3 are exact reversals.

[verify.py](verify.py) passes native/256 locks, six-frame inventory, three distinct
poses, exact strip cells and blank gutters, adverse alpha/outside-eye mutations, and
empty-output canonical-pixel plus metadata regeneration. Existing payload SHA-256
checks remain separate from compressor-independent pixel replay. Pillow 12.3.0 and
NumPy 2.5.2 were used. The documentation gate returned `DOCUMENTATION_OK`. Four theme plates and the v005 comparison were visually
inspected by the agent. These are art evidence, not native Qt runtime proof.

The timed page paints the 256 candidates through Canvas 2D at actual CSS sizes,
uses requestAnimationFrame with six 100 ms slots, and shows measured paint indices,
missed frames, and neutral-return time when run. **No actual-speed playback was
observed here.** Chrome was unavailable; IAB rejected visible tabs in a subagent;
its file URL load was explicitly blocked by browser URL policy. No workaround was
attempted. The operator must run the prepared page to supply motion evidence.

## Reproduce

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v006/blink/prepare.py
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v006/blink/build_preview.py
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v006/blink/verify.py
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/scripts/verify_documentation.py --root /Users/jaeger/Documents/GitHub/BetterPullReview
```

## Exact 256-frame identity

| Index | Pose, each 100 ms | PNG SHA-256 |
|---|---|---|
| 0 | neutral | `21ddf78662c4f840a78ddcba366a4fedf300b79021b514b93362639ef62004a5` |
| 1 | half lowered | `d7db8da5a9562e45e1176dd89ee64617c5affa7efaadd072c1b37601d80803e7` |
| 2 | closed | `f6501b54a26e6e1140436614d2cad5ec9345e2f3d6cf69a1a93fe81ae8a6bbc9` |
| 3 | half reopened | `d7db8da5a9562e45e1176dd89ee64617c5affa7efaadd072c1b37601d80803e7` |
| 4 | neutral | `21ddf78662c4f840a78ddcba366a4fedf300b79021b514b93362639ef62004a5` |
| 5 | neutral tail | `21ddf78662c4f840a78ddcba366a4fedf300b79021b514b93362639ef62004a5` |

## Review preparation and quiescence

The enabled-tool inventory was searched for skill invocation before the direct
`.agents/skills/attended-review/SKILL.md` fallback; no callable invocation API exists.
The repository skill and session contract guided the single-decision operator packet.
The integrator explicitly requested keeping it local until the catalog is complete;
no shared session, external registry, live review server, or verdict intake was created.
All owned commands are finished; no owned process or pending image generation remains.
