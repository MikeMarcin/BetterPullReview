# Dif d7 generated pose inputs

Run `dif-takeover-20260914`, dispatch `d7`, producer `rfc-integrator-d4`,
mandate revision 5. The user requested detailed prompts and had already
authorized scoped offline image editing. These are unaccepted native
candidate inputs, not normalized production source or a human verdict.

## G1 — partly opening greeting bridge

The built-in `image_gen.imagegen` edit tool received exactly the fenced **G1**
prompt in [PROMPTS.md](../PROMPTS.md), extracted verbatim for the call.
Reference roles and exact bytes:

1. Edit target: accepted prop-free neutral
   `docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png`,
   SHA-256 `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
2. Supporting connected-arm reference:
   `docs/verification/dif-production-art-v004/generated-correction/open-paw-intermediate-fullbody.png`,
   SHA-256 `71aed58a2c88fec60e76ca00d314c55cfe65c1dd539ebd42e47b010930fd347f`.
3. Supporting motion-intent reference: immutable incoming `dif-greet.png`,
   SHA-256 `1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58`.

The tool returned [greet-partly-open-bridge.png](greet-partly-open-bridge.png),
SHA-256 `36c5871091ddccfee8cb1297da15dedf659051b6a05eebf4ed4bc7e351f6012f`,
native 1254×1254 RGBA. The exact copied output was saved by the built-in tool
under `/Users/jaeger/.codex/generated_images/01a0a1a2-a470-79a1-bd79-a848c0614c16/exec-83f0915b-62f0-4021-858a-bc2fe5081d6c.png`.
No CLI/API key, model ID, seed or prompt replay claim is inferred. Inspection
shows a partly open padded paw and connected forearm, but the generated
full-body head, feet and scale differ from the accepted neutral; those pixels
must not enter a production frame. Its arm is undergoing a separate local
extraction/locked-region study. Ordinary builds and runtime contain no
generation.

## G2 — shoulder-fixed open-paw peak

The built-in `image_gen.imagegen` edit tool received exactly the fenced **G2**
prompt in [PROMPTS.md](../PROMPTS.md), extracted verbatim for the call.
Reference roles and exact bytes:

1. Edit target: the same accepted neutral, SHA-256
   `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
2. Supporting transition reference: the G1 generated input above, SHA-256
   `36c5871091ddccfee8cb1297da15dedf659051b6a05eebf4ed4bc7e351f6012f`.
3. Supporting pad-shape reference: the v004 corrected open-paw input above,
   SHA-256 `71aed58a2c88fec60e76ca00d314c55cfe65c1dd539ebd42e47b010930fd347f`.

The tool returned
[greet-shoulder-fixed-open-peak.png](greet-shoulder-fixed-open-peak.png),
SHA-256 `e64f4de396ccfa88b0fd1002ef3855a93cdd0424c259284ea70e7efdef7966c0`,
native 1254×1254 RGBA. Its original built-in-tool output is
`/Users/jaeger/.codex/generated_images/01a0a1a2-a470-79a1-bd79-a848c0614c16/exec-e4f6c673-6e40-4f7e-b266-a06af0ebfc35.png`.
Visual inspection shows a raised connected arm and pink pads, but the returned
full body still differs from the exact accepted neutral. This is only an
unaccepted local-arm source candidate under objective extraction study.

## B1 — clean closed blink lids

The built-in `image_gen.imagegen` edit tool received exactly the fenced **B1**
prompt in [PROMPTS.md](../PROMPTS.md), extracted verbatim for the call.
Reference roles and exact bytes:

1. Edit target: the same accepted neutral, SHA-256
   `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
2. Supporting closed-lid action reference: the provisional v004
   `docs/verification/dif-production-art-v004/extended/blink-repair-eye-study.png`,
   SHA-256 `32d4fc719318f0a9f0bb5f2a3cae3e28883fe9d02f5e1d3e6ff1a19581498c24`;
   its third panel (index 2) alone supplied action intent, not texture quality.

The tool returned [blink-closed-lids.png](blink-closed-lids.png), SHA-256
`fb6764b7ab9df2c087ca909b5e2c748a70245591c17cb2cbe81e2fabd516174e`,
native 1254×1254 RGBA. Its original built-in-tool output is
`/Users/jaeger/.codex/generated_images/01a0a1a2-a470-79a1-bd79-a848c0614c16/exec-8984944c-fc62-4427-9d76-ab55c42612f3.png`.
Visual inspection shows closed curves, but the generated face/body are not
pixel-equal to the accepted neutral. Only a strictly local eye extraction can
qualify; the current blink study tests that boundary and texture at shipping
sizes.

## E1–E6 — first extended-gesture edited inputs

Each row below is a separate built-in `image_gen.imagegen` edit call. The
prompt is exactly the fenced **E1**, **E2**, **E3**, **E4**, **E5** or **E6**
block in [PROMPTS.md](../PROMPTS.md), copied verbatim. Reference image 1 was
the accepted neutral (SHA-256
`bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`);
reference image 2 was the matching immutable incoming sheet. E4's optional
third reference was not supplied. The sheet hashes are in PROMPTS.md's
source table. The last column records the exact original built-in output
filename under
`/Users/jaeger/.codex/generated_images/01a0a1a2-a470-79a1-bd79-a848c0614c16/`;
the linked project copy has the recorded digest. Every output is native
1254×1254 RGBA and is **unaccepted**.

| Prompt / intended pose | Retained project copy and SHA-256 | Original generated filename |
|---|---|---|
| E1 compare viewer-left | [compare-glance-left.png](compare-glance-left.png) · `eeea4f02d3629ec16645b64a5e06b9e2223662abf7ea5a00237e5cf23f0613bd` | `exec-12f4e29d-78c3-47c8-b9d9-b482f5263b1b.png` |
| E2 consider half-lift | [consider-half-lift.png](consider-half-lift.png) · `f591e00aa37ad5db86c67bfea508ce9a1d7c24afa03d66a03382a30d706d380b` | `exec-c780da59-06ac-4c7a-9f71-3fd77f8e9b18.png` |
| E3 comment-ready open peak | [comment-ready-open-peak.png](comment-ready-open-peak.png) · `27a98932a6aec3ba0fc821d9e91064838747fbdf950bb8ccec55683cebb957ad` | `exec-ebe59b5e-817e-4de2-adf5-e4d19bf5989b.png` |
| E4 attention chest paw | [attention-chest-paw.png](attention-chest-paw.png) · `05fda8c8844e558a3ca0e951c6676c59505f5328bc84bd8f3f5c16861adc8809` | `exec-ac2243ee-6f67-4336-9aa5-ef1fbb3a9bc2.png` |
| E5 acknowledge nod | [acknowledge-nod-peak.png](acknowledge-nod-peak.png) · `4a7a8198c3e7bc2ecf566cb30d91992e3ff193e4158dc55bc80dca6df271c3de` | `exec-484455cf-f894-48ad-8a1b-a82cade281f0.png` |
| E6 blocked correct-side paw | [blocked-left-paw.png](blocked-left-paw.png) · `58296fe7aa833d09501887d9eba8211031058a10339ec1f24fc6249ca7b1a5ad` | `exec-1d2b0bc6-b1d0-4a12-87f6-01d637f73b7d.png` |

Initial full-output inspection shows that E2–E6 express their requested
actions, but all shift face/body registration from the accepted neutral. E1
visibly changes only one pupil, not the requested conjugate pair. The
extended study independently tests whether any bounded local material is
reusable. No peak by itself establishes a complete, timed clip, and none
may replace the neutral or enter the production catalog without local
extraction, objective/visual checks and the final human art decision.
