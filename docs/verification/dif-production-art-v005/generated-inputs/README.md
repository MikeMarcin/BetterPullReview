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
