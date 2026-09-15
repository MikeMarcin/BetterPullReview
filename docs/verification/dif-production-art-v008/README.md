# d10 external Dif result intake and structural catalog candidate

This directory retains the exact files supplied for run
`dif-takeover-20260914`, dispatch `d10`, producer `rfc-integrator-d10`, mandate
revision `7`. It is a candidate evidence package. It does not replace the
production source under `src/adapters/qt/assets/dif/` and does not establish a
human art or derivative-rights verdict.

## Exact intake

The supplied result archive `dif-d9-poses-no-eye-whites.zip` is 52,533,993
bytes with SHA-256
`714ea8bd255695cc50d09b9895de6ebc94dd83e965c0ede0a28f3dcdc0ce1126`.
Its extracted `poses/` tree contains exactly 71 PNGs: rest 1, calm idle 12,
greet 8, blink 6, compare glance 8, consider 8, comment ready 8, attention 6,
acknowledge 8 and blocked 6. The sorted
`path<TAB>size<TAB>sha256<NEWLINE>` inventory of those PNGs plus the adjacent
`eye-fix-log.json` has digest
`f31d657e52bb6b93a172ecdf15a7fea9c6bc9d3acb75166428fe066e0f8b4eb7`.
The original bytes and supplied `eye-fix-log.json` are under [incoming](incoming/).
[intake-manifest.json](intake-manifest.json) records every file hash, canonical
PNG hash, decoded and runtime RGBA hash, PNG chunks, alpha bounds and
transparent-pixel result.

The result was produced from the d9 handoff archive whose retained local copy
has SHA-256
`cf1390d284b2abcd9af6de07f991ab17b1c7ac1efe7369ff42ef57c60359e751`.
The result package does not include `generation-log.json`, generator provider,
model, seed or version, exact per-pose prompts and input roles, native editable
layers, pre-catchlight-removal hashes, external service terms or third-party
notice obligations. The supplied eye-fix log identifies 68 edited files but
does not fill those gaps.

## Objective result

All 71 PNGs decode as 1024×1024 RGBA8. They contain no nonzero RGB beneath
zero alpha. At 256 runtime pixels the union clears 45 pixels left, 19 top, 45
right and 33 bottom; every frame stays on baseline 223. The candidate uses the
measured neutral regions and one-pixel tolerance in
[candidate/dif-sprite-manifest.json](candidate/dif-sprite-manifest.json).
Canonical generation produces ten strips, 71 substantive cells, 20 gutters and
a 22.75 MiB decoded catalog. Generate, validate, verify and empty-output
reproduction pass. [prepare.py](prepare.py) verifies the retained intake hashes,
rebuilds the structural harness in an empty output directory and renders the
exact [preview](preview/).

This objective pass does not decide whether a pose reads well. Declared-speed
APNGs were inspected at 64 and 96 pixels on light and dark surfaces. Greet,
blink, acknowledge and blocked are coherent candidates. Calm idle has broad
lower-torso deformation. Compare glance uses full-head yaw with possible edge
notches. Consider changes the shoulder and chin abruptly. Comment ready jumps
from tucked paws to fully open palms. Attention changes its open paw and head
abruptly. Those observations determine the scope of the attended questions;
they are not human decisions.

## Neutral identity conflict

The returned rest and every neutral endpoint share one decoded image with
file SHA-256
`a6bb1934dd7336e575abe1feac7b7714354559002abdb648189c8ec991460b49`.
It is not the exact neutral accepted earlier. Against the canonical accepted
neutral SHA-256
`e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`,
alpha is identical while 1,424 RGB pixels differ inside
`[398,345,643,376)` on the 1024 canvas. The visible change removes the white
eye catchlights. Treating this as the old accepted subject would be false, and
mixing the old neutral into only the endpoints would introduce a facial pop.
The exact no-catchlight endpoint therefore remains a separate identity
candidate in the focused review.

## Review and disposition

[review/player.html](review/player.html) presents all ten exact compiler APNGs
at their declared timing with 64, 72 and 96 pixel controls on light, dark and
black surfaces. [review/neutral-identity.png](review/neutral-identity.png)
compares the earlier accepted neutral with the returned endpoint. The attended
manifest binds each visual question separately so partial decisions survive.
The derivative question states the missing provenance rather than inferring
authority from receipt of the files.

The fresh external-state attended session is
`7e489c0dd362c256bc28715a16892b65`. Its twelve static candidates are ready
and protected at sequence `110`: one changed-neutral identity decision, one
decision for each declared-speed clip and one exact derivative disposition.
All bind candidate commit
`1b7daff5ec0d6ad2015e50e22b89c5193ebdbbfc`. The
[candidate manifest](review/candidate-manifest.json) has SHA-256
`df4d6dc8a65ab0f5dffd41edae9ad2d80dd1deb2bb6537931d79d42aac79f937`;
the [offline fallback](review/offline-review.html) has SHA-256
`44d1ac0697ad18f292420ef268b0705e21d8099260a61789c329753a2a119008`.
Each candidate carries one criterion so a decision cannot hide another
unanswered question.

Independent review found that the first prepared player's CSS forced every
image to 96 pixels even after its 64/72 controls changed the HTML attributes.
All twelve unanswered candidates were explicitly released before repair. The
fixed player has SHA-256
`d06666d505c586cf0c5d59321c652cedee19f754bb28a866e9b8493e82cd2584`;
browser computed style measured 96×96 initially, 64×64 after the 64-pixel
button and 72×72 after the 72-pixel button. Current revision
`1b7daff5-dif-d10-v008.3-protected-player-d06666d505c5` also protects the
player and the entire compiler motion directory for every candidate. The
released v008.1/v008.2 definitions received no decision.

Until those exact candidates receive their scoped human verdicts, BPR-0755,
BPR-0756 and BPR-0757 remain incomplete. No source or generated runtime asset
is promoted from this package.

## Native execution record

The integrator dispatched a read-only `rfc-scout` visual audit and a read-only
`rfc-utility` intake audit with `fork_turns: none` and no optional model or
effort override. The configured custom roles were selected before dispatch.
The scout banner reported `gpt-5.6-sol` / `low`; the utility banner reported
`gpt-5.6-luna` / `xhigh`. These reported banners are recorded separately from
dispatch arguments and are not serving telemetry or evidence of runtime role
enforcement. Both leaves returned without writing the repository.

After integration, the integrator dispatched one read-only `rfc-reviewer` with
`fork_turns: none` and no optional route override. Its banner reported
`gpt-6-astra` / `medium`. It found the initial player-size defect described
above; after correction it independently reproduced the structural candidate,
matched all 59 preview files, checked the 12 refreshed candidate bindings and
reported no remaining blocking or nonblocking finding. This banner likewise
does not establish serving telemetry or runtime enforcement. The reviewer made
no repository edit.
