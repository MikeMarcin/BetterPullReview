# RFC-BPR-0031 Home integration evidence

This record binds the authored `home-integration` milestone (BPR-0761/0762) to
run `dif-takeover-20260914`, dispatch `d12`, producer
`rfc-integrator-d12`, and mandate revision 7. The selected plan is
`bpr-dif-integration-v1`; its definition, obligation, and prerequisite
fingerprints are respectively
`sha256:b445184d9a0bf38579cd81ae950aeb5a4b88e6c558d0233ffd1a73ab9169f959`,
`sha256:44db6def452ef79ae90a252abcd61600d82f88513723fa6247728f9ec39df9f8`,
and
`sha256:c4a997783c151380076a58bd9a7e6b432417b18369788347d54edc80d6c4c241`.

## BPR-0761 source result

Revision `4bbecb48950cdd3b10eaea06699df4c5c464c804` selects the exact accepted
schema-2 production catalog through Qt resources in normal tests-enabled and
tests-disabled applications. A desktop owns one lazy provider shared by every
profile and window. It waits for a resolved, shown Home with enough layout
space before decoding, latches refusal with one diagnostic, and exposes an
art-free Home if validation fails. Decoding checks dimensions, both full-cell
transparent end gutters, the straight-RGBA digest, the 24 MiB pixel ceiling,
the 32 MiB retained ceiling, and the 40 MiB conversion peak.

The existing Home landing owns the decorative slot. At normal width it presents
a 96-pixel Dif, compacts to 72 pixels, and hides before the existing input is
compressed. Dif has no focus or pointer path. The current Appearance host's
visibility and resolved motion reach each workbench: full motion greets once,
returns to the declared calm loop, blinks after four complete loops, and drops
expired blink debt; reduced motion displays static rest with no deadline.
Covering an open review with Home still uses the existing Home transition.

The selected-build verifier passed after a clean rebuild of the revision:

- `dev`: tests enabled, 395 first-party translation units, all ten accepted
  compiled strips, application SHA-256
  `8082da2d8e1dd2afd97dba82ee4d35969815d6c6c12832cee3776548985c2504`.
- `app-release`: tests disabled, 200 first-party translation units, all ten
  accepted compiled strips, application SHA-256
  `fb3fc6725708bf202983b448d13a362a995915de74869b77414dbebad7e9296c`.
- `core-only`: Qt disabled, 210 first-party translation units, no Dif bytes;
  four core artifacts had combined SHA-256
  `e40d47916e49adae21ba84cc54212811d85ba4091cf5f245e00bb44388e7d9ea`.

The production runtime test measured 23,855,104 decoded pixel bytes,
23,872,384 accounted retained bytes, and a 26,196,828-byte conversion peak.
The selected asset tree retained manifest SHA-256
`c9f3f38e303caea3bb2c1b28edf947a717c2c1e5f484945a1d6814d8aee7f40d`
and provenance SHA-256
`f77354d6e55fd8631b5741d5a055c1dcb42a52e3a5ebf515d98b6ad1fed76f1c`.

## Integrated Home and profile course

The combined result at revision
`b299818e36887bdf3d4e8be138ed08095ca3e662` ran this network-isolated CTest
selection after the independent-review corrections:

```text
qt_dif_runtime
qt_workbench_window_scale_100
qt_workbench_window_scale_125
qt_workbench_window_scale_200
application_profile_management_public_path
application_profile_multi_window_public_path
dif_assets_contract
```

All seven passed in 115.20 seconds. The workbench course samples actual
production greeting and breathing frames and requires unequal rendered images,
waits through four declared calm loops to the real blink, then spans 350 ms in
reduced motion without a timer wakeup. It also proves compact-first Home does
not decode hidden art, expansion performs one decode, all three shipping scale
factors render, and the existing input keeps focus when the art hides.

The profile courses use the real Settings controls. They prove durable motion
updates, same-profile window fan-out, cross-profile isolation and restart,
one cache identity and one ten-strip decode across three profiles, and no timer
wakeup in an inactive sibling. The multi-window course also opens independent
reviews, covers one with Home, and returns with comparison identity, selected
file, epoch, scroll position, and focus unchanged.

The project-wide 658-file format check and `git diff --check` passed. An
independent `rfc-reviewer` found eager construction decode and active
`WindowStateChange` handling defects. Both were corrected and covered by
negative assertions; the reviewer then reported no remaining blocking or
nonblocking finding. The complete seven-test selection above ran after both
corrections.

## Limits retained for later milestones

This milestone reuses the accepted production-art human verdict. It does not
claim a new native visual decision, an actual operating-system motion-setting
change, signed/notarized packaging, or native-backend memory measurement.
Contextual gesture routing remains assigned to `context-integration`.

BPR-0761 was accepted at
`cc9c97d68a59982d602f6fa85539f1c318d71968`; BPR-0762 was accepted at
`dbbaafc14c3e3ffab74b41ab9ad6e3a8485eb414`. Their card records retain the
criterion-level source revisions and evidence above.
