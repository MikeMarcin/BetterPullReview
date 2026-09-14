# Dif d9 external pose correction prompts

Use the copyable base prompts in
[`../dif-production-art-v005/PROMPTS.md`](../dif-production-art-v005/PROMPTS.md)
with its exact accepted-neutral edit target and immutable action-only sheets.
This addendum records what the d8 studies taught. Every output remains an
unaccepted candidate until local locked-region extraction, complete timed
sequence inspection and a bound human art/derivative verdict.

## Greeting: repair the shared root in two poses

The d8 partly-open candidate
`../dif-production-art-v006/greet/greet-partly-open-rounded-input.png` still
has a near-vertical inner shoulder cut. In the final normalized study, 85
root pixels visible in G1 disappear in G2 within runtime rectangle
`[168,145,187,166)` although visible limb area changes only 6.3%.
`../dif-production-art-v006/greet/shoulder-root-comparison.png` is a **negative
diagnostic**, not anatomy to copy. The d8 candidate and v005 G1/G2 donors
are pose/material guidance only; each has full-body identity drift.

**G1 replacement — partly open bridge.** Supply images in this order:
accepted neutral, v005 `greet-partly-open-bridge.png`, v005
`greet-shoulder-fixed-open-peak.png`, and optionally the d8 root comparison.
Copy this prompt for one image-edit call:

```text
Use case: precise-object-edit. Output one original transparent full-body Dif panda pose, not a sheet. Image 1 is the exact accepted prop-free neutral and controls identity, canvas, body registration, head/chin, opposite arm, hips and feet. Image 2 is a rejected partly-open greeting bridge for hand articulation only. Image 3 is a rejected open-paw peak for the common arm volume only. If supplied, Image 4 is a negative shoulder-root comparison: its straight inner cut and vanishing wedge must be repaired, not copied.
Create the viewer-right raised paw halfway between a curled lift and a small open wave beside the lower cheek. Keep two or three subtle pink finger-pad hints and a partially exposed palm, distinctly less open than the peak. Author one thick, softly rounded purple shoulder attachment under the same cheek point, a continuous upper arm and bent elbow, and a forearm whose width, texture, lighting and belly occlusion are compatible with the open peak. The inner silhouette must curve naturally into the torso; no vertical cut, missing root wedge, sharp dark seam, floating paw, duplicate resting paw or abrupt forearm taper. Preserve the accepted neutral head silhouette and translucent chin edge; put the arm behind the head without touching facial pixels.
Move only this arm and the small exposed belly/shoulder join. Keep Image 1's closed smile, eyes, ears, head scale, opposite arm, torso breadth, hips and both planted feet at the same positions. One subject on genuine transparent RGBA, ideally on the original 1024-square canvas. No prop, badge, text, code, shadow, halo, ground, background or camera shift. Return one pose; an external full-body result will still be locally extracted onto exact neutral pixels.
```

**G2 replacement — fully open peak.** Use the exact accepted neutral as image
1. Use the newly created G1 as image 2 **only after** its root and anatomy
pass static inspection; use the v005 open-paw donor as image 3 for pad shape
only. If G1 fails, repair it before making G2.

```text
Use case: precise-object-edit. Output one original transparent full-body Dif greeting peak pose, not a sheet. Image 1 is the exact accepted prop-free neutral and fixes identity, head/chin, body registration, opposite arm, hips and planted feet. Image 2 is the inspected partly-open bridge and defines the same rounded shoulder root, bent elbow, forearm breadth, belly occlusion, material and light. Image 3 is a rejected open-paw donor for small pink pad shapes only; do not copy its full body or shoulder cut.
Continue exactly the same viewer-right upper arm from Image 2 to a restrained fully open paw beside the lower cheek, below the ear and inside the head-width envelope. Keep the inner root curved and present across both poses; the bridge-to-peak transition must not erase a wedge beneath the cheek. Open the same paw toward the viewer with three small finger pads and one palm pad, then rotate the wrist slightly outward. Preserve continuous forearm thickness and attached plush elbow. Do not translate, scale, detach or duplicate the arm to reach the peak.
Change only the active arm and the minimal exposed belly/shoulder join. Preserve Image 1's exact awake face, translucent head/chin edge, closed smile, body breadth, opposite arm, hips, feet, camera and baseline. Genuine transparent RGBA, preferably the original 1024-square canvas. No other mascot, prop, text, symbol, code, cast/contact shadow, halo, background or whole-body squash. Return one pose; local exact-pixel locks and timed transition review remain required.
```

The same shoulder/elbow contour must survive the small wrist return and
lowering poses. Author distinct curled lift, partly open, open, wrist turn,
lowering and nearly neutral states. Copy accepted neutral for frames 0 and 7.
Check the first lift and final handoff at actual 125-ms timing; a root-area
number alone is not smooth motion.

## Extended-gesture addenda to the v005 prompts

Use each v005 E1–E6 block with the following latest negative findings. Ask
for separate entry, peak and return art at the frame indices in the clip
contract; no isolated donor is a complete clip.

| Prompt | Extra instruction from d8 |
|---|---|
| E1 glance | Author both pupils at matching travel within the two original sockets. Reject doubled highlights and the pale wedge from the donor-eye graft; make separate left, center and right states. |
| E2 consider | Draw the genuinely lower curled half-lift below the chin first, with connected shoulder/elbow and unobstructed face. The earlier donor paw was already too high; a head mask cannot lower it. |
| E3 comment ready | Preserve the useful two open paws but reconstruct exposed belly to the accepted torso breadth and refine both shoulder joins; add a distinct half-lift and closing articulation. |
| E4 attention | Keep the raised paw at chest height with a complete rounded forearm and elbow. The previous donor was too high and truncating it at the chest created a cut hand and belly crease. |
| E5 acknowledge | Incline the original-scale head locally with a continuous neck join. The smaller/lower donor left old ear fragments and did not prove a nod. Keep both eyes awake. |
| E6 blocked | Use the correct viewer-left open paw as material guidance, then add the required viewer-left patient head tilt and partly opening/closing states. Refine wrist and belly shading. |

The v006 blink candidate is a useful eye-local material and six-frame timing
reference, still unaccepted. Keep exact neutral pixels outside the two eye
interiors, reuse exact neutral at frames 0, 4 and 5, and inspect native texture
as well as true-size 100-ms playback. `calm_idle` and `rest` derive from the
accepted neutral and contract; do not ask a model to redesign them.
