# Dif d6 scoped image-edit inputs

Run `dif-takeover-20260914`, dispatch `d6`, producer `rfc-integrator-d4`,
mandate revision `3`. The user explicitly authorized offline image editing
for connected greeting-arm poses. These are unaccepted **candidate inputs**,
not normalized source frames or shipped assets. The built-in image-generation
edit tool produced them; no CLI/API model or credential was used.

The first edit used the exact accepted prop-free neutral candidate
`docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png`
(`bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`)
as edit target and immutable incoming `dif-greet.png`
(`1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58`)
as supporting pose reference. Prompt:

> Use case: precise-object-edit. Asset type: one high-resolution transparent
> character pose for a fixed-cell application sprite. Image 1 is the exact
> accepted Dif neutral and is the edit target. Image 2 is a supporting pose
> reference only: its raised padded paw shows the intended greeting, but its
> open mouth and different body proportions must not transfer. Edit Image 1
> into a single intermediate raised-paw greeting pose on the same 1024×1024
> canvas. Change only the arm on the viewer's right and the small belly surface
> revealed when that arm lifts. Keep the exact same purple-and-white panda
> identity, head silhouette, ears, eyes, nose, CLOSED-MOUTH smile, head
> position, opposite arm, torso silhouette, hips, feet, baseline, centered
> scale and lighting as Image 1. The moving arm must be anatomically connected
> continuously from shoulder to elbow to forearm to wrist and rise about
> halfway toward the cheek; its paw is turned outward enough to show a hint
> of pink pads, without an abrupt change in forearm thickness. No second
> character, no props, no captions, no grid, no duplicate paws, no shadow on
> the background. Output one full-body pose on a genuinely transparent
> background with clean straight-alpha edges; match Image 1's soft dimensional
> painted texture.

The retained result is [greet-raised-paw-fullbody.png](greet-raised-paw-fullbody.png),
SHA-256 `4c95bdec249c92ba1397df740dca1a9cec3eb2941926fc0bc920af1e14077d68`.

The second edit used that first output as edit target and the same accepted
neutral as supporting identity reference. Prompt:

> Use case: precise-object-edit. Asset type: companion transparent animation
> pose for Dif. Image 1 is the edit target: a generated raised-paw pose. Image
> 2 is the accepted neutral reference for this same purple-and-white panda.
> Change ONLY the raised arm on the viewer's right in Image 1: lower the
> entire anatomically connected shoulder-elbow-forearm-paw halfway toward
> the resting position, roughly 45 degrees above rest, with the paw near
> upper chest height and only a small hint of its pink pads. Keep the same
> arm thickness, rounded shoulder, purple material, wrist and paw scale as
> Image 1; the forearm must grow continuously from shoulder through elbow,
> not appear as a separate blob. Keep the exact current head, CLOSED-MOUTH
> smile, opposite arm, torso, hips, feet and baseline unchanged. Single
> full-body pose, same canvas and body scale as Image 1, genuinely transparent
> background; no duplicate paw, props, text, grid or background shadow. This
> is an intermediate animation frame that will be composited locally with the
> exact neutral.

The retained result is [greet-lower-paw-fullbody.png](greet-lower-paw-fullbody.png),
SHA-256 `691f021099c25aa415f8b2494c141c6ac706c2d2a49ff1d1bedb62a49bc1d944`.
Both tool outputs are RGBA 1254×1254 despite the first prompt's requested
1024-square canvas; neither is a native 1024 source or an accepted image.
Whole-body scale, face and body pixels visibly differ from the accepted
neutral. Any derived study must retain exact accepted neutral endpoints,
explain registration/extraction, and test head, opposite limb, torso and feet
locks; neither full generated image may replace the approved master.
